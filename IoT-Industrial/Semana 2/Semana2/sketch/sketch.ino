#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ─── CREDENCIALES —
#define WIFI_SSID  "Wokwi-GUEST"
#define WIFI_PASS  ""
#define TB_TOKEN    "iGauXxnV3cnJCl9Pyoof"

// ─── ThingsBoard Cloud ─────────────────────────────────────
#define TB_HOST     "mqtt.thingsboard.cloud"
#define TB_PORT     1883

// ─── TOPICS MQTT ──────────────────────────────────────────
#define TOPIC_TELEMETRY   "v1/devices/me/telemetry"
#define TOPIC_RPC_SUB     "v1/devices/me/rpc/request/+"
// La respuesta usa TOPIC_RPC_RESP + requestId (construido dinámicamente)
#define TOPIC_RPC_RESP    "v1/devices/me/rpc/response/"

// ─── PINES ────────────────────────────────────────────────
#define DHTPIN      4
#define DHTTYPE     DHT22
#define POT_PIN     34   // Simula corriente del ventilador
#define LED_GREEN   25
#define LED_YELLOW  26
#define LED_RED     27
#define LED_BLUE    33   // Ventilador de emergencia (fanOn)

// ─── CONSTANTES ───────────────────────────────────────────
#define SAMPLE_SIZE       5
#define TEMP_WARN_DEFAULT 28.0f   // °C — ajustable por setAlarm
#define TEMP_FAULT_OFFSET  4.0f   // FAULT = WARN + offset
#define LOAD_WARN_THRESH  60.0f
#define LOAD_FAULT_THRESH 80.0f
#define LOOP_DELAY_MS     3000
#define MQTT_RECONNECT_MS 5000
#define BUZZER_PIN        -1      // Sin buzzer en LAB 2 (LED azul = actuador)

// ─── OBJETOS ──────────────────────────────────────────────
DHT         dht(DHTPIN, DHTTYPE);
WiFiClient  wifiClient;
PubSubClient mqtt(wifiClient);

// ─── ESTADO GLOBAL ────────────────────────────────────────
float loadSamples[SAMPLE_SIZE] = {0};
int   sampleIndex  = 0;
bool  bufferFilled = false;
int   alertCount   = 0;

float alarmThreshold = TEMP_WARN_DEFAULT;  // Umbral dinámico (setAlarm)
bool  fanOn          = false;               // Estado LED azul (fanOn)

unsigned long lastTelemetry  = 0;
unsigned long lastReconnect  = 0;

enum SystemState { STATE_OK, STATE_WARNING, STATE_FAULT };

// ─── DECLARACIONES ────────────────────────────────────────
void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishTelemetry();
void handleRPC(const char* topic, const char* payload, unsigned int length);
void applyOutputs(SystemState state);
void setAllLEDs(int g, int y, int r);
float movingAverage(int n);
SystemState evaluateState(float temp, float load);
const char* stateToString(SystemState s);

// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Pines de salida
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_BLUE,   OUTPUT);
  setAllLEDs(LOW, LOW, LOW);
  digitalWrite(LED_BLUE, LOW);

  dht.begin();

  Serial.println("\n=== LAB 2 — Nodo IIoT con MQTT bidireccional ===");
  connectWiFi();

  mqtt.setServer(TB_HOST, TB_PORT);
  mqtt.setCallback(mqttCallback);
  // Aumentar buffer para JSON grandes en callback
  mqtt.setBufferSize(512);

  connectMQTT();
}

// =====================================================================
void loop() {
  // Mantener conexión MQTT activa
  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastReconnect > MQTT_RECONNECT_MS) {
      lastReconnect = now;
      connectMQTT();
    }
  }
  mqtt.loop();   // Procesar mensajes entrantes (RPC)

  // Telemetría cada LOOP_DELAY_MS
  unsigned long now = millis();
  if (now - lastTelemetry >= LOOP_DELAY_MS) {
    lastTelemetry = now;
    publishTelemetry();
  }
}

// =====================================================================
//  WiFi
// =====================================================================
void connectWiFi() {
  Serial.printf("[WiFi] Conectando a %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] Conectado — IP: %s\n", WiFi.localIP().toString().c_str());
}

// =====================================================================
//  MQTT — conexión y suscripción
// =====================================================================
void connectMQTT() {
  Serial.printf("[MQTT] Conectando a %s:%d ... ", TB_HOST, TB_PORT);
  // ThingsBoard usa el token como username; password y clientId vacíos
  if (mqtt.connect("ESP32_Lab2", TB_TOKEN, NULL)) {
    Serial.println("OK");
    mqtt.subscribe(TOPIC_RPC_SUB);
    Serial.printf("[MQTT] Suscrito a: %s\n", TOPIC_RPC_SUB);
  } else {
    Serial.printf("FALLÓ (rc=%d) — reintentando en %d s\n",
                  mqtt.state(), MQTT_RECONNECT_MS / 1000);
  }
}

// =====================================================================
//  Callback MQTT — se invoca por mqtt.loop() al recibir un mensaje
// =====================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convertir payload a string terminada en null
  char msg[length + 1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.printf("\n[RPC] Topic: %s\n[RPC] Payload: %s\n", topic, msg);

  handleRPC(topic, msg, length);
}

