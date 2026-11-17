#include "modelo/Veiculo.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace gcs {

Veiculo::Veiculo(std::string nome) : nome_(std::move(nome)) {}

EstadoVeiculo Veiculo::estado() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estado_;  // cópia
}

std::string Veiculo::modo_de_voo() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estado_.modo_de_voo;
}

void Veiculo::registrar_observador(Observador observador) {
    std::lock_guard<std::mutex> trava(mutex_);
    observadores_.push_back(std::move(observador));
}

void Veiculo::atualizar(const std::function<void(EstadoVeiculo&)>& modificacao) {
    EstadoVeiculo copia;
    std::vector<Observador> observadores;
    {
        std::lock_guard<std::mutex> trava(mutex_);
        modificacao(estado_);
        estado_.instante = std::chrono::steady_clock::now();
        copia = estado_;
        observadores = observadores_;
    }
    // Os observadores são chamados FORA da trava: se um deles pedir estado()
    // de volta, não travamos o programa.
    for (const Observador& observador : observadores) {
        observador(copia);
    }
}

std::ostream& operator<<(std::ostream& fluxo, const Veiculo& veiculo) {
    return fluxo << veiculo.nome() << ": " << veiculo.estado().resumo();
}

// ---------------------------------------------------------------------------
// VeiculoSimulado
// ---------------------------------------------------------------------------
namespace {

// Parâmetros da simulação.
constexpr double LATITUDE_CASA = -35.363261;    // mesma casa do SITL
constexpr double LONGITUDE_CASA = 149.165230;
constexpr double ALTITUDE_CASA_M = 584.0;
constexpr double SUBIDA_MAXIMA_M_S = 2.5;
constexpr double DESCIDA_POUSO_M_S = 1.0;
constexpr double VELOCIDADE_RTL_M_S = 5.0;
constexpr double ALTITUDE_MINIMA_RTL_M = 15.0;
constexpr double CONSUMO_PCT_POR_S = 0.1;       // bateria gasta com o drone armado
constexpr double METROS_POR_GRAU = 111320.0;

constexpr double PI = 3.14159265358979323846;

double limitar(double valor, double minimo, double maximo) {
    return std::max(minimo, std::min(maximo, valor));
}

std::string para_maiusculas(const std::string& texto) {
    std::string maiusculo;
    for (char caractere : texto) {
        maiusculo += static_cast<char>(std::toupper(static_cast<unsigned char>(caractere)));
    }
    return maiusculo;
}

}  // namespace

VeiculoSimulado::VeiculoSimulado(std::string nome) : Veiculo(std::move(nome)) {}

