// Objetos de valor que representam a telemetria de um veículo.
//
// São structs simples e copiáveis: cada pacote de telemetria que chega gera um
// novo EstadoVeiculo completo, em vez de alterar campos espalhados pelo
// programa. Copiar um estado é baratíssimo e torna o acesso entre threads
// (a telemetria do MAVSDK chega em outra thread) muito mais simples de acertar.
#pragma once

#include <chrono>
#include <string>

namespace gcs {

using Instante = std::chrono::steady_clock::time_point;

/// Orientação do veículo, em graus (convenção aeronáutica).
struct Atitude {
    double rolagem = 0.0;  // roll  — positivo com a asa direita abaixada
    double arfagem = 0.0;  // pitch — positivo com o nariz acima do horizonte
    double guinada = 0.0;  // yaw   — 0 = norte, cresce para leste
};

/// Posição global informada pelo GPS.
struct Posicao {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude_absoluta_m = 0.0;  // em relação ao nível do mar (AMSL)
    double altitude_relativa_m = 0.0;  // em relação ao ponto de decolagem

    /// Esta posição deslocada alguns metros para norte e leste (negativos
    /// valem para sul e oeste), na mesma altitude. Boa aproximação para
    /// distâncias curtas, de até alguns quilômetros.
    Posicao deslocada(double norte_m, double leste_m) const;

    /// Esta posição numa nova altura em relação ao ponto de decolagem,
    /// ajustando as duas altitudes.
    Posicao na_altura(double altura_relativa_m) const;
};

/// Estado da bateria principal.
struct Bateria {
    double tensao_v = 0.0;
    double percentual = 0.0;  // 0 a 100
};

/// Velocidade em relação ao solo, no referencial NED (norte, leste, baixo), em m/s.
struct Velocidade {
    double norte_m_s = 0.0;
    double leste_m_s = 0.0;
    double baixo_m_s = 0.0;  // positivo descendo

    /// Velocidade horizontal (no solo), em m/s.
    double no_solo() const;
    /// Taxa de subida, em m/s: positiva subindo, negativa descendo.
    double taxa_de_subida() const { return 0.0 - baixo_m_s; }
};

/// Situação do receptor GPS.
struct Gps {
    int satelites = 0;
};

/// Enlace de rádio (controle remoto).
struct Radio {
    bool disponivel = false;
    double sinal_percentual = 0.0;  // 0 a 100
};

/// Fotografia completa do veículo em um instante.
///
struct EstadoVeiculo {
    std::string modo_de_voo = "DESCONHECIDO";
    bool armado = false;
    bool em_voo = false;
    Atitude atitude{};
    Posicao posicao{};
    Velocidade velocidade{};
    Bateria bateria{};
    Gps gps{};
    Radio radio{};
    Posicao home{};  // ponto de decolagem

    Instante instante = std::chrono::steady_clock::now();

    /// Uma linha legível, usada pelo comando `status`.
    std::string resumo() const;
};

/// Segundos decorridos entre dois instantes.
double segundos_entre(Instante inicio, Instante fim);

}  // namespace gcs
