#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <DHT.h>

#define PIR_PIN 2         // Sensor de presença (Vaga 1)
#define PIR_PIN2 3        // Sensor de presença (Vaga 2)
#define DHT_PIN 4         // Sensor de temperatura/umidade
#define DHT_TYPE DHT22

#define LED_PIN 5         // LED de alerta
#define BUZZER_PIN 6      // Buzzer de alerta

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
DHT dht(DHT_PIN, DHT_TYPE);

// Estruturas e estados
struct LogEntry {
  uint32_t ts;       // millis()
  uint8_t code;      // tipo de evento
  int8_t v1;         // 0/1 para vagas, -1 se N/A
  int8_t v2;
  int16_t t10;       // temp *10 (int16 para não saturar)
  int8_t h1;         // hum /1
  uint8_t crc;       // checksum simples
};

// Códigos de evento
enum EventCode : uint8_t {
  EVT_BOOT=1, EVT_V1_CHANGE=2, EVT_V2_CHANGE=3, EVT_ALERT=4, EVT_ASSOC=5, EVT_STATUS=6
};

// Ring buffer
const int EEPROM_SIZE = 1024; // Uno: 1KB
const int SECTOR_AREA_SIZE = 16; // bytes reservados no final para setores persistidos
const int LOG_EEPROM_SIZE = EEPROM_SIZE - SECTOR_AREA_SIZE; // área exclusiva para logs
const int ENTRY_SIZE = sizeof(LogEntry);
const int MAX_ENTRIES = LOG_EEPROM_SIZE / ENTRY_SIZE; // calcula com base apenas na área de logs
int headIndex = 0; // próxima escrita

// Identificação das motos nas vagas
String motoVaga1 = "";
String motoVaga2 = "";
String setorVaga1 = "-";
String setorVaga2 = "-";

// Valores padrão de setor (edite aqui e regrave para definir sem usar Serial)
const char DEFAULT_SETOR_V1[] = "Manutenção"; // ex.: "A1"
const char DEFAULT_SETOR_V2[] = "Alugar"; // ex.: "B2"

// Estados anteriores para detecção de mudança
int lastV1 = -1;
int lastV2 = -1;
unsigned long lastLcdSwitch = 0;
bool lcdPage = false; // false: vagas, true: clima

// Métricas
unsigned long v1OccupiedSince = 0;
unsigned long v2OccupiedSince = 0;
unsigned long v1TotalMs = 0;
unsigned long v2TotalMs = 0;
float tempSum = 0;
float humSum = 0;
unsigned long samples = 0;
float tempMax = -1000, humMax = -1000;
bool lastAlertState = false;
bool outputHuman = true; // controla formato de saída no loop (HUMAN/MACHINE)
String lastAlertMsg = "OK"; // mensagem do último estado de alerta
bool maintenanceMode = false; // suprime alertas durante manutenção
unsigned long graceMs = 5000; // período de tolerância para 'desaparecida'
unsigned long lastAnyOccupiedMs = 0; // última vez que qualquer vaga estava ocupada
// Ocupação com retenção (debounce/hold)
unsigned long holdMs = 3000; // mantém 'ocupada' por este tempo após o PIR desativar
unsigned long lastDetectV1Ms = 0;
unsigned long lastDetectV2Ms = 0;

// Persistência dos setores na EEPROM (reservados ao final da EEPROM)
struct SectorData {
  char s1[7];   // até 6 chars + terminador
  char s2[7];   // até 6 chars + terminador
  uint8_t ver;  // versão do layout
  uint8_t crc;  // checksum simples (xor)
};
const uint8_t SECTOR_VER = 1;

uint8_t calcSectorCrc(const SectorData &e) {
  const uint8_t *p = (const uint8_t *)&e;
  uint8_t c = 0;
  for (size_t i=0;i<sizeof(SectorData)-1;i++) c ^= p[i];
  return c;
}

void loadSetores() {
  SectorData sd; // conteúdo bruto da EEPROM
  int base = LOG_EEPROM_SIZE; // setor começa imediatamente após a área de logs
  uint8_t *p = (uint8_t*)&sd;
  for (int i=0;i<sizeof(SectorData); i++) p[i] = EEPROM.read(base+i);
  if (sd.ver == SECTOR_VER && sd.crc == calcSectorCrc(sd)) {
    // garante null-termination
    ((char*)sd.s1)[6] = '\0';
    ((char*)sd.s2)[6] = '\0';
    String s1 = String(sd.s1);
    String s2 = String(sd.s2);
    if (s1.length() > 0) setorVaga1 = s1; // mantém "-" padrão se vazio
    if (s2.length() > 0) setorVaga2 = s2;
  } else {
    // Inicializa com padrões de compilação e persiste
    setorVaga1 = String(DEFAULT_SETOR_V1);
    setorVaga2 = String(DEFAULT_SETOR_V2);
    setorVaga1.toUpperCase();
    setorVaga2.toUpperCase();
    saveSetores();
  }
}