VeiculoSimulado::~VeiculoSimulado() {
    parar_ = true;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void VeiculoSimulado::conectar() {
    atualizar([](EstadoVeiculo& estado) {
        estado.modo_de_voo = "STABILIZE";
        estado.posicao.latitude = LATITUDE_CASA;
        estado.posicao.longitude = LONGITUDE_CASA;
        estado.posicao.altitude_absoluta_m = ALTITUDE_CASA_M;
        estado.home = estado.posicao;
        estado.bateria.percentual = 100.0;
        estado.bateria.tensao_v = 12.6;
        estado.gps.satelites = 12;
        estado.radio.disponivel = true;
        estado.radio.sinal_percentual = 100.0;
    });
}

void VeiculoSimulado::iniciar_telemetria() {
    thread_ = std::thread(&VeiculoSimulado::laco_de_telemetria, this);
}

void VeiculoSimulado::laco_de_telemetria() {
    constexpr double DT_S = 0.05;
    const Instante inicio = std::chrono::steady_clock::now();
    while (!parar_) {
        const double t = segundos_entre(inicio, std::chrono::steady_clock::now());
        {
            std::lock_guard<std::mutex> trava(mutex_simulacao_);
            passo(t, DT_S);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void VeiculoSimulado::passo(double t, double dt) {
    Manobra proxima = manobra_;
    const Manobra atual = manobra_;
    const double alvo = altitude_alvo_m_;

    atualizar([&](EstadoVeiculo& estado) {
        double subida = 0.0;  // m/s, positivo subindo
        double norte = 0.0;
        double leste = 0.0;
        double& altitude = estado.posicao.altitude_relativa_m;

        switch (atual) {
            case Manobra::Parado:
                break;
            case Manobra::Subindo:
            case Manobra::Pairando:
                subida = limitar(alvo - altitude, -SUBIDA_MAXIMA_M_S, SUBIDA_MAXIMA_M_S);
                if (atual == Manobra::Subindo && std::abs(alvo - altitude) < 0.1) {
                    proxima = Manobra::Pairando;
                }
                break;
            case Manobra::Pousando:
                subida = -std::max(0.3, std::min(DESCIDA_POUSO_M_S, altitude * 0.5));
                break;
            case Manobra::Retornando: {
                // Primeiro sobe até a altitude mínima de RTL, depois vai para casa.
                if (altitude < ALTITUDE_MINIMA_RTL_M - 0.1) {
                    subida = limitar(ALTITUDE_MINIMA_RTL_M - altitude, 0.0, SUBIDA_MAXIMA_M_S);
                    break;
                }
                const double cos_lat = std::cos(estado.posicao.latitude * PI / 180.0);
                const double dn = (estado.home.latitude - estado.posicao.latitude) * METROS_POR_GRAU;
                const double de =
                    (estado.home.longitude - estado.posicao.longitude) * METROS_POR_GRAU * cos_lat;
                const double distancia = std::hypot(dn, de);
                if (distancia < 0.5) {
                    proxima = Manobra::Pousando;
                    break;
                }
                const double velocidade = std::min(VELOCIDADE_RTL_M_S, distancia);
                norte = velocidade * dn / distancia;
                leste = velocidade * de / distancia;
                estado.posicao.latitude += norte * dt / METROS_POR_GRAU;
                estado.posicao.longitude += leste * dt / (METROS_POR_GRAU * cos_lat);
                estado.atitude.guinada = std::fmod(std::atan2(leste, norte) * 180.0 / PI + 360.0, 360.0);
                break;
            }
        }

        altitude = std::max(0.0, altitude + subida * dt);
        if (atual == Manobra::Pousando && altitude <= 0.0) {
            // Tocou o chão: o ArduPilot desarma sozinho depois do pouso.
            proxima = Manobra::Parado;
            estado.armado = false;
        }

        estado.em_voo = altitude > 0.2;
        estado.posicao.altitude_absoluta_m = estado.home.altitude_absoluta_m + altitude;
        estado.velocidade.norte_m_s = norte;
        estado.velocidade.leste_m_s = leste;
        estado.velocidade.baixo_m_s = -subida;

        // Atitude: um balanço leve no ar e o nariz abaixado quando anda para frente.
        if (estado.em_voo) {
            const double velocidade_solo = std::hypot(norte, leste);
            estado.atitude.rolagem = 2.0 * std::sin(t * 0.7);
            estado.atitude.arfagem = 1.5 * std::sin(t * 0.5) - 3.0 * velocidade_solo;
        } else {
            estado.atitude.rolagem = 0.0;
            estado.atitude.arfagem = 0.0;
        }

        if (estado.armado) {
            estado.bateria.percentual = std::max(0.0, estado.bateria.percentual - CONSUMO_PCT_POR_S * dt);
        }
        estado.bateria.tensao_v = 10.5 + 2.1 * estado.bateria.percentual / 100.0;
        estado.radio.sinal_percentual = 95.0 + 5.0 * std::sin(t / 7.0);
    });

    manobra_ = proxima;
}

void VeiculoSimulado::aplicar_modo(const std::string& modo) {
    const EstadoVeiculo agora = estado();
    if (modo == "LAND") {
        manobra_ = agora.em_voo ? Manobra::Pousando : Manobra::Parado;
    } else if (modo == "RTL") {
        manobra_ = agora.em_voo ? Manobra::Retornando : Manobra::Parado;
    } else if (agora.em_voo) {
        // Qualquer outro modo em voo: segura a altitude atual.
        manobra_ = Manobra::Pairando;
        altitude_alvo_m_ = agora.posicao.altitude_relativa_m;
    }
    atualizar([&modo](EstadoVeiculo& estado) { estado.modo_de_voo = modo; });
}

void VeiculoSimulado::alterar_modo_de_voo(const std::string& modo) {
    std::lock_guard<std::mutex> trava(mutex_simulacao_);
    aplicar_modo(para_maiusculas(modo));
}

void VeiculoSimulado::armar() {
    atualizar([](EstadoVeiculo& estado) { estado.armado = true; });
}

void VeiculoSimulado::desarmar() {
    if (estado().em_voo) {
        throw ErroDeVeiculo("não é possível desarmar em voo; use pousar");
    }
    std::lock_guard<std::mutex> trava(mutex_simulacao_);
    manobra_ = Manobra::Parado;
    atualizar([](EstadoVeiculo& estado) { estado.armado = false; });
}

void VeiculoSimulado::decolar(double altura_m) {
    if (estado().em_voo) {
        throw ErroDeVeiculo("o drone já está em voo");
    }
    std::lock_guard<std::mutex> trava(mutex_simulacao_);
    aplicar_modo("GUIDED");
    atualizar([](EstadoVeiculo& estado) { estado.armado = true; });
    altitude_alvo_m_ = altura_m;
    manobra_ = Manobra::Subindo;
}

void VeiculoSimulado::pousar() {
    std::lock_guard<std::mutex> trava(mutex_simulacao_);
    aplicar_modo("LAND");
}

void VeiculoSimulado::retornar_para_casa() {
    std::lock_guard<std::mutex> trava(mutex_simulacao_);
    aplicar_modo("RTL");
}

}  // namespace gcs
