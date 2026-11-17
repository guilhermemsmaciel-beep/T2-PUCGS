#include "modelo/Telemetria.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace gcs {

std::string EstadoVeiculo::resumo() const {
    std::ostringstream fluxo;
    fluxo << std::fixed << std::setprecision(1)
          << "modo=" << std::left << std::setw(18) << modo_de_voo << std::right
          << " armado=" << (armado ? "sim" : "nao")
          << " em_voo=" << (em_voo ? "sim" : "nao")
          << " alt_rel=" << std::setw(7) << posicao.altitude_relativa_m << " m"
          << " rolagem=" << std::setw(6) << atitude.rolagem << " deg"
          << " arfagem=" << std::setw(6) << atitude.arfagem << " deg"
          << " vel=" << std::setw(5) << velocidade.no_solo() << " m/s"
          << " subida=" << std::setw(5) << velocidade.taxa_de_subida() << " m/s"
          << " bat=" << std::setw(5) << bateria.percentual << " %"
          << " sat=" << gps.satelites
          << " radio=" << (radio.disponivel ? std::to_string(static_cast<int>(radio.sinal_percentual)) + "%" : "sem sinal");
    return fluxo.str();
}

double Velocidade::no_solo() const {
    return std::hypot(norte_m_s, leste_m_s);
}

double segundos_entre(Instante inicio, Instante fim) {
    return std::chrono::duration<double>(fim - inicio).count();
}

}  // namespace gcs
