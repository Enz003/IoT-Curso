/*
  LAB 5 — Ciudad Inteligente Multi-Nodo
  NODO-PLAZA: Plaza céntrica de Asunción
  Hardware: ESP32 + DHT22 + LDR + Potenciómetro (ruido)
  Protocolo: MQTT → ThingsBoard
*/

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Arduino.h>

// ─── CREDENCIALES — editar antes de compilar ──────────────
#define WIFI_SSID  "Wokwi-GUEST"
#define WIFI_PASS  ""
#define TB_TOKEN    "zOSe2RlutmQddromnbKp"

// ─── ThingsBoard Cloud ─────────────────────────────────────
#define TB_HOST     "mqtt.thingsboard.cloud"
#define TB_PORT     1883

// ─── Pines ────────────────────────────────────────────────────────────────
#define DHT_PIN       4
#define DHT_TYPE      DHT22
#define LDR_PIN       34    // ADC1_CH6
#define POT_PIN       35    // ADC1_CH7 → simula ruido 0-120 dB
#define LED_VERDE     25    // calidad BUENA
#define LED_AMARILLO  26    // calidad ACEPTABLE
#define LED_ROJO      27    // calidad MALA

// ─── Umbrales Nodo-Plaza (entorno tranquilo) ───────────────────────────────
// La plaza tiene menor ruido que la avenida, por eso usamos un umbral
// de ruido ligeramente más estricto (75 dB vs 85 dB en avenida).
// Los umbrales de temperatura y humedad son iguales en ambos nodos
// ya que la condición térmica no depende del tipo de vía.
const float TEMP_CRITICA   = 35.0;    // °C — igual para ambos nodos
const float HUM_ALERTA     = 85.0;    // %  — igual para ambos nodos
const float RUIDO_PLAZA_OK = 60.0;    // dB — plaza: ambiente tranquilo
const float RUIDO_PLAZA_AL = 75.0;    // dB — plaza: umbral alerta
const int   LUX_MIN        = 100;     // lux mínimo aceptable
const int   LUX_MAX        = 80000;   // lux máximo aceptable (mediodía pleno)

const char* UBICACION      = "Plaza Central - Asuncion";
const long  INTERVALO_MS   = 5000;    // publicar cada 5 segundos

// ─── Objetos ──────────────────────────────────────────────────────────────
DHT        dht(DHT_PIN, DHT_TYPE);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastPublish = 0;
long          seqNumber   = 0;

