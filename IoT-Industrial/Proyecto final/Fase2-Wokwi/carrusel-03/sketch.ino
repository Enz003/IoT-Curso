/* ============================================================================
 *  Smart Carousel Parking — Gateway ESP32 (carrusel-01)
 *  Broker: HiveMQ Cloud (TLS, puerto 8883)
 *  Payload: JSON anidado con objetos "slots" y "motor"
 *  Topic:   carrusel/carrusel-01/telemetria
 * ============================================================================
 *
 *  Convergencia IT/OT (Semana 4): el ESP32 actúa como GATEWAY DE BORDE que
 *  solo LEE las señales del PLC (no intrusivo). En Wokwi:
 *    - 8 INTERRUPTORES emulan la ocupación de cada slot (ON=ocupado, OFF=libre).
 *    - 3 POTENCIÓMETROS emulan registros del motor: temperatura, vibración
 *      y corriente.
 * ========================================================================== */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ---------------------------------------------------------------------------
// WiFi (red abierta de la simulación Wokwi)
// ---------------------------------------------------------------------------
const char* ssid     = "Wokwi-GUEST";
const char* password = "";

// ---------------------------------------------------------------------------
// HiveMQ Cloud
// ---------------------------------------------------------------------------
const char* mqtt_server = "bd6de059ad85437bb98161dc2061832b.s1.eu.hivemq.cloud";
const char* mqtt_user   = "parking-carrusel";
const char* mqtt_pass   = "Carrusel123";
const int   mqtt_port   = 8883;

const char* DEVICE_ID = "carrusel-03";
const char* TOPIC     = "carrusel/carrusel-01/telemetria";

// ---------------------------------------------------------------------------
// Pines de los 8 interruptores de slot (ocupación)
// ---------------------------------------------------------------------------
const int NUM_SLOTS = 8;
const int SLOT_PINS[NUM_SLOTS] = {13, 4, 14, 27, 26, 25, 33, 23};

// ---------------------------------------------------------------------------
// Pines analógicos del motor (potenciómetros, todos ADC1: ok con WiFi)
// ---------------------------------------------------------------------------
const int PIN_TEMP = 34;
const int PIN_VIB  = 35;
const int PIN_CORR = 32;

// ---------------------------------------------------------------------------
// Umbrales de clasificación del motor (mantenimiento predictivo)
// ---------------------------------------------------------------------------
const float TEMP_ADV = 60.0, TEMP_FALLA = 75.0;   // grados C
const float VIB_ADV  = 4.5,  VIB_FALLA  = 7.1;    // mm/s (ISO 10816)
const float CORR_ADV = 12.0, CORR_FALLA = 16.0;   // Amperios

// ---------------------------------------------------------------------------
// Cliente MQTT con TLS
// ---------------------------------------------------------------------------
WiFiClientSecure espClient;
PubSubClient     client(espClient);

bool slotOcupado[NUM_SLOTS] = {false};
unsigned long lastPublish = 0;
int  seq = 0;

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
void connectWiFi() {
  Serial.print("Conectando a WiFi ");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("  OK  IP: ");
  Serial.println(WiFi.localIP());
}

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------
void connectMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(768);

  while (!client.connected()) {
    Serial.print("Conectando a HiveMQ (");
    Serial.print(mqtt_server);
    Serial.print(")...");

    if (client.connect(DEVICE_ID, mqtt_user, mqtt_pass)) {
      Serial.println("  CONECTADO");
    } else {
      Serial.print("  fallo rc=");
      Serial.print(client.state());
      Serial.println("  Reintento en 2s");
      delay(2000);
    }
  }
}

// ---------------------------------------------------------------------------
// Helpers de clasificación
// ---------------------------------------------------------------------------
int clasificar(float v, float adv, float falla) {
  if (v >= falla) return 2;
  if (v >= adv)   return 1;
  return 0;
}

const char* nivelTexto(int n) {
  if (n == 2) return "FALLA";
  if (n == 1) return "ADVERTENCIA";
  return "NORMAL";
}

