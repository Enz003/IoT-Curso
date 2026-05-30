

#include <DHT.h>

// ─── PINES ────────────────────────────────────────────────
#define DHTPIN      4
#define DHTTYPE     DHT22
#define POT_PIN     34
#define LED_GREEN   25
#define LED_YELLOW  26
#define LED_RED     27
#define BUZZER_PIN  33

// ─── CONSTANTES ───────────────────────────────────────────
#define SAMPLE_SIZE       5       // Tamaño media móvil
#define TEMP_WARN_THRESH  28.0    // °C umbral WARNING
#define TEMP_FAULT_THRESH 32.0    // °C umbral FAULT
#define LOAD_WARN_THRESH  60.0    // % umbral WARNING
#define LOAD_FAULT_THRESH 80.0    // % umbral FAULT
#define LOOP_DELAY_MS     3000    // Intervalo de lectura (ms)
#define BUZZER_FREQ       1000    // Frecuencia buzzer (Hz)

// ─── OBJETOS ──────────────────────────────────────────────
DHT dht(DHTPIN, DHTTYPE);

// ─── VARIABLES GLOBALES ───────────────────────────────────
float loadSamples[SAMPLE_SIZE] = {0};
int   sampleIndex  = 0;
bool  bufferFilled = false;
int   alertCount   = 0;
unsigned long lastMillis = 0;

// ─── ENUMERACIÓN DE ESTADOS ───────────────────────────────
enum SystemState { STATE_OK, STATE_WARNING, STATE_FAULT };

// =====================================================================
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  setAllLEDs(LOW, LOW, LOW);
  noTone(BUZZER_PIN);

  Serial.println("=== Nodo IIoT - Monitor Bomba de Agua ===");
  delay(2000);
}

// =====================================================================
void loop() {
  unsigned long now = millis();
  if (now - lastMillis < LOOP_DELAY_MS) return;
  lastMillis = now;

  // 1. Lectura DHT22
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("{\"error\":\"DHT22 no responde\"}");
    return;
  }

  // 2. Lectura potenciómetro (ADC 12-bit: 0-4095)
  int   rawADC  = analogRead(POT_PIN);
  float loadRaw = (rawADC / 4095.0) * 100.0;

  // 3. Media móvil de 5 muestras
  loadSamples[sampleIndex] = loadRaw;
  sampleIndex = (sampleIndex + 1) % SAMPLE_SIZE;
  if (sampleIndex == 0) bufferFilled = true;

  int   samplesUsed  = bufferFilled ? SAMPLE_SIZE : sampleIndex;
  float loadFiltered = movingAverage(samplesUsed);

  // 4. Lógica de estados
  SystemState state = evaluateState(temperature, loadFiltered);

  // 5. Salidas (LEDs + Buzzer)
  applyOutputs(state);

  // 6. Contador de alertas
  if (state != STATE_OK) alertCount++;

  // 7. Telemetría JSON
  printJSON(temperature, humidity, loadRaw, loadFiltered, samplesUsed, state);
}

// =====================================================================
float movingAverage(int n) {
  float sum = 0;
  for (int i = 0; i < n; i++) sum += loadSamples[i];
  return sum / n;
}

SystemState evaluateState(float temp, float load) {
  if (temp > TEMP_FAULT_THRESH || load > LOAD_FAULT_THRESH) return STATE_FAULT;
  if (temp > TEMP_WARN_THRESH  || load > LOAD_WARN_THRESH)  return STATE_WARNING;
  return STATE_OK;
}

void applyOutputs(SystemState state) {
  switch (state) {
    case STATE_OK:
      setAllLEDs(HIGH, LOW, LOW);
      noTone(BUZZER_PIN);
      break;
    case STATE_WARNING:
      setAllLEDs(LOW, HIGH, LOW);
      noTone(BUZZER_PIN);
      break;
    case STATE_FAULT:
      setAllLEDs(LOW, LOW, HIGH);
      tone(BUZZER_PIN, BUZZER_FREQ);
      break;
  }
}

void setAllLEDs(int green, int yellow, int red) {
  digitalWrite(LED_GREEN,  green);
  digitalWrite(LED_YELLOW, yellow);
  digitalWrite(LED_RED,    red);
}

const char* stateToString(SystemState state) {
  switch (state) {
    case STATE_OK:      return "OK";
    case STATE_WARNING: return "WARNING";
    case STATE_FAULT:   return "FAULT";
    default:            return "UNKNOWN";
  }
}

void printJSON(float temp, float hum, float loadRaw,
               float loadFilt, int samplesUsed, SystemState state) {
  char buffer[300];
  sprintf(buffer,
    "{\"temp_c\":%.1f,\"humidity_pct\":%.1f,"
    "\"load_raw_pct\":%.1f,\"load_pct\":%.1f,\"samples_used\":%d,"
    "\"state\":\"%s\",\"fault_temp\":%s,\"fault_load\":%s,\"alert_count\":%d}",
    temp, hum, loadRaw, loadFilt, samplesUsed,
    stateToString(state),
    (temp     > TEMP_FAULT_THRESH) ? "true" : "false",
    (loadFilt > LOAD_FAULT_THRESH) ? "true" : "false",
    alertCount
  );
  Serial.println(buffer);
}