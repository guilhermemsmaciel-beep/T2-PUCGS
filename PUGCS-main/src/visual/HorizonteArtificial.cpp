#include "visual/HorizonteArtificial.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <vector>

#include "modelo/Telemetria.hpp"
#include "visual/Terminal.hpp"

namespace gcs {
namespace {

/// Quantos graus cada caractere vale. Como um caractere de terminal é mais ou
/// menos duas vezes mais alto do que largo, uma coluna vale metade de uma linha
/// — é isso que mantém o horizonte com a inclinação correta.
constexpr double GRAUS_POR_LINHA = 4.0;
constexpr double GRAUS_POR_COLUNA = 2.0;
constexpr double LIMITE_ESCADA = 35.0;
constexpr double LARGURA_ESCADA = 15.0;
constexpr double RAIO_ARCO = 40.0;

constexpr double PI = 3.14159265358979323846;

/// O que há atrás de cada caractere.
enum class Fundo { Ceu, Chao, Horizonte, Seta, Cinza, Caixa };
/// A cor do caractere desenhado em cima do fundo.
enum class Tinta { Padrao, Simbolo, Branco, Texto };

struct Celula {
    Fundo fundo = Fundo::Ceu;
    Tinta tinta = Tinta::Padrao;
    char glifo = ' ';
};

// Codigos RGB de fundo
const std::string FUNDO_CEU = terminal::fundo(0x2A, 0x78, 0xD6); // azul
const std::string FUNDO_CHAO = terminal::fundo(0x8C, 0x5A, 0x2B); // marrom
const std::string FUNDO_HORIZONTE = terminal::fundo(0xFC, 0xFC, 0xFB); // branco
const std::string FUNDO_CAIXA = terminal::fundo(0x1A, 0x1A, 0x19); // preto
const std::string FUNDO_SETA = terminal::fundo(0xFF, 0xFF, 0x00); // amarelo
const std::string FUNDO_CINZA = terminal::fundo(0xC8, 0xC8, 0xC8); // cinza

// Codigos RGB de tinta
const std::string FRENTE_PADRAO = terminal::frente(0xFC, 0xFC, 0xFB); // branco
const std::string FRENTE_SIMBOLO = terminal::frente(0xED, 0xA1, 0x00); // amarelo
const std::string FRENTE_TEXTO = terminal::frente(0xFC, 0xFC, 0xFB); // branco
const std::string FRENTE_BRANCO = terminal::frente(0xFF, 0xFF, 0xFF);

const std::string& codigo(Fundo fundo) {
    switch (fundo) {
        case Fundo::Chao: return FUNDO_CHAO;
        case Fundo::Horizonte: return FUNDO_HORIZONTE;
        case Fundo::Caixa: return FUNDO_CAIXA;
        case Fundo::Seta: return FUNDO_SETA;
        case Fundo::Cinza: return FUNDO_CINZA;
        case Fundo::Ceu: break;
    }
    return FUNDO_CEU;
}

const std::string& codigo(Tinta tinta) {
    switch (tinta) {
        case Tinta::Simbolo: return FRENTE_SIMBOLO;
        case Tinta::Texto: return FRENTE_TEXTO;
        case Tinta::Branco: return FRENTE_BRANCO;
        case Tinta::Padrao: break;
    }
    return FRENTE_PADRAO;
}

std::string graus(double valor) {
    std::ostringstream fluxo;
    fluxo << std::fixed << std::setprecision(1) << std::showpos << std::setw(7) << valor;
    return fluxo.str();
}

}  // namespace

HorizonteArtificial::HorizonteArtificial(int largura, int altura)
    : Painel("Horizonte artificial"),
      largura_(std::max(40, largura)),
      altura_(std::max(11, altura | 1)) {}  // altura ímpar: existe uma linha central


std::string HorizonteArtificial::quadro(const Historico& historico) const {
    bool recebeu = false;
    const EstadoVeiculo estado = historico.ultimo(&recebeu);
    std::ostringstream saida;
    if (!recebeu) {
        saida << titulo() << terminal::LIMPAR_LINHA << "\n\n"
              << "aguardando telemetria..." << terminal::LIMPAR_LINHA << "\n";
        return saida.str();
    }

    // 1. Decide, caractere por caractere, se ali é céu, chão ou horizonte.
    const double seno = std::sin(estado.atitude.rolagem * PI / 180.0);
    const double cosseno = std::cos(estado.atitude.rolagem * PI / 180.0);
    const double centro_linha = (altura_ - 1) / 2.0;
    const double centro_coluna = (largura_ - 1) / 2.0;

    // Vetor com os caracteres, vulgo "celulas"
    std::vector<std::vector<Celula>> tela(
        static_cast<std::size_t>(altura_),
        std::vector<Celula>(static_cast<std::size_t>(largura_)));

    // bool aproximacao = [] (double valor, double medida) {
    //     for (int i = medida)
    // }


    // Versão Original
    // for (int linha = 0; linha < altura_; ++linha) {
    //     for (int coluna = 0; coluna < largura_; ++coluna) {
    //         const double y = (centro_linha - linha) * GRAUS_POR_LINHA; // Posicao em graus horizontal
    //         const double x = (coluna - centro_coluna) * GRAUS_POR_COLUNA; // Posicao em graus vertical
    //         const double u = -x * seno + y * cosseno + estado.atitude.arfagem; // Distancia em graus entre celula e linha do horizonte
    //         const double inverse_u = x * cosseno + y * seno + estado.atitude.arfagem; // u rotacionado em 90 graus
    //         const double resto = (std::abs(u) < LIMITE_ESCADA) ? std::abs(std::remainder(u, 10.0)) : 100.0;
    //         const double raio = sqrt((y * y) + (x * x));
    //         Celula& celula = tela[static_cast<std::size_t>(linha)][static_cast<std::size_t>(coluna)];
    //         // || std::abs(inverse_u) <= GRAUS_POR_LINHA * 0.6
            
    //         // Desenha Arco de Rolagem
    //         if (y >= 0.0 && raio <= (RAIO_ARCO + (GRAUS_POR_LINHA * 0.6)) && raio > (RAIO_ARCO - (GRAUS_POR_LINHA * 0.3))) {
    //             if (-u <= GRAUS_POR_LINHA * 0.6 && std::abs(inverse_u) <= GRAUS_POR_LINHA * 0.6) {
    //                 celula.fundo = Fundo::Seta;
    //                 //celula.glifo = 'o';
    //                 //celula.tinta = Tinta::Simbolo;
    //             }
    //             else {
    //                 celula.fundo = Fundo::Cinza;
    //                 //celula.tinta = Tinta::Branco;
    //                 //celula.glifo = 'o';
    //             }
    //         }
    //         else if (std::abs(u) <= GRAUS_POR_LINHA * 0.6 ) { // Desenha Horizonte
    //             celula.fundo = Fundo::Horizonte;
    //         } 
    //         else if (resto <= GRAUS_POR_LINHA * 0.6) { // Desenha Escada de Arfagem
    //             if ((inverse_u) < LARGURA_ESCADA && (inverse_u) > -LARGURA_ESCADA) {
    //                 celula.fundo = Fundo::Cinza;
    //                 //celula.glifo = '-';
    //                 //celula.tinta = Tinta::Branco;
    //                 if (std::abs(inverse_u) <= GRAUS_POR_LINHA * 0.6) {
    //                     //celula.glifo = '1';
    //                 }
    //                     //celula.glifo = (std::abs(seno) < 0.5) ? '-' : '|';
    //             }
    //             else {
    //                 celula.fundo = (u > 0.0) ? Fundo::Ceu : Fundo::Chao; 
    //             }
                
    //         }
    //         else {
    //             celula.fundo = (u > 0.0) ? Fundo::Ceu : Fundo::Chao;
    //         }
    //     }
    // }


    // Versão Teste
    for (int linha = 0; linha < altura_; ++linha) {
        for (int coluna = 0; coluna < largura_; ++coluna) {
            const double y = (centro_linha - linha) * GRAUS_POR_LINHA; // Posicao em graus horizontal
            const double x = (coluna - centro_coluna) * GRAUS_POR_COLUNA; // Posicao em graus vertical
            const double u = -x * seno + y * cosseno + estado.atitude.arfagem; // Distancia em graus entre celula e linha do horizonte
            const double inverse_u = x * cosseno + y * seno + estado.atitude.arfagem; // u rotacionado em 90 graus
            const double resto = (std::abs(u) < LIMITE_ESCADA) ? std::abs(std::remainder(u, 10.0)) : 100.0; // Define quantos graus da escada devem aparecer
            const double raio = sqrt((y * y) + (x * x));
            Celula& celula = tela[static_cast<std::size_t>(linha)][static_cast<std::size_t>(coluna)];
            // || std::abs(inverse_u) <= GRAUS_POR_LINHA * 0.6
            
            // Desenha Arco de Rolagem
            if (y >= 0.0 && raio <= (RAIO_ARCO + (GRAUS_POR_LINHA * 0.6)) && raio > (RAIO_ARCO - (GRAUS_POR_LINHA * 0.3))) {
                if (-u <= GRAUS_POR_LINHA * 0.6 && std::abs(inverse_u) <= GRAUS_POR_LINHA * 0.6) {
                    //celula.fundo = Fundo::Seta;
                    celula.glifo = 'R';
                    celula.tinta = Tinta::Simbolo;
                }
                else {
                    celula.glifo = '.';
                    celula.tinta = Tinta::Branco;
                    //celula.fundo = Fundo::Cinza;
                    //celula.tinta = Tinta::Branco;
                    //celula.glifo = 'o';
                }
                celula.fundo = (u > 0.0) ? Fundo::Ceu : Fundo::Chao;
            }
            else if (std::abs(u) <= GRAUS_POR_LINHA * 0.6 ) { // Desenha Horizonte
                celula.fundo = Fundo::Horizonte;
            }
            else { // resto <= GRAUS_POR_LINHA * 0.6 &&
                if (resto <= GRAUS_POR_LINHA * 0.6 && (inverse_u) < LARGURA_ESCADA && (inverse_u) > -LARGURA_ESCADA) {
                    celula.tinta = Tinta::Branco;
                    // if (estado.atitude.rolagem >= 20.0) celula.glifo = '/';
                    // else if (estado.atitude.rolagem <= -20.0) celula.glifo = '\\'; 
                    // else celula.glifo = '-';
                    celula.glifo = '\'';
                    //celula.fundo = Fundo::Cinza;
                }
                celula.fundo = (u > 0.0) ? Fundo::Ceu : Fundo::Chao;
            }
        }
    }

    // 2. O símbolo da aeronave, sempre fixo no centro: é a referência do piloto.
    const int linha_central = altura_ / 2;
    const int coluna_central = largura_ / 2;
    // Escreve um caractere por cima do desenho. `fundo` nulo mantém o céu/chão
    // que já estava ali — é o que faz o símbolo da aeronave parecer parte do HUD.
    const auto escrever = [&](int linha, int coluna, char glifo, Tinta tinta,
                              std::optional<Fundo> fundo = std::nullopt) {
        if (linha < 0 || linha >= altura_ || coluna < 0 || coluna >= largura_) {
            return;
        }
        Celula& celula = tela[static_cast<std::size_t>(linha)][static_cast<std::size_t>(coluna)];
        celula.glifo = glifo;
        celula.tinta = tinta;
        if (fundo.has_value()) {
            celula.fundo = *fundo;
        }
    };



    // Escreve o simbolo da aeronave (=====o======)
    for (int distancia = 5; distancia <= 11; ++distancia) {
        escrever(linha_central, coluna_central - distancia, '=', Tinta::Simbolo, Fundo::Seta);
        escrever(linha_central, coluna_central + distancia, '=', Tinta::Simbolo, Fundo::Seta);
    }
    escrever(linha_central, coluna_central, 'o', Tinta::Simbolo, Fundo::Seta);


    // for (int distancia = 0; distancia <= 20; distancia++) {
    //     escrever(linha_central + 7, coluna_central - distancia, '-', Tinta::Simbolo, Fundo::Cinza);
    //     escrever(linha_central + 7, coluna_central + distancia, '-', Tinta::Simbolo, Fundo::Cinza);
    // }
    // Teste
    // for (int linha = 0; linha < altura_; linha++) {
    //     for (int coluna = 0; coluna < largura_; coluna++) {
    //         Celula& celula = tela[linha][coluna];
    //         celula.glifo = 'a';
    //     }
    // }

    // 3. As leituras numéricas, em uma caixa escura para ficarem legíveis tanto
    //    sobre o céu quanto sobre o chão.
    std::vector<std::string> leituras{
        " modo    " + estado.modo_de_voo,
        " rolagem " + graus(estado.atitude.rolagem) + " deg",
        " arfagem " + graus(estado.atitude.arfagem) + " deg",
        " rumo    " + graus(estado.atitude.guinada) + " deg",
    };
    std::size_t largura_caixa = 0;
    for (const std::string& leitura : leituras) {
        largura_caixa = std::max(largura_caixa, leitura.size() + 1);
    }
    for (std::size_t i = 0; i < leituras.size(); ++i) {
        leituras[i].resize(largura_caixa, ' ');  // deixa a caixa retangular
        for (std::size_t j = 0; j < leituras[i].size(); ++j) {
            escrever(static_cast<int>(i) + 1, static_cast<int>(j) + 2, leituras[i][j],
                     Tinta::Texto, Fundo::Caixa);
        }
    }

    // 4. Emite a tela, trocando as cores só quando elas mudam de verdade.
    saida << terminal::NEGRITO << titulo() << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    for (const std::vector<Celula>& linha : tela) {
        const Fundo* fundo_atual = nullptr;
        const Tinta* tinta_atual = nullptr;
        for (const Celula& celula : linha) {
            if (fundo_atual == nullptr || celula.fundo != *fundo_atual) {
                saida << codigo(celula.fundo);
                fundo_atual = &celula.fundo;
            }
            if (tinta_atual == nullptr || celula.tinta != *tinta_atual) {
                saida << codigo(celula.tinta);
                tinta_atual = &celula.tinta;
            }
            saida << celula.glifo;
        }
        saida << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    }
    saida << "\n"
          << terminal::frente(0x82, 0x81, 0x7C) << "[Enter] volta para o console"
          << terminal::NORMAL << terminal::LIMPAR_LINHA << "\n";
    return saida.str();
}

// TODO (aluno): o horizonte está no mínimo necessário. Melhorias naturais:
//  * escada de arfagem: linhas a cada 10 graus, com o número ao lado;
//  * arco de rolagem no topo, com um ponteiro triangular;
//  * fita de rumo (compasso) na base e fitas de altitude/velocidade nas laterais;
//  * pintar de vermelho quando a rolagem passar de um limite de segurança;
//  * indicador de bateria e de modo de voo com destaque quando houver falha.


// Escada de Arfagem: fixa em relação ao horizonte. Se o horizonte tá inclinado, a escada acompanha. 
// são basicamente linhas brancas retas com o número ao lado. Parecido com a linha de horizonte só que deslocada



}  // namespace gcs



// ./mini_gcs --endereco udpin://:14552 --simulado