// =====================================================================
//  Manejo de comandos RPC
// =====================================================================
void handleRPC(const char* topic, const char* payload, unsigned int length) {
  // Extraer requestId del topic: "v1/devices/me/rpc/request/{id}"
  String topicStr(topic);
  int lastSlash = topicStr.lastIndexOf('/');
  String requestId = topicStr.substring(lastSlash + 1);

  // Parsear JSON del comando
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.printf("[RPC] Error JSON: %s\n", err.c_str());
    return;
  }

  const char* method = doc["method"] | "";
  Serial.printf("[RPC] Método: %s\n", method);

  // ── Comando: setAlarm ────────────────────────────────────
  if (strcmp(method, "setAlarm") == 0) {
    float newThreshold = doc["params"]["value"] | alarmThreshold;
    alarmThreshold = newThreshold;
    Serial.printf("[RPC] Nuevo umbral de alarma: %.1f °C\n", alarmThreshold);
  }
  // ── Comando: fanOn ───────────────────────────────────────
  else if (strcmp(method, "fanOn") == 0) {
    fanOn = true;
    digitalWrite(LED_BLUE, HIGH);
    Serial.println("[RPC] Ventilador de emergencia ENCENDIDO (LED azul)");
  }
  // ── Comando: fanOff (extensión opcional) ─────────────────
  else if (strcmp(method, "fanOff") == 0) {
    fanOn = false;
    digitalWrite(LED_BLUE, LOW);
    Serial.println("[RPC] Ventilador de emergencia APAGADO");
  }
  else {
    Serial.printf("[RPC] Método desconocido: %s\n", method);
  }

  // ── Responder en rpc/response/{requestId} ────────────────
  String responseTopic = String(TOPIC_RPC_RESP) + requestId;
  const char* responsePayload = "{\"result\":\"OK\"}";
  mqtt.publish(responseTopic.c_str(), responsePayload);
  Serial.printf("[RPC] Respuesta enviada → %s : %s\n",
                responseTopic.c_str(), responsePayload);
}

// =====================================================================
//  Telemetría — lectura y publicación
// =====================================================================
void publishTelemetry() {
  // 1. DHT22
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT] Error: sensor no responde");
    return;
  }

  // 2. Potenciómetro en GPIO34 — simula corriente del ventilador
  int   rawADC  = analogRead(POT_PIN);
  float loadRaw = (rawADC / 4095.0f) * 100.0f;

  // 3. Media móvil de 5 muestras
  loadSamples[sampleIndex] = loadRaw;
  sampleIndex = (sampleIndex + 1) % SAMPLE_SIZE;
  if (sampleIndex == 0) bufferFilled = true;
  int   samplesUsed  = bufferFilled ? SAMPLE_SIZE : sampleIndex;
  float loadFiltered = movingAverage(samplesUsed);

  // 4. Evaluar estado (usa umbral dinámico)
  SystemState state = evaluateState(temperature, loadFiltered);
  if (state != STATE_OK) alertCount++;

  // 5. Actualizar actuadores físicos
  applyOutputs(state);
  // LED azul se maneja solo desde fanOn, no desde applyOutputs

  // 6. Construir JSON de telemetría
  StaticJsonDocument<384> doc;
  doc["temp_c"]        = serialized(String(temperature, 1));
  doc["humidity_pct"]  = serialized(String(humidity, 1));
  doc["load_raw_pct"]  = serialized(String(loadRaw, 1));
  doc["load_pct"]      = serialized(String(loadFiltered, 1));
  doc["samples_used"]  = samplesUsed;
  doc["state"]         = stateToString(state);
  doc["fault_temp"]    = (temperature > (alarmThreshold + TEMP_FAULT_OFFSET));
  doc["fault_load"]    = (loadFiltered > LOAD_FAULT_THRESH);
  doc["alert_count"]   = alertCount;
  doc["alarm_thresh"]  = serialized(String(alarmThreshold, 1));
  doc["fan_on"]        = fanOn;

  char buffer[384];
  size_t n = serializeJson(doc, buffer);

  // 7. Publicar en ThingsBoard
  bool ok = mqtt.publish(TOPIC_TELEMETRY, buffer, n);

  // 8. Log en Serial Monitor
  Serial.printf("[TEL] %s → %s\n", ok ? "OK" : "FAIL", buffer);
}

// =====================================================================
//  Lógica de estados — usa umbral dinámico
// =====================================================================
SystemState evaluateState(float temp, float load) {
  float faultThresh = alarmThreshold + TEMP_FAULT_OFFSET;
  if (temp > faultThresh      || load > LOAD_FAULT_THRESH) return STATE_FAULT;
  if (temp > alarmThreshold   || load > LOAD_WARN_THRESH)  return STATE_WARNING;
  return STATE_OK;
}

// =====================================================================
//  Salidas físicas (LEDs de estado)
//  El LED azul (GPIO33) NO se toca aquí — solo desde fanOn RPC
// =====================================================================
void applyOutputs(SystemState state) {
  switch (state) {
    case STATE_OK:
      setAllLEDs(HIGH, LOW, LOW);
      break;
    case STATE_WARNING:
      setAllLEDs(LOW, HIGH, LOW);
      break;
    case STATE_FAULT:
      setAllLEDs(LOW, LOW, HIGH);
      break;
  }
}

void setAllLEDs(int g, int y, int r) {
  digitalWrite(LED_GREEN,  g);
  digitalWrite(LED_YELLOW, y);
  digitalWrite(LED_RED,    r);
}

// =====================================================================
//  Utilidades
// =====================================================================
float movingAverage(int n) {
  float sum = 0;
  for (int i = 0; i < n; i++) sum += loadSamples[i];
  return (n > 0) ? sum / n : 0;
}

const char* stateToString(SystemState s) {
  switch (s) {
    case STATE_OK:      return "OK";
    case STATE_WARNING: return "WARNING";
    case STATE_FAULT:   return "FAULT";
    default:            return "UNKNOWN";
  }
}