// ─── Función: conectar WiFi ───────────────────────────────────────────────
void conectarWiFi() {
  Serial.printf("\n[WiFi] Conectando a %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
}

// ─── Función: conectar MQTT a ThingsBoard ─────────────────────────────────
void conectarMQTT() {
  mqtt.setServer(TB_HOST, TB_PORT);
  while (!mqtt.connected()) {
    Serial.print("[MQTT] Conectando a ThingsBoard...");
    if (mqtt.connect("NodoPlaza", TB_TOKEN, nullptr)) {
      Serial.println(" OK");
    } else {
      Serial.printf(" Error: %d. Reintentando en 5s\n", mqtt.state());
      delay(5000);
    }
  }
}

// ─── Función: mapear ADC a Lux (0-100000 lux) ─────────────────────────────
float adc_a_lux(int adc) {
  // ADC 12-bit (0-4095). LDR: mayor luz → menor resistencia → mayor tensión
  return map(adc, 0, 4095, 0, 100000);
}

// ─── Función: mapear ADC a dB (0-120 dB) ─────────────────────────────────
float adc_a_dB(int adc) {
  return (float)adc * 120.0f / 4095.0f;
}

// ─── Función: clasificar calidad ambiental ────────────────────────────────
// Lógica combinada: una condición crítica individual = MALA,
// dos condiciones de alerta simultáneas = MALA,
// una condición de alerta = ACEPTABLE,
// sin alertas = BUENA.
String clasificarCalidad(float temp, float hum, float lux, float ruido) {
  int alertas = 0;
  bool critico = false;

  // Condiciones críticas individuales
  if (temp >= TEMP_CRITICA)        critico = true;
  if (ruido >= RUIDO_PLAZA_AL)     critico = true;  // umbral plaza

  // Condiciones de alerta
  if (temp >= 30.0 && temp < TEMP_CRITICA)           alertas++;
  if (hum >= HUM_ALERTA)                             alertas++;
  if (lux < LUX_MIN || lux > LUX_MAX)               alertas++;
  if (ruido >= RUIDO_PLAZA_OK && ruido < RUIDO_PLAZA_AL) alertas++;

  if (critico || alertas >= 2) return "MALA";
  if (alertas == 1)             return "ACEPTABLE";
  return "BUENA";
}

// ─── Función: actualizar LEDs ─────────────────────────────────────────────
void actualizarLEDs(const String& calidad) {
  digitalWrite(LED_VERDE,    calidad == "BUENA"      ? HIGH : LOW);
  digitalWrite(LED_AMARILLO, calidad == "ACEPTABLE"  ? HIGH : LOW);
  digitalWrite(LED_ROJO,     calidad == "MALA"       ? HIGH : LOW);
}

// ─── setup ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_VERDE,    OUTPUT);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO,     OUTPUT);

  dht.begin();
  conectarWiFi();
  conectarMQTT();

  Serial.println("\n===== NODO-PLAZA INICIADO =====");
  Serial.printf("ThingsBoard: %s:%d\n", TB_HOST, TB_PORT);
  Serial.printf("Intervalo: %ld ms\n\n", INTERVALO_MS);
}

// ─── loop ─────────────────────────────────────────────────────────────────
void loop() {
  if (!mqtt.connected()) conectarMQTT();
  mqtt.loop();

  unsigned long ahora = millis();
  if (ahora - lastPublish >= INTERVALO_MS) {
    lastPublish = ahora;
    seqNumber++;

    // ── Leer sensores ─────────────────────────────────────────────────────
    float temperatura = dht.readTemperature();
    float humedad     = dht.readHumidity();

    if (isnan(temperatura) || isnan(humedad)) {
      Serial.println("[DHT] Error de lectura, reintentando...");
      return;
    }

    int   adc_ldr  = analogRead(LDR_PIN);
    int   adc_pot  = analogRead(POT_PIN);
    float lux      = adc_a_lux(adc_ldr);
    float ruido_db = adc_a_dB(adc_pot);

    String calidad = clasificarCalidad(temperatura, humedad, lux, ruido_db);
    actualizarLEDs(calidad);

    // ── Construir JSON para ThingsBoard ───────────────────────────────────
    char payload[300];
    snprintf(payload, sizeof(payload),
      "{"
        "\"temperature\":%.2f,"
        "\"humidity\":%.2f,"
        "\"lux\":%.1f,"
        "\"noise_db\":%.1f,"
        "\"calidad_ambiental\":\"%s\","
        "\"ubicacion\":\"%s\","
        "\"seq\":%ld"
      "}",
      temperatura, humedad, lux, ruido_db,
      calidad.c_str(), UBICACION, seqNumber
    );

    // ── Publicar ──────────────────────────────────────────────────────────
    bool ok = mqtt.publish("v1/devices/me/telemetry", payload);

    // ── Log en Serial Monitor ─────────────────────────────────────────────
    Serial.printf("[SEQ %ld] %s\n", seqNumber, ok ? "OK" : "FAIL");
    Serial.printf("  Temp: %.2f°C  |  Hum: %.2f%%\n", temperatura, humedad);
    Serial.printf("  Lux:  %.1f   |  Ruido: %.1f dB\n", lux, ruido_db);
    Serial.printf("  Calidad: %s\n", calidad.c_str());
    Serial.printf("  JSON: %s\n\n", payload);
  }
}
