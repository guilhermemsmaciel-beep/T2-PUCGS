#include "cli/ComandosPadrao.hpp"

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <memory>

#include "EstacaoDeControle.hpp"
#include "modelo/Veiculo.hpp"
#include "visual/GraficoTempo.hpp"
#include "visual/HorizonteArtificial.hpp"

namespace gcs {
namespace {

// ---------------------------------------------------------------------------
// Comandos de exemplo. Cada um é uma classe pequena; o console não sabe quais
// existem, só pede ao registro o comando com o nome digitado.
// ---------------------------------------------------------------------------

class Ajuda : public Comando {
public:
    std::string nome() const override { return "ajuda"; }
    std::string ajuda() const override { return "lista os comandos disponíveis"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        std::cout << "Comandos:\n";
        for (const Comando* comando : estacao.comandos().todos()) {
            std::cout << "  " << comando->linha_de_ajuda() << '\n';
        }
    }
};

class Status : public Comando {
public:
    std::string nome() const override { return "status"; }
    std::string ajuda() const override { return "mostra o modo de voo e a telemetria atual"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        std::cout << estacao.veiculo() << '\n';
    }
};

class Modo : public Comando {
public:
    std::string nome() const override { return "modo"; }
    std::string uso() const override { return "modo <NOME>"; }
    std::string ajuda() const override { return "troca o modo de voo (ex.: modo RTL)"; }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        estacao.veiculo().alterar_modo_de_voo(argumentos.front());
        std::cout << "pedido de troca de modo enviado\n";
    }
};

class Plotar : public Comando {
public:
    std::string nome() const override { return "plotar"; }
    std::string uso() const override { return "plotar <campo>"; }
    std::string ajuda() const override {
        return "abre o gráfico de um campo: " + nomes_dos_campos();
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        const auto& catalogo = campos_disponiveis();
        const auto encontrado = catalogo.find(argumentos.front());
        if (encontrado == catalogo.end()) {
            throw ErroDeComando("campo '" + argumentos.front() +
                                "' não existe. Disponíveis: " + nomes_dos_campos());
        }
        const GraficoTempo grafico{encontrado->second};
        estacao.exibir_painel(grafico);
    }
};

class Horizonte : public Comando {
public:
    std::string nome() const override { return "horizonte"; }
    std::string ajuda() const override { return "abre o horizonte artificial"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        const HorizonteArtificial horizonte;
        estacao.exibir_painel(horizonte);
    }
};

/// Arma o veículo e decola até a altura pedida.
/// Converte o texto digitado em número, validando formato e faixa.
/// Ex.: ler_numero("15", "altura", 1, 100) → 15; "abc" ou "500" → ErroDeComando.
double ler_numero(const std::string& texto, const std::string& nome, double minimo,
                  double maximo) {
    double valor = 0.0;
    std::size_t lidos = 0;
    try {
        valor = std::stod(texto, &lidos);
    } catch (const std::logic_error&) {  // invalid_argument e out_of_range
        lidos = 0;
    }
    if (lidos == 0 || lidos != texto.size() || !std::isfinite(valor)) {
        throw ErroDeComando("valor inválido para " + nome + ": '" + texto + "' não é um número");
    }
    if (valor < minimo || valor > maximo) {
        std::ostringstream faixa;
        faixa << nome << " deve ficar entre " << minimo << " e " << maximo << " m";
        throw ErroDeComando(faixa.str());
    }
    return valor;
}

// Faixas aceitas pelos comandos de voo, em metros.
constexpr double ALTURA_MINIMA_M = 1.0;
constexpr double ALTURA_MAXIMA_M = 100.0;
constexpr double DESLOCAMENTO_MAXIMO_M = 500.0;

/// Arma o veículo e decola até a altura pedida.
class Decolar : public Comando {
public:
    std::string nome() const override { return "decolar"; }
    std::string uso() const override { return "decolar <altura_m>"; }
    std::string ajuda() const override {
        return "arma o drone e decola até a altura pedida (1 a 100 m)";
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        const double altura_m =
            ler_numero(argumentos.front(), "altura", ALTURA_MINIMA_M, ALTURA_MAXIMA_M);

        estacao.veiculo().decolar(altura_m);
        std::cout << "decolando até " << altura_m << " m\n";
    }
};

/// Sobe ou desce até uma nova altura, sem sair do lugar.
class Altitude : public Comando {
public:
    std::string nome() const override { return "altitude"; }
    std::string uso() const override { return "altitude <altura_m>"; }
    std::string ajuda() const override { return "sobe ou desce até a altura pedida, no lugar"; }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        const double altura_m =
            ler_numero(argumentos.front(), "altura", ALTURA_MINIMA_M, ALTURA_MAXIMA_M);

