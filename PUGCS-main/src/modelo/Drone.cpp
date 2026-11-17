#include "modelo/Drone.hpp"

#include <mavsdk/log_callback.hpp>
#include <mavsdk/mavsdk.hpp>
#include <mavsdk/plugins/action/action.hpp>
#include <mavsdk/plugins/telemetry/telemetry.hpp>
#include <mavsdk/plugins/mavlink_direct/mavlink_direct.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace gcs {
namespace {

std::string para_maiusculas(std::string texto) {
    std::transform(texto.begin(), texto.end(), texto.begin(), [](unsigned char letra) {
        return static_cast<char>(std::toupper(letra));
    });
    return texto;
}

/// Modos de voo do ArduCopter e o número (custom_mode) de cada um.
///
/// Uma tabela só serve para os dois sentidos: trocar de modo (nome → número)
/// e mostrar o modo atual (número → nome). O MAVSDK traduz os modos para os
/// nomes do PX4 ("Offboard" no lugar de GUIDED, "Hold" no lugar de BRAKE),
/// por isso lemos o número direto do HEARTBEAT.
const std::map<std::string, int>& modos_arducopter() {
    static const std::map<std::string, int> modos{
        {"STABILIZE", 0}, {"ACRO", 1},       {"ALT_HOLD", 2},   {"AUTO", 3},
        {"GUIDED", 4},    {"LOITER", 5},     {"RTL", 6},        {"CIRCLE", 7},
        {"LAND", 9},      {"DRIFT", 11},     {"SPORT", 13},     {"FLIP", 14},
        {"AUTOTUNE", 15}, {"POSHOLD", 16},   {"BRAKE", 17},     {"THROW", 18},
        {"SMART_RTL", 21}, {"FOLLOW", 23},   {"ZIGZAG", 24},    {"AUTO_RTL", 27},
    };
    return modos;
}

/// Nome do modo a partir do número; "MODO_<n>" se não estiver na tabela.
std::string nome_do_modo(long numero) {
    for (const auto& [nome, valor] : modos_arducopter()) {
        if (valor == numero) {
            return nome;
        }
    }
    return "MODO_" + std::to_string(numero);
}

/// Lê o campo custom_mode do JSON de um HEARTBEAT; -1 se não achar.
long ler_custom_mode(const std::string& json) {
    static const std::regex padrao{R"re("custom_mode"\s*:\s*(\d+))re"};
    std::smatch achado;
    if (std::regex_search(json, achado, padrao)) {
        return std::stol(achado[1].str());
    }
    return -1;
}

/// O MAVSDK imprime os enums com operator<<; é a forma mais simples de
/// transformar um FlightMode em texto.
template <typename T>
std::string para_texto(const T& valor) {
    std::ostringstream fluxo;
    fluxo << valor;
    return fluxo.str();
}

/// Troca o modo do ArduPilot pelo número dele, enviando um COMMAND_LONG com
/// MAV_CMD_DO_SET_MODE (176). Usado para os modos que o plugin Action não
/// cobre (GUIDED, AUTO...).
void enviar_modo_ardupilot(mavsdk::MavlinkDirect& mavlink, const mavsdk::System& sistema,
                           int numero, const std::string& nome) {
    const mavsdk::Mavsdk::Configuration gcs{mavsdk::ComponentType::GroundStation};
    const int alvo_sistema = sistema.get_system_id();
    const int alvo_componente = 1;  // o autopiloto

    mavsdk::MavlinkDirect::MavlinkMessage mensagem{};
    mensagem.message_name = "COMMAND_LONG";
    mensagem.system_id = gcs.get_system_id();
    mensagem.component_id = gcs.get_component_id();
    mensagem.target_system_id = alvo_sistema;
    mensagem.target_component_id = alvo_componente;
    mensagem.fields_json =
        "{\"target_system\": " + std::to_string(alvo_sistema) +
        ", \"target_component\": " + std::to_string(alvo_componente) +
        ", \"command\": 176, \"confirmation\": 0"
        ", \"param1\": 1.0, \"param2\": " + std::to_string(numero) + ".0"
        ", \"param3\": 0.0, \"param4\": 0.0, \"param5\": 0.0"
        ", \"param6\": 0.0, \"param7\": 0.0}";

    const auto resultado = mavlink.send_message(mensagem);
    if (resultado != mavsdk::MavlinkDirect::Result::Success) {
        throw ErroDeVeiculo("o autopiloto recusou o modo " + nome + ": " + para_texto(resultado));
    }
}

}  // namespace