void saveSetores() {
  SectorData sd = {}; // zera buffers e campos
  sd.ver = SECTOR_VER;

  String s1 = setorVaga1; s1.trim(); if (s1 == "-") s1 = ""; // já está em MAIÚSCULAS por toUpperCase()
  String s2 = setorVaga2; s2.trim(); if (s2 == "-") s2 = "";
  for (int i=0; i<6 && i < s1.length(); i++) sd.s1[i] = s1[i];
  for (int i=0; i<6 && i < s2.length(); i++) sd.s2[i] = s2[i];

  sd.crc = calcSectorCrc(sd);

  int base = LOG_EEPROM_SIZE;
  for (int i=0;i<sizeof(SectorData); i++) EEPROM.update(base+i, *(((uint8_t*)&sd)+i));
}

uint8_t calcCrc(const LogEntry &e) {
  const uint8_t *p = (const uint8_t *)&e;
  uint8_t c = 0;
  for (size_t i=0;i<sizeof(LogEntry)-1;i++) c ^= p[i];
  return c;
}

void eepromWriteEntry(const LogEntry &in) {
  LogEntry e = in;
  e.crc = calcCrc(e);
  int addr = headIndex * ENTRY_SIZE; // garantido < LOG_EEPROM_SIZE por MAX_ENTRIES
  for (int i=0;i<ENTRY_SIZE;i++) {
    EEPROM.update(addr+i, *(((uint8_t*)&e)+i));
  }
  headIndex = (headIndex + 1) % MAX_ENTRIES;
}

void logEvent(uint8_t code, int v1, int v2, float temp, float hum) {
  LogEntry e;
  e.ts = millis();
  e.code = code;
  e.v1 = v1 >= 0 ? (v1 ? 1 : 0) : -1;
  e.v2 = v2 >= 0 ? (v2 ? 1 : 0) : -1;
  e.t10 = (int16_t)constrain((int)(temp*10), -32768, 32767);
  e.h1 = (int8_t)constrain((int)(hum), -127, 127);
  e.crc = 0;
  eepromWriteEntry(e);
}

void printHelp() {
  Serial.println(F("Comandos:"));
  Serial.println(F(" HELP"));
  Serial.println(F(" ASSOC VAGA1:<ID> | ASSOC VAGA2:<ID>"));
  Serial.println(F(" STATUS"));
  Serial.println(F(" LOG DUMP | LOG CLEAR"));
  Serial.println(F(" SET BUZZER ON|OFF | SET LED ON|OFF"));
  Serial.println(F(" OUTPUT HUMAN | OUTPUT MACHINE"));
  Serial.println(F(" MODE MAINT ON|OFF"));
  Serial.println(F(" SET GRACE <seg>"));
  Serial.println(F(" SET HOLD <seg>"));
  Serial.println(F(" SET SETOR VAGA1:<nome> | SET SETOR VAGA2:<nome>"));
  Serial.println(F(" SCAN VAGA1:<ID> | SCAN VAGA2:<ID>"));
  Serial.println(F(" Obs: Setor persiste na EEPROM (ate 6 chars, MAIUSCULAS)."));
  Serial.println(F("      Espaco apos ':' eh aceito; LCD mostra so os 5 primeiros."));
  Serial.println(F(" Dica: Para nao usar Serial, edite DEFAULT_SETOR_V1/V2 no .ino e regrave."));
  Serial.println(F(" Extra: SET SETOR DEFAULTS  -> aplica os DEFAULT_SETOR_* e salva."));
}

void dumpLogs() {
  Serial.println(F("# Dump de logs (ts,code,v1,v2,tempC,hum)"));
  for (int i=0;i<MAX_ENTRIES;i++) {
    int idx = (headIndex + i) % MAX_ENTRIES;
    int addr = idx * ENTRY_SIZE; // lê apenas dentro da área de logs
    LogEntry e; uint8_t *p=(uint8_t*)&e;
    for (int j=0;j<ENTRY_SIZE;j++) p[j] = EEPROM.read(addr+j);
    if (e.crc != calcCrc(e)) continue; // pula vazio/corrompido
    float t = e.t10/10.0;
    float h = e.h1;
    Serial.print(e.ts); Serial.print(",");
    Serial.print(e.code); Serial.print(",");
    Serial.print((int)e.v1); Serial.print(",");
    Serial.print((int)e.v2); Serial.print(",");
    Serial.print(t,1); Serial.print(",");
    Serial.println(h,0);
  }
}

