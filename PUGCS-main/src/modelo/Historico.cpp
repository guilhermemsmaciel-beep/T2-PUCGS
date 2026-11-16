#include "modelo/Historico.hpp"

#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace gcs {

Historico::Historico(std::size_t capacidade) : capacidade_(capacidade) {}

void Historico::registrar(const EstadoVeiculo& estado) {
    std::lock_guard<std::mutex> trava(mutex_);
    estados_.push_back(estado);
    while (estados_.size() > capacidade_) {
        estados_.pop_front();
    }
}

std::size_t Historico::tamanho() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estados_.size();
}

bool Historico::vazio() const {
    std::lock_guard<std::mutex> trava(mutex_);
    return estados_.empty();
}

EstadoVeiculo Historico::ultimo(bool* ok) const {
    std::lock_guard<std::mutex> trava(mutex_);
    if (estados_.empty()) {
        if (ok != nullptr) {
            *ok = false;
        }
        return EstadoVeiculo{};
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return estados_.back();
}

Historico::Serie Historico::serie(const Extrator& extrator) const {
    std::lock_guard<std::mutex> trava(mutex_);
    Serie resultado;
    if (estados_.empty()) {
        return resultado;
    }
    const Instante t0 = estados_.front().instante;
    resultado.segundos.reserve(estados_.size());
    resultado.valores.reserve(estados_.size());
    for (const EstadoVeiculo& estado : estados_) {
        resultado.segundos.push_back(segundos_entre(t0, estado.instante));
        resultado.valores.push_back(extrator(estado));
    }
    return resultado;
}

void Historico::limpar() {
    std::lock_guard<std::mutex> trava(mutex_);
    estados_.clear();
}

std::size_t Historico::exportar_csv(const std::string& caminho) const {
    // Copia as amostras sob a trava e escreve o arquivo fora dela, para não
    // segurar a telemetria (que chega em outra thread) durante a escrita.
    std::deque<EstadoVeiculo> copia;
    {
        std::lock_guard<std::mutex> trava(mutex_);
        copia = estados_;
    }

    std::ofstream arquivo(caminho);
    if (!arquivo) {
        throw std::runtime_error("não foi possível criar o arquivo '" + caminho + "'");
    }
    arquivo << std::setprecision(10);  // latitude/longitude precisam de casas decimais

    arquivo << "tempo_s,modo,armado,em_voo,latitude,longitude,altitude_rel_m,"
               "velocidade_m_s,subida_m_s,rolagem_deg,arfagem_deg,guinada_deg,"
               "bateria_pct,satelites\n";
    if (!copia.empty()) {
        const Instante t0 = copia.front().instante;
        for (const EstadoVeiculo& e : copia) {
            arquivo << segundos_entre(t0, e.instante) << ','
                    << '"' << e.modo_de_voo << '"' << ','
                    << e.armado << ',' << e.em_voo << ','
                    << e.posicao.latitude << ',' << e.posicao.longitude << ','
                    << e.posicao.altitude_relativa_m << ','
                    << e.velocidade.no_solo() << ',' << e.velocidade.taxa_de_subida() << ','
                    << e.atitude.rolagem << ',' << e.atitude.arfagem << ','
                    << e.atitude.guinada << ','
                    << e.bateria.percentual << ',' << e.gps.satelites << '\n';
        }
    }

    if (!arquivo) {
        throw std::runtime_error("erro ao escrever em '" + caminho + "'");
    }
    return copia.size();
}

}  // namespace gcs