std::string normalizar_endereco(const std::string& endereco) {
    if (endereco.find("://") != std::string::npos) {
        return endereco;  // já está no formato de URL
    }
    const std::size_t separador = endereco.find(':');
    if (separador == std::string::npos) {
        return endereco;
    }
    std::string esquema = endereco.substr(0, separador);
    const std::string resto = endereco.substr(separador + 1);
    // "tcp" é ambíguo para o MAVSDK; para falar com o SITL queremos "tcpout".
    if (esquema == "tcp") {
        esquema = "tcpout";
    }
    return esquema + "://" + resto;
}

/// Tudo que depende do MAVSDK vive aqui dentro.
class Drone::Impl {
public:
    Impl()
        : nucleo(mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation}) {
        // O MAVSDK escreve muitos logs de depuração no terminal, no meio do
        // prompt. Deixamos passar só os erros dele; os avisos do autopiloto
        // que interessam ao piloto chegam pelo STATUSTEXT (iniciar_telemetria).
        mavsdk::log::subscribe([](mavsdk::log::Level nivel, const std::string&,
                                  const std::string&, int) {
            return nivel < mavsdk::log::Level::Err;  // true = "já tratei, não imprima"
        });
    }

    mavsdk::Mavsdk nucleo;
    std::shared_ptr<mavsdk::System> sistema;
    std::unique_ptr<mavsdk::Telemetry> telemetria;
    std::unique_ptr<mavsdk::Action> acoes;
    std::unique_ptr<mavsdk::MavlinkDirect> mavlink;
};

Drone::Drone(std::string nome, std::string endereco)
    : Veiculo(std::move(nome)),
      impl_(std::make_unique<Impl>()),
      endereco_(std::move(endereco)) {}

Drone::~Drone() = default;  // precisa estar no .cpp por causa do pimpl

void Drone::conectar() {
    const std::string url = normalizar_endereco(endereco_);
    std::cout << "[" << nome() << "] conectando em " << url << " ..." << std::endl;

    const mavsdk::ConnectionResult resultado = impl_->nucleo.add_any_connection(url);
    if (resultado != mavsdk::ConnectionResult::Success) {
        throw ErroDeVeiculo("não foi possível abrir " + url + ": " + para_texto(resultado));
    }

    // O primeiro autopiloto que aparecer na rede MAVLink é o nosso drone.
    const auto sistema = impl_->nucleo.first_autopilot(TEMPO_LIMITE_S);
    if (!sistema) {
        throw ErroDeVeiculo(
            "nenhum autopiloto respondeu em " + url + ". O SITL do ArduPilot está rodando?");
    }

    impl_->sistema = sistema.value();
    impl_->telemetria = std::make_unique<mavsdk::Telemetry>(impl_->sistema);
    impl_->acoes = std::make_unique<mavsdk::Action>(impl_->sistema);
    impl_->mavlink = std::make_unique<mavsdk::MavlinkDirect>(impl_->sistema);
    std::cout << "[" << nome() << "] conectado." << std::endl;
}