// ---------------------------------------------------------------------------
// Lectura de slots (lectura directa — interruptores)
// Interruptor ON  (LOW con INPUT_PULLUP) = OCUPADO
// Interruptor OFF (HIGH)                 = LIBRE
// ---------------------------------------------------------------------------
void leerSlots() {
  for (int i = 0; i < NUM_SLOTS; i++) {
    bool estadoAnterior = slotOcupado[i];
    slotOcupado[i] = (digitalRead(SLOT_PINS[i]) == LOW);
    if (slotOcupado[i] != estadoAnterior) {
      Serial.print("  >> Slot ");
      Serial.print(i + 1);
      Serial.println(slotOcupado[i] ? " -> OCUPADO" : " -> LIBRE");
    }
  }
}

// ---------------------------------------------------------------------------
// Construcción del JSON anidado
// ---------------------------------------------------------------------------
String construirJSON(float temp, float vib, float corr,
                     int ocupados, int libres, float ocupacionPct,
                     const char* estadoMotor, int alerta) {
  String j = "{";

  j += "\"carrusel\":\"" + String(DEVICE_ID) + "\",";
  j += "\"seq\":" + String(seq) + ",";

  j += "\"slots\":{";
  j += "\"detalle\":{";
  for (int i = 0; i < NUM_SLOTS; i++) {
    j += "\"slot_" + String(i + 1) + "\":" + String(slotOcupado[i] ? 1 : 0);
    if (i < NUM_SLOTS - 1) j += ",";
  }
  j += "},";
  j += "\"ocupados\":" + String(ocupados) + ",";
  j += "\"libres\":" + String(libres) + ",";
  j += "\"ocupacion_pct\":" + String(ocupacionPct, 1);
  j += "},";

  j += "\"motor\":{";
  j += "\"temp\":" + String(temp, 1) + ",";
  j += "\"vib\":" + String(vib, 2) + ",";
  j += "\"corriente\":" + String(corr, 1) + ",";
  j += "\"estado\":\"" + String(estadoMotor) + "\",";
  j += "\"alerta\":" + String(alerta == 1 ? "true" : "false");
  j += "}";

  j += "}";
  return j;
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== Smart Carousel Parking - Gateway ESP32 (carrusel-01) ===");
  Serial.println("Broker: HiveMQ Cloud | Topic: " + String(TOPIC));

  for (int i = 0; i < NUM_SLOTS; i++) {
    pinMode(SLOT_PINS[i], INPUT_PULLUP);
  }

  connectWiFi();
  espClient.setInsecure();
  connectMQTT();
  Serial.println("Listo. Mové los interruptores (slots) y girá los pots (motor).");
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
  if (!client.connected()) connectMQTT();
  client.loop();

  leerSlots();

  if (millis() - lastPublish < 3000) return;
  lastPublish = millis();

  float temp = map(analogRead(PIN_TEMP), 0, 4095, 200,  900) / 10.0;
  float vib  = map(analogRead(PIN_VIB),  0, 4095, 0,   1200) / 100.0;
  float corr = map(analogRead(PIN_CORR), 0, 4095, 0,    200) / 10.0;

  int n = clasificar(temp, TEMP_ADV, TEMP_FALLA);
  n = max(n, clasificar(vib,  VIB_ADV,  VIB_FALLA));
  n = max(n, clasificar(corr, CORR_ADV, CORR_FALLA));
  const char* estadoMotor = nivelTexto(n);
  int alerta = (n > 0) ? 1 : 0;

  int ocupados = 0;
  for (int i = 0; i < NUM_SLOTS; i++) if (slotOcupado[i]) ocupados++;
  int   libres       = NUM_SLOTS - ocupados;
  float ocupacionPct = (100.0 * ocupados) / NUM_SLOTS;

  seq++;

  String json = construirJSON(temp, vib, corr,
                              ocupados, libres, ocupacionPct,
                              estadoMotor, alerta);

  client.publish(TOPIC, json.c_str());

  Serial.println("Publicado en " + String(TOPIC) + ":");
  Serial.println(json);
  Serial.println();
}