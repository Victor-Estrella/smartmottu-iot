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
- `SET SETOR VAGA1:<nome>` | `SET SETOR VAGA2:<nome>`

## Testes funcionais sugeridos
- Caso 1: Vaga2 ocupada sem Vaga1 ("vaga errada").
- Caso 2: Nenhuma vaga ocupada ("desaparecida") após ter associado uma moto.
- Caso 3: Entrada/Saída alternadas para verificar debouncing e logs.

## Resultados parciais
- Persistência de eventos na EEPROM usando ring buffer com checksum simples.
- Persistência dos setores (Vaga1/Vaga2) em área reservada da EEPROM, com valores padrão definíveis no código e comando de restauração (`SET SETOR DEFAULTS`).
- Métricas em tempo real via Serial (`STATUS`): tempo de ocupação por vaga e máxima na sessão.
- Lógica de alertas funcional: "vaga errada" e "moto desaparecida" com tempo de tolerância configurável (`SET GRACE`).
- Suporte a dois formatos de saída no Serial (HUMAN/MACHINE) para facilitar leitura e ingestão por ferramentas externas.

## Tecnologias utilizadas
- Arduino UNO (C++/Arduino)
- Wokwi (simulação)
- Bibliotecas: `LiquidCrystal`, `EEPROM`, `DHT sensor library` (Adafruit) e `Adafruit Unified Sensor`
- Serial Monitor para interação/validação

## Vídeo de demonstração
Inclua o link aqui quando publicar no YouTube.
