# SmartMottu IoT (Sprint 3)

Protótipo IoT simulado no Wokwi para gestão de vagas de motos com:
- 2 sensores PIR (vagas 1 e 2)
- 1 sensor DHT22 (temperatura/umidade)
- LCD 16x2 (telemetria/status)
- LED + buzzer (alertas)
- EEPROM (persistência como ring buffer)
- Interface serial (comandos e dashboard opcional)

## Objetivo
Demonstrar integração de sensores/atuadores, persistência e evidências quantitativas/qualitativas de performance para a Sprint 3.

## Estrutura do projeto

```
.
├─ SmartMottu_IoT.ino        # Firmware Arduino UNO (lógica, métricas, alertas, EEPROM)
├─ wokwi-diagram.json        # Diagrama/conexões para simulação no Wokwi
└─ README.md                 # Este documento
```

Código-fonte principal: `SmartMottu_IoT.ino`

## Hardware (Wokwi)
- Arduino UNO
- PIR x2 nos pinos 2 e 3
- DHT22 no pino 4
- LED no pino 5
- Buzzer no pino 6
- LCD1602 RS=12, E=11, D4=9, D5=8, D6=10, D7=7

O diagrama está em `wokwi-diagram.json` (você pode colar o conteúdo no `diagram.json` do Wokwi) e os pinos batem com o sketch.

## Funcionalidades
- Detecção por mudança de estado das vagas (evita spam de logs)
- Log estruturado na EEPROM (ring buffer com checksum simples)
- Comandos seriais:
  - HELP
  - ASSOC VAGA1:<ID> | ASSOC VAGA2:<ID>
  - STATUS
  - LOG DUMP | LOG CLEAR
  - SET BUZZER ON|OFF / SET LED ON|OFF (manual)
  - SET SETOR VAGA1:<nome> | SET SETOR VAGA2:<nome> (persistem na EEPROM)
  - SET SETOR DEFAULTS (aplica valores padrão definidos no código)
- LCD com duas telas alternando: vagas e clima.
- Alertas:
  - Moto em vaga errada
  - Motos desaparecidas (nenhuma vaga ocupada quando esperado)
- Métricas: contagem de eventos por tipo, tempo de ocupação, temperatura/umidade médias e máximas (sessão)

## Como rodar (Wokwi)
1. Crie um projeto Arduino Uno no Wokwi.
2. Abra o arquivo `diagram.json` do projeto e substitua o conteúdo pelo de `wokwi-diagram.json` deste repositório (ou adicione os mesmos componentes e conexões manualmente).
3. Crie/abra o `sketch.ino` e cole o conteúdo de `SmartMottu_IoT.ino`.
4. Adicione a biblioteca “DHT sensor library” (Adafruit) no Wokwi (Libraries → procure por “DHT sensor library”). Se pedir dependência, adicione também “Adafruit Unified Sensor”.
5. Inicie a simulação.
6. Abra o Monitor Serial a 9600 baud e envie os comandos abaixo.

Observação sobre Wokwi Chat: você pode colar o conteúdo do `sketch.ino` e do `diagram.json` diretamente via Chat para o Wokwi montar. Depois, use o Monitor Serial no próprio Wokwi para interagir com o firmware.

### Dica: definir Setor sem usar Serial
- No topo do `SmartMottu_IoT.ino`, edite:
  - `DEFAULT_SETOR_V1 = "A1"`
  - `DEFAULT_SETOR_V2 = "B2"`
- Faça upload. Na primeira inicialização (ou se não houver dados válidos), esses valores são gravados na EEPROM e passam a persistir.
- Opcional: `SET SETOR DEFAULTS` força reaplicar os defaults do código e salvar.

## Comandos rápidos
- `HELP`
- `ASSOC VAGA1:MOTO123`
- `ASSOC VAGA2:MOTO456`
- `STATUS`
- `LOG DUMP`
- `LOG CLEAR`
- `OUTPUT HUMAN` | `OUTPUT MACHINE` (muda formato do STATUS periódico)
- `MODE MAINT ON|OFF` (modo manutenção, suprime alertas)
- `SET GRACE <segundos>` (tolerância antes de considerar "desaparecida")
- `SET SETOR VAGA1:<nome>` | `SET SETOR VAGA2:<nome>`
- `SCAN VAGA1:<ID>` | `SCAN VAGA2:<ID>` (simula leitura de QR)

### Roteiro de teste (copiar/colar no Monitor Serial)
1) `HELP`
2) `ASSOC VAGA1:MOTO123`
3) `ASSOC VAGA2:MOTO456`
4) Simule Vaga2 ocupada com Vaga1 livre (mova o PIR2 no Wokwi) e observe alerta no LCD/Serial.
5) `STATUS` (verifique também as METRICS após alguns segundos de simulação)
6) `LOG DUMP` (confira logs persistidos)
7) `LOG CLEAR` (limpe os logs e confirme com outro `LOG DUMP`)
8) `SET SETOR VAGA1:AZUL` e `SET SETOR VAGA2:VERDE` (veja no LCD/STATUS)
9) `SCAN VAGA1:MOTO123` (OK) e depois `SCAN VAGA1:MOTO999` (gera divergência)
10) `SET GRACE 10` e depois deixe ambas vagas livres por >10s para disparar "desaparecida"
11) `MODE MAINT ON` enquanto movimenta sensores para não gerar alertas

## Dashboard opcional (fora do Wokwi)
Se você usar uma placa física, pode integrar a saída do Serial a um dashboard/CSV (por exemplo, com Python + `pyserial`). Não está incluído neste projeto focado no `.ino` e Wokwi.

## Como mudar Temperatura/Umidade no Wokwi
- Clique no sensor DHT22 no diagrama.
- No painel de propriedades (lado direito), ajuste os sliders/valores de Temperatura (°C) e Umidade (%).
- Alternativamente, edite o `simulator`/`props` do DHT no `diagram.json` do Wokwi, mas a interface gráfica é a forma mais rápida.

## Testes funcionais sugeridos
- Caso 1: Vaga2 ocupada sem Vaga1 ("vaga errada").
- Caso 2: Nenhuma vaga ocupada ("desaparecida") após ter associado uma moto.
- Caso 3: Entrada/Saída alternadas para verificar debouncing e logs.
- Caso 4: Temperatura/umidade variando (slider do Wokwi) e métricas no `STATUS`.

## Tecnologias utilizadas
- Arduino UNO (C++/Arduino)
- Wokwi (simulação)
- Bibliotecas: `LiquidCrystal`, `EEPROM`, `DHT sensor library` (Adafruit) e `Adafruit Unified Sensor`
- Serial Monitor para interação/validação

## Resultados parciais (Sprint 3)
- Persistência de eventos em ring buffer com checksums e pageo correto da EEPROM
- Persistência de Setor (Vaga1/Vaga2) em área reservada da EEPROM, com defaults no código e comando de restauração
- Métricas online: tempo de ocupação por vaga, médias/máximos de T/Umid
- Alertas funcionais (vaga errada e desaparecida com grace configurável)
- LCD com duas páginas rotativas (vagas/setores e clima/IDs)
- Saída Serial em dois formatos (HUMAN/MACHINE) para fácil leitura e ingestão

## Critérios Sprint 3 (mapeamento)
- Dashboard/output visual (LCD + opcional dashboard serial): até 30 pts
- Persistência (EEPROM ring buffer + CSV): até 20 pts
- Organização/documentação (este README, comandos, testes, estrutura): até 20 pts

## Vídeo de demonstração
Inclua o link aqui quando publicar no YouTube.