void clearLogs() {
  // limpa apenas a área de logs; preserva setores persistidos
  for (int i=0;i<LOG_EEPROM_SIZE;i++) EEPROM.update(i, 0xFF);
  headIndex = 0;
  Serial.println(F("Logs limpos."));
}

void setup() {
  pinMode(PIR_PIN, INPUT);
  pinMode(PIR_PIN2, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  lcd.begin(16, 2);
  dht.begin();
  Serial.begin(9600);
  // Carrega setores persistidos (se existentes)
  loadSetores();
  lcd.print("SmartMottu IA");
  delay(1200);
  lcd.clear();
  Serial.println(F("Digite HELP para ver comandos."));
  logEvent(EVT_BOOT, -1, -1, NAN, NAN);
}

void updateMetrics(int v1, int v2, float t, float h) {
  samples++;
  if (!isnan(t)) {
    tempSum += t;
    if (t > tempMax) tempMax = t;
  }
  if (!isnan(h)) {
    humSum += h;
    if (h > humMax) humMax = h;
  }
  unsigned long now = millis();
  if (v1) {
    if (v1OccupiedSince == 0) v1OccupiedSince = now;
  } else if (v1OccupiedSince) {
    v1TotalMs += (now - v1OccupiedSince);
    v1OccupiedSince = 0;
  }
  if (v2) {
    if (v2OccupiedSince == 0) v2OccupiedSince = now;
  } else if (v2OccupiedSince) {
    v2TotalMs += (now - v2OccupiedSince);
    v2OccupiedSince = 0;
  }
}

void handleAlerts(int v1, int v2, float t, float h) {
  if (maintenanceMode) {
    // Em manutenção, sem alertas
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    lastAlertMsg = "MANUTENCAO";
    lastAlertState = false;
    return;
  }
  bool alert = false;
  String msg = "OK";
  if (!v1 && v2) {
    alert = true;
    msg = "Moto na vaga errada";
  } else if (!v1 && !v2 && (motoVaga1.length()>0 || motoVaga2.length()>0)) {
    // só alerta se nenhum ocupado por mais que graceMs
    if (lastAnyOccupiedMs > 0 && (millis() - lastAnyOccupiedMs) > graceMs) {
      alert = true;
      msg = "Moto desaparecida";
    }
  }
  digitalWrite(LED_PIN, alert ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, alert ? HIGH : LOW);
  if (alert != lastAlertState) {
    if (alert) { Serial.print(F("ALERTA: ")); Serial.println(msg); }
    logEvent(EVT_ALERT, v1, v2, t, h);
    lastAlertState = alert;
  }
  lastAlertMsg = alert ? msg : "OK";
}

void printStatusLine(int v1, int v2, float t, float h, const char* alert) {
  Serial.print(F("STATUS v1=")); Serial.print(v1);
  Serial.print(F(" v2=")); Serial.print(v2);
  Serial.print(F(" T=")); Serial.print(t,1);
  Serial.print(F(" H=")); Serial.print(h,0);
  Serial.print(F(" A=")); Serial.println(alert);
}

void printStatusHuman(int v1, int v2, float t, float h, const char* /*alert*/) {
  Serial.print(F("STATUS Vaga 1: "));
  if (v1) {
    Serial.print(F("Ocupada"));
    if (motoVaga1.length()) { Serial.print(F(" (")); Serial.print(motoVaga1); Serial.print(F(")")); }
  } else {
    Serial.print(F("Livre"));
  }
  Serial.print(F(" | Setor1: "));
  Serial.print(setorVaga1);
  Serial.print(F(" | Vaga 2: "));
  if (v2) {
    Serial.print(F("Ocupada"));
    if (motoVaga2.length()) { Serial.print(F(" (")); Serial.print(motoVaga2); Serial.print(F(")")); }
  } else {
    Serial.print(F("Livre"));
  }
  Serial.print(F(" | Setor2: "));
  Serial.print(setorVaga2);
  Serial.print(F(" | Temperatura: "));
  Serial.print(t, 1);
  Serial.println(F(" C"));
}

void printMetrics() {
  // Ocupação acumulada em segundos
  unsigned long now = millis();
  unsigned long v1Ms = v1TotalMs + (v1OccupiedSince ? (now - v1OccupiedSince) : 0);
  unsigned long v2Ms = v2TotalMs + (v2OccupiedSince ? (now - v2OccupiedSince) : 0);
  float v1Sec = v1Ms / 1000.0;
  float v2Sec = v2Ms / 1000.0;
  float tAvg = samples ? (tempSum / samples) : 0;
  float hAvg = samples ? (humSum / samples) : 0;
  Serial.print(F("METRICS v1_sec=")); Serial.print(v1Sec, 1);
  Serial.print(F(" v2_sec=")); Serial.print(v2Sec, 1);
  Serial.print(F(" Tavg=")); Serial.print(tAvg, 1);
  Serial.print(F(" Tmax=")); Serial.print(tempMax, 1);
  Serial.print(F(" Havg=")); Serial.print(hAvg, 1);
  Serial.print(F(" Hmax=")); Serial.println(humMax, 0);
}

void updateLcd(int v1, int v2, float t, float h) {
  unsigned long now = millis();
  if (now - lastLcdSwitch > 3000) {
    lcdPage = !lcdPage;
    lastLcdSwitch = now;
  }
  lcd.clear();
  if (!lcdPage) {
    lcd.setCursor(0,0); lcd.print("V1:"); lcd.print(v1?"Ocupada":"Livre  ");
    lcd.setCursor(8,0); lcd.print("V2:"); lcd.print(v2?"Ocupada":"Livre  ");
    lcd.setCursor(0,1); lcd.print("S1:"); lcd.print(setorVaga1.length()?setorVaga1.substring(0,5):"-");
    lcd.setCursor(8,1); lcd.print("S2:"); lcd.print(setorVaga2.length()?setorVaga2.substring(0,5):"-");
  } else {
    lcd.setCursor(0,0); lcd.print("T:"); lcd.print(t,1); lcd.print("C ");
    // Removida umidade na tela
    lcd.setCursor(0,1); lcd.print("M1:"); lcd.print(motoVaga1.length()?motoVaga1.substring(0,5):"-");
    lcd.setCursor(8,1); lcd.print("M2:"); lcd.print(motoVaga2.length()?motoVaga2.substring(0,5):"-");
  }
}

void processSerial(int v1, int v2, float t, float h) {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "HELP") { printHelp(); return; }
  if (cmd.startsWith("ASSOC VAGA1:")) {
    motoVaga1 = cmd.substring(12); // após 'ASSOC VAGA1:'
    Serial.print(F("Vaga1 associada a: ")); Serial.println(motoVaga1);
    logEvent(EVT_ASSOC, v1, v2, t, h);
    return;
  }
  if (cmd.startsWith("ASSOC VAGA2:")) {
    motoVaga2 = cmd.substring(12);
    Serial.print(F("Vaga2 associada a: ")); Serial.println(motoVaga2);
    logEvent(EVT_ASSOC, v1, v2, t, h);
    return;
  }
  if (cmd == "STATUS") {
    // Imprime linha descritiva para humanos
    printStatusHuman(v1, v2, t, h, lastAlertMsg.length()?lastAlertMsg.c_str():"Sem alerta");
    logEvent(EVT_STATUS, v1, v2, t, h);
    printMetrics();
    return;
  }
  if (cmd == "SET SETOR DEFAULTS") {
    setorVaga1 = String(DEFAULT_SETOR_V1); setorVaga1.toUpperCase();
    setorVaga2 = String(DEFAULT_SETOR_V2); setorVaga2.toUpperCase();
    saveSetores();
    Serial.print(F("Setores restaurados: V1=")); Serial.print(setorVaga1);
    Serial.print(F(" V2=")); Serial.println(setorVaga2);
    return;
  }
  if (cmd.startsWith("SET SETOR VAGA1:")) {
    setorVaga1 = cmd.substring(16); // aceita opcional espaco apos ':'
    setorVaga1.trim();
    Serial.print(F("Setor da Vaga1: ")); Serial.println(setorVaga1);
    saveSetores();
    return;
  }
  if (cmd.startsWith("SET SETOR VAGA2:")) {
    setorVaga2 = cmd.substring(16);
    setorVaga2.trim();
    Serial.print(F("Setor da Vaga2: ")); Serial.println(setorVaga2);
    saveSetores();
    return;
  }
  if (cmd.startsWith("SCAN VAGA1:")) {
    String id = cmd.substring(12);
    if (motoVaga1.length() == 0) {
      motoVaga1 = id; Serial.print(F("SCAN Vaga1 associou: ")); Serial.println(id);
    } else if (motoVaga1 != id) {
      Serial.print(F("ALERTA: SCAN divergente Vaga1. Esperado ")); Serial.print(motoVaga1); Serial.print(F(", lido ")); Serial.println(id);
      lastAlertMsg = "SCAN divergente V1";
    } else {
      Serial.println(F("SCAN Vaga1 OK"));
    }
    return;
  }
  if (cmd.startsWith("SCAN VAGA2:")) {
    String id = cmd.substring(12);
    if (motoVaga2.length() == 0) {
      motoVaga2 = id; Serial.print(F("SCAN Vaga2 associou: ")); Serial.println(id);
    } else if (motoVaga2 != id) {
      Serial.print(F("ALERTA: SCAN divergente Vaga2. Esperado ")); Serial.print(motoVaga2); Serial.print(F(", lido ")); Serial.println(id);
      lastAlertMsg = "SCAN divergente V2";
    } else {
      Serial.println(F("SCAN Vaga2 OK"));
    }
    return;
  }
  if (cmd == "MODE MAINT ON" || cmd == "MODE MAINTENANCE ON") { maintenanceMode = true; Serial.println(F("Modo manutencao: ON")); return; }
  if (cmd == "MODE MAINT OFF" || cmd == "MODE MAINTENANCE OFF") { maintenanceMode = false; Serial.println(F("Modo manutencao: OFF")); return; }
  if (cmd == "OUTPUT HUMAN" || cmd == "OUTPUT MODE HUMAN") { outputHuman = true; Serial.println(F("Output: HUMAN")); return; }
  if (cmd == "OUTPUT MACHINE" || cmd == "OUTPUT MODE MACHINE") { outputHuman = false; Serial.println(F("Output: MACHINE")); return; }
  if (cmd == "LOG DUMP") { dumpLogs(); return; }
  if (cmd == "LOG CLEAR") { clearLogs(); return; }
  if (cmd.startsWith("SET GRACE ")) {
    long s = cmd.substring(10).toInt();
    if (s < 0) s = 0;
    graceMs = (unsigned long)s * 1000UL;
    Serial.print(F("Grace time: ")); Serial.print(s); Serial.println(F("s"));
    return;
  }
  if (cmd == "SET BUZZER ON") { digitalWrite(BUZZER_PIN, HIGH); return; }
  if (cmd == "SET BUZZER OFF") { digitalWrite(BUZZER_PIN, LOW); return; }
  if (cmd == "SET LED ON") { digitalWrite(LED_PIN, HIGH); return; }
  if (cmd == "SET LED OFF") { digitalWrite(LED_PIN, LOW); return; }

  Serial.println(F("Comando não reconhecido. Digite HELP."));
}