void Drone::iniciar_telemetria() {
    if (!impl_->telemetria) {
        throw ErroDeVeiculo("iniciar_telemetria() antes de conectar()");
    }
    mavsdk::Telemetry& telemetria = *impl_->telemetria;

    // O ArduPilot só envia a telemetria que a GCS pede (normalmente quem pede é
    // o MAVProxy). Pedimos aqui a taxa de cada dado, em Hz. Se o autopiloto
    // recusar alguma, avisamos e seguimos sem ela.
    const std::vector<std::pair<std::string, mavsdk::Telemetry::Result>> taxas{
        {"atitude", telemetria.set_rate_attitude_euler(10.0)},
        {"posição", telemetria.set_rate_position(5.0)},
        {"velocidade", telemetria.set_rate_velocity_ned(5.0)},
        {"bateria", telemetria.set_rate_battery(1.0)},
        {"gps", telemetria.set_rate_gps_info(1.0)},
        {"home", telemetria.set_rate_home(1.0)},
        {"em voo", telemetria.set_rate_in_air(1.0)},
        {"rádio", telemetria.set_rate_rc_status(1.0)},
    };
    for (const auto& [dado, resultado] : taxas) {
        if (resultado != mavsdk::Telemetry::Result::Success) {
            std::cerr << "[" << nome() << "] aviso: taxa de " << dado
                      << " recusada: " << para_texto(resultado) << std::endl;
        }
    }

    // Cada assinatura é um callback chamado pela thread do MAVSDK. Tudo que
    // fazemos é traduzir o dado da biblioteca para o NOSSO modelo — assim o
    // resto do programa não depende do MAVSDK.
    // Modo de voo: lido do HEARTBEAT do autopiloto, com os nomes do ArduPilot.
    const auto id_do_drone = impl_->sistema->get_system_id();
    impl_->mavlink->subscribe_message(
        "HEARTBEAT", [this, id_do_drone](mavsdk::MavlinkDirect::MavlinkMessage mensagem) {
            constexpr unsigned COMPONENTE_AUTOPILOTO = 1;
            if (mensagem.system_id != id_do_drone ||
                mensagem.component_id != COMPONENTE_AUTOPILOTO) {
                return;  // heartbeat de outra GCS, câmera, gimbal...
            }
            const long numero = ler_custom_mode(mensagem.fields_json);
            if (numero < 0) {
                return;
            }
            const std::string nome = nome_do_modo(numero);
            atualizar([&nome](EstadoVeiculo& estado) { estado.modo_de_voo = nome; });
        });

    // Avisos do autopiloto (PreArm, falhas, failsafe): mostramos só os de
    // severidade "warning" para cima, numa linha própria.
    telemetria.subscribe_status_text([this](mavsdk::Telemetry::StatusText aviso) {
        if (aviso.type >= mavsdk::Telemetry::StatusTextType::Warning) {
            std::cout << "\n[" << nome() << "] " << aviso.text << std::endl;
        }
    });

    telemetria.subscribe_attitude_euler([this](mavsdk::Telemetry::EulerAngle angulo) {
        atualizar([&angulo](EstadoVeiculo& estado) {
            estado.atitude.rolagem = angulo.roll_deg;
            estado.atitude.arfagem = angulo.pitch_deg;
            estado.atitude.guinada = angulo.yaw_deg;
        });
    });

    telemetria.subscribe_position([this](mavsdk::Telemetry::Position posicao) {
        atualizar([&posicao](EstadoVeiculo& estado) {
            estado.posicao.latitude = posicao.latitude_deg;
            estado.posicao.longitude = posicao.longitude_deg;
            estado.posicao.altitude_absoluta_m = posicao.absolute_altitude_m;
            estado.posicao.altitude_relativa_m = posicao.relative_altitude_m;
        });
    });

    telemetria.subscribe_battery([this](mavsdk::Telemetry::Battery bateria) {
        atualizar([&bateria](EstadoVeiculo& estado) {
            estado.bateria.tensao_v = bateria.voltage_v;
            // No MAVSDK 4.x remaining_percent já vem de 0 a 100 (nas versões
            // 1.x vinha de 0 a 1 — cuidado ao trocar de versão).
            estado.bateria.percentual = bateria.remaining_percent;
        });
    });

    telemetria.subscribe_armed([this](bool armado) {
        atualizar([armado](EstadoVeiculo& estado) { estado.armado = armado; });
    });

    telemetria.subscribe_in_air([this](bool em_voo) {
        atualizar([em_voo](EstadoVeiculo& estado) { estado.em_voo = em_voo; });
    });

    telemetria.subscribe_velocity_ned([this](mavsdk::Telemetry::VelocityNed velocidade) {
        atualizar([&velocidade](EstadoVeiculo& estado) {
            estado.velocidade.norte_m_s = velocidade.north_m_s;
            estado.velocidade.leste_m_s = velocidade.east_m_s;
            estado.velocidade.baixo_m_s = velocidade.down_m_s;
        });
    });

    telemetria.subscribe_gps_info([this](mavsdk::Telemetry::GpsInfo gps) {
        atualizar([&gps](EstadoVeiculo& estado) { estado.gps.satelites = gps.num_satellites; });
    });

    telemetria.subscribe_home([this](mavsdk::Telemetry::HomePosition home) {
        atualizar([&home](EstadoVeiculo& estado) {
            estado.home.latitude = home.latitude_deg;
            estado.home.longitude = home.longitude_deg;
            estado.home.altitude_absoluta_m = home.absolute_altitude_m;
            estado.home.altitude_relativa_m = home.relative_altitude_m;
        });
    });

    telemetria.subscribe_rc_status([this](mavsdk::Telemetry::RcStatus radio) {
        atualizar([&radio](EstadoVeiculo& estado) {
            estado.radio.disponivel = radio.is_available;
            estado.radio.sinal_percentual = std::isnan(radio.signal_strength_percent) ? 0.0 : radio.signal_strength_percent;
        });
    });
}

