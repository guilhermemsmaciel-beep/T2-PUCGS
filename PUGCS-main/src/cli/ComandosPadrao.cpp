#include "cli/ComandosPadrao.hpp"

#include <iostream>
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

/// Exemplo de comando deixado como exercício: o console trata NaoImplementado
/// de forma amigável, então dá para registrar o esqueleto antes de implementar.
class Decolar : public Comando {
public:
    std::string nome() const override { return "decolar"; }
    std::string uso() const override { return "decolar <altura_m>"; }
    std::string ajuda() const override {
        return "[exercício] arma o drone e decola até a altura pedida";
    }

    void executar(EstacaoDeControle& estacao, const std::vector<std::string>& argumentos) override {
        // TODO (aluno): validar a altura recebida, chamar veiculo().armar() e
        // veiculo().decolar(altura) — métodos que você vai declarar em Veiculo e
        // implementar em Drone — e avisar o usuário do resultado.

        // auto altura_m = [] (const std::vector<std::string>& argumentos) {
        //     int altura = stoi(argumentos[0]);
        //     return static_cast<double>(altura);
        // };

        int altura = stoi(argumentos[0]);

    
        estacao.veiculo().armar();
        estacao.veiculo().decolar(static_cast<double>(altura));

        //throw NaoImplementado("decolagem");
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
