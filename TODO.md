# PUGCS
Parceria PUCRS x Skydrones. Este projeto consiste na implementação de uma ground control station mínima em C++.

### Tasks
- [] Modelo
  - [] ampliar EstadoVeiculo (velocidade, taxa de subida, satélites, rádio, home);
  - [] gerar telemetria sintética em VeiculoSimulado para testar sem o SITL;
- [] Visualização
  - [] novos campos no catálogo de plotar; escada de arfagem, arco de rolagem e fita de rumo no horizonte; ligar os pontos do gráfico; alarmes visuais (bateria baixa, rolagem excessiva);
- [] Extra
  - [] Interface Gráfica: Trocar visualização por uma janela.

### In Progress
- [] Comando
  - [] implementar decolar e criar os seus (armar, pousar, rtl, gravar, limpar...). Copie uma classe existente;

### Completed ✓
- [x] declarar em Veiculo e implementar em Drone: armar, desarmar, decolar, pousar, retornar_para_casa, ir_para;
- [x] completar o mapa de modos de voo (GUIDED, AUTO, LOITER...);