void Drone::alterar_modo_de_voo(const std::string& modo) {
    if (!impl_->mavlink) {
        throw ErroDeVeiculo("alterar_modo_de_voo() antes de conectar()");
    }
    const std::string alvo = para_maiusculas(modo);

    const auto encontrado = modos_arducopter().find(alvo);
    if (encontrado == modos_arducopter().end()) {
        std::string disponiveis;
        for (const auto& par : modos_arducopter()) {
            disponiveis += (disponiveis.empty() ? "" : ", ") + par.first;
        }
        throw ErroDeVeiculo("modo '" + alvo + "' não existe no ArduCopter; disponíveis: " +
                            disponiveis);
    }
    enviar_modo_ardupilot(*impl_->mavlink, *impl_->sistema, encontrado->second, alvo);
}

void Drone::esperar_modo(const std::string& modo, double tempo_limite_s) const {
    const auto limite = std::chrono::steady_clock::now() +
                        std::chrono::duration<double>(tempo_limite_s);
    while (modo_de_voo() != modo) {
        if (std::chrono::steady_clock::now() > limite) {
            throw ErroDeVeiculo("o autopiloto não entrou em " + modo + " (modo atual: " +
                                modo_de_voo() + ")");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Comandos de voo: cada um delega para o plugin Action e converte o Result
// em exceção.
void Drone::armar() {
    const auto resultado = impl_->acoes->arm();
    if (resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("não foi possível armar o drone: " + para_texto(resultado));
    }
}

void Drone::desarmar() {
    const auto resultado = impl_->acoes->disarm();
    if (resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("não foi possível desarmar o drone: " + para_texto(resultado));
    }
}

void Drone::pousar() {
    const auto resultado = impl_->acoes->land();
    if (resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha no pouso: " + para_texto(resultado));
    }
}

void Drone::retornar_para_casa() {
    const auto resultado = impl_->acoes->return_to_launch();
    if (resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha ao retornar para casa: " + para_texto(resultado));
    }
}

void Drone::decolar(double altura_m) {
    if (estado().em_voo) {
        throw ErroDeVeiculo("o drone já está em voo");
    }
    // No ArduCopter a decolagem por comando só funciona em GUIDED, e alguns
    // modos (BRAKE, LAND, RTL...) nem deixam armar. Por isso trocamos de modo
    // antes e esperamos o autopiloto confirmar pelo HEARTBEAT.
    if (modo_de_voo() != "GUIDED") {
        alterar_modo_de_voo("GUIDED");
        esperar_modo("GUIDED", 3.0);
    }
    armar();

    const auto altura = impl_->acoes->set_takeoff_altitude(static_cast<float>(altura_m));
    if (altura != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("altura de decolagem recusada: " + para_texto(altura));
    }
    const auto resultado = impl_->acoes->takeoff();
    if (resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha na decolagem: " + para_texto(resultado));
    }
}

void Drone::ir_para(const Posicao& destino) {
    if (!impl_->acoes) {
        throw ErroDeVeiculo("ir_para() antes de conectar()");
    }
    if (!estado().em_voo) {
        throw ErroDeVeiculo("o drone precisa estar em voo para ir a uma posição");
    }
    // No ArduCopter, ir a um ponto por comando também exige o modo GUIDED.
    if (modo_de_voo() != "GUIDED") {
        alterar_modo_de_voo("GUIDED");
        esperar_modo("GUIDED", 3.0);
    }
    const auto resultado = impl_->acoes->goto_location(
        destino.latitude, destino.longitude, static_cast<float>(destino.altitude_absoluta_m),
        0.0f);
    if (resultado != mavsdk::Action::Result::Success) {
        throw ErroDeVeiculo("falha ao ir para a posição pedida: " + para_texto(resultado));
    }
}

}  // namespace gcs