        const Posicao agora = estacao.veiculo().estado().posicao;
        estacao.veiculo().ir_para(agora.na_altura(altura_m));
        std::cout << "indo para " << altura_m << " m de altura\n";
    }
};

/// Voa até um ponto a alguns metros da posição atual.
class Ir : public Comando {
public:
    std::string nome() const override { return "ir"; }
    std::string uso() const override { return "ir <norte_m> <leste_m> <altura_m>"; }
    std::string ajuda() const override {
        return "voa até um ponto relativo (negativo = sul/oeste)";
    }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 3) {
            throw ErroDeComando("uso: " + uso() + "  (ex.: ir 50 0 20)");
        }
        const double norte_m = ler_numero(argumentos.at(0), "norte", -DESLOCAMENTO_MAXIMO_M,
                                          DESLOCAMENTO_MAXIMO_M);
        const double leste_m = ler_numero(argumentos.at(1), "leste", -DESLOCAMENTO_MAXIMO_M,
                                          DESLOCAMENTO_MAXIMO_M);
        const double altura_m =
            ler_numero(argumentos.at(2), "altura", ALTURA_MINIMA_M, ALTURA_MAXIMA_M);

        const Posicao agora = estacao.veiculo().estado().posicao;
        estacao.veiculo().ir_para(agora.deslocada(norte_m, leste_m).na_altura(altura_m));
        std::cout << "indo " << norte_m << " m ao norte e " << leste_m << " m a leste, a "
                  << altura_m << " m de altura\n";
    }
};

class Sair : public Comando {
public:
    std::string nome() const override { return "sair"; }
    std::string ajuda() const override { return "encerra a estação de controle"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.encerrar();
    }
};


class Armar : public Comando {
public:
    std::string nome() const override { return "armar"; }
    std::string ajuda() const override { return "arma os motores"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().armar();
        std::cout << "motores armados\n";
    }
};

class Desarmar : public Comando {
public:
    std::string nome() const override { return "desarmar"; }
    std::string ajuda() const override { return "desarma os motores (só no chão)"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().desarmar();
        std::cout << "motores desarmados\n";
    }
};

class Pousar : public Comando {
public:
    std::string nome() const override { return "pousar"; }
    std::string ajuda() const override { return "pousa no lugar onde está"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().pousar();
        std::cout << "pousando\n";
    }
};

class RTL : public Comando {
public:
    std::string nome() const override { return "rtl"; }
    std::string ajuda() const override { return "volta para o ponto de decolagem e pousa"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().retornar_para_casa();
        std::cout << "retornando para casa\n";
    }
};

class Gravar : public Comando {
public:
    std::string nome() const override { return "gravar"; }
    std::string uso() const override { return "gravar <arquivo.csv>"; }
    std::string ajuda() const override { return "salva o histórico de telemetria em CSV"; }

    void executar(EstacaoDeControle& estacao,
                  const std::vector<std::string>& argumentos) override {
        if (argumentos.size() != 1) {
            throw ErroDeComando("uso: " + uso());
        }
        const std::size_t amostras = estacao.historico().exportar_csv(argumentos.front());
        std::cout << amostras << " amostras gravadas em " << argumentos.front() << '\n';
    }
};

class Limpar : public Comando {
public:
    std::string nome() const override { return "limpar"; }
    std::string ajuda() const override { return "apaga o histórico (os gráficos recomeçam)"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.limpar_historico();
        std::cout << "histórico apagado\n";
    }
};

}  // namespace

RegistroDeComandos comandos_padrao() {
    RegistroDeComandos registro;
    registro.registrar(std::make_unique<Ajuda>());
    registro.registrar(std::make_unique<Status>());
    registro.registrar(std::make_unique<Modo>());
    registro.registrar(std::make_unique<Plotar>());
    registro.registrar(std::make_unique<Horizonte>());
    registro.registrar(std::make_unique<Decolar>());
    registro.registrar(std::make_unique<Altitude>());
    registro.registrar(std::make_unique<Ir>());
    registro.registrar(std::make_unique<Sair>());
    registro.registrar(std::make_unique<Armar>());
    registro.registrar(std::make_unique<Desarmar>());
    registro.registrar(std::make_unique<Pousar>());
    registro.registrar(std::make_unique<RTL>());
    registro.registrar(std::make_unique<Gravar>());
    registro.registrar(std::make_unique<Limpar>());
    
    return registro;
}

}  // namespace gcs
