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
class Decolar : public Comando {
public:
    /// Faixa aceita para a altura de decolagem, em metros.
    static constexpr double ALTURA_MINIMA_M = 1.0;
    static constexpr double ALTURA_MAXIMA_M = 100.0;

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
        const double altura_m = ler_altura(argumentos.front());

        estacao.veiculo().armar();
        estacao.veiculo().decolar(altura_m);
        std::cout << "decolando até " << altura_m << " m\n";
    }

private:
    /// "1 e 100 m" — usado na mensagem de erro de faixa.
    static std::string faixa() {
        std::ostringstream texto;
        texto << ALTURA_MINIMA_M << " e " << ALTURA_MAXIMA_M << " m";
        return texto.str();
    }

    /// Converte o texto digitado em altura, validando formato e faixa.
    static double ler_altura(const std::string& texto) {
        double altura_m = 0.0;
        std::size_t lidos = 0;
        try {
            altura_m = std::stod(texto, &lidos);
        } catch (const std::logic_error&) {  // invalid_argument e out_of_range
            throw ErroDeComando("altura inválida: '" + texto + "' não é um número");
        }
        if (lidos != texto.size() || !std::isfinite(altura_m)) {
            throw ErroDeComando("altura inválida: '" + texto + "' não é um número");
        }
        if (altura_m < ALTURA_MINIMA_M || altura_m > ALTURA_MAXIMA_M) {
            throw ErroDeComando("a altura deve ficar entre " + faixa());
        }
        return altura_m;
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
    std::string ajuda() const override { return "arma o veiculo"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().armar();
    }
};


class Pousar : public Comando {
public:
    std::string nome() const override { return "pousar"; }
    std::string ajuda() const override { return "pousa o veiculo"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().pousar();
    }
};


class RTL : public Comando {
public:
    std::string nome() const override { return "rtl"; }
    std::string ajuda() const override { return "retorna veiculo ao ponto de decolagem"; }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>&) override {
        estacao.veiculo().retornar_para_casa();
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
    registro.registrar(std::make_unique<Sair>());
    registro.registrar(std::make_unique<Armar>());
    registro.registrar(std::make_unique<Pousar>());
    registro.registrar(std::make_unique<RTL>());
    registro.registrar(std::make_unique<Gravar>());
    registro.registrar(std::make_unique<Limpar>());
    
    return registro;
}

}  // namespace gcs