void loop() {
  int rawV1 = digitalRead(PIR_PIN);
  int rawV2 = digitalRead(PIR_PIN2);
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) { t = 0; h = 0; }

  unsigned long now = millis();
  // Ocupação com retenção: segura como ocupada por holdMs após último HIGH
  if (rawV1) { lastDetectV1Ms = now; }
  if (rawV2) { lastDetectV2Ms = now; }
  int v1 = (rawV1 || (now - lastDetectV1Ms) < holdMs) ? 1 : 0;
  int v2 = (rawV2 || (now - lastDetectV2Ms) < holdMs) ? 1 : 0;

  // Mudança de estado -> loga uma vez só
  if (lastV1 != v1) {
    logEvent(EVT_V1_CHANGE, v1, v2, t, h);
    lastV1 = v1;
  }
  if (lastV2 != v2) {
    logEvent(EVT_V2_CHANGE, v1, v2, t, h);
    lastV2 = v2;
  }

  // marca quando alguma vaga esteve ocupada pela última vez
  if (v1 || v2) {
    lastAnyOccupiedMs = millis();
  }

  updateMetrics(v1, v2, t, h);
  handleAlerts(v1, v2, t, h);
  updateLcd(v1, v2, t, h);
  processSerial(v1, v2, t, h);

  // Linha resumida para o dashboard/CSV a cada iteração
  if (outputHuman) {
    printStatusHuman(v1, v2, t, h, "");
  } else {
    printStatusLine(v1, v2, t, h, lastAlertMsg.length()?lastAlertMsg.c_str():"-");
  }

  delay(500);
}
