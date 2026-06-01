/* ============================================================================
 *  Smart Carousel Parking — Gateway ESP32 (carrusel-03)
 *  Broker: HiveMQ Cloud (TLS, puerto 8883)
 *  Payload: JSON anidado con objetos "slots" y "motor"
 *  Topic:   carrusel/carrusel-03/telemetria
 * ============================================================================
 *
 *  Convergencia IT/OT (Semana 4): el ESP32 actúa como GATEWAY DE BORDE que
 *  solo LEE las señales del PLC (no intrusivo). En Wokwi:
 *    - 8 PULSADORES  emulan la ocupación de cada slot (libre / ocupado).
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
// >>> Completá con los datos de tu cluster <<<
// ---------------------------------------------------------------------------
const char* mqtt_server = "bd6de059ad85437bb98161dc2061832b.s1.eu.hivemq.cloud"; // host de tu cluster
const char* mqtt_user   = "parking-carrusel" ;
const char* mqtt_pass   = "Carrusel123";
const int   mqtt_port   = 8883;                        // TLS obligatorio

const char* DEVICE_ID = "carrusel-03";
const char* TOPIC     = "carrusel/carrusel-03/telemetria";

// ---------------------------------------------------------------------------
// Pines de los 8 pulsadores de slot (ocupación)
// ---------------------------------------------------------------------------
const int NUM_SLOTS = 8;
const int SLOT_PINS[NUM_SLOTS] = {13, 4, 14, 27, 26, 25, 33, 23};

// ---------------------------------------------------------------------------
// Pines analógicos del motor (potenciómetros, todos ADC1: ok con WiFi)
// ---------------------------------------------------------------------------
const int PIN_TEMP = 34;   // temperatura del motor
const int PIN_VIB  = 35;   // vibración del motor
const int PIN_CORR = 32;   // corriente del motor

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
int  lastBtn[NUM_SLOTS];
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
  client.setBufferSize(768);   // el payload anidado es más grande

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
// Devuelve 0=NORMAL, 1=ADVERTENCIA, 2=FALLA
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
// Lectura de slots (toggle por flanco)
// ---------------------------------------------------------------------------
void leerSlots() {
  for (int i = 0; i < NUM_SLOTS; i++) {
    int lectura = digitalRead(SLOT_PINS[i]);        // LOW = presionado (INPUT_PULLUP)
    if (lectura == LOW && lastBtn[i] == HIGH) {     // flanco de bajada = pulsación
      slotOcupado[i] = !slotOcupado[i];
      Serial.print("  >> Slot ");
      Serial.print(i + 1);
      Serial.println(slotOcupado[i] ? " -> OCUPADO" : " -> LIBRE");
      delay(50);                                    // anti-rebote simple
    }
    lastBtn[i] = lectura;
  }
}

// ---------------------------------------------------------------------------
// Construcción del JSON anidado
// ---------------------------------------------------------------------------
String construirJSON(float temp, float vib, float corr,
                     int ocupados, int libres, float ocupacionPct,
                     const char* estadoMotor, int alerta) {
  String j = "{";

  // Metadatos raíz
  j += "\"carrusel\":\"" + String(DEVICE_ID) + "\",";
  j += "\"seq\":" + String(seq) + ",";

  // Objeto slots
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

  // Objeto motor
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
  Serial.println("=== Smart Carousel Parking - Gateway ESP32 (carrusel-03) ===");
  Serial.println("Broker: HiveMQ Cloud | Topic: " + String(TOPIC));

  for (int i = 0; i < NUM_SLOTS; i++) {
    pinMode(SLOT_PINS[i], INPUT_PULLUP);
    lastBtn[i] = HIGH;
  }

  connectWiFi();

  // Sin verificación de certificado (válido para prototipos/simulación)
  espClient.setInsecure();

  connectMQTT();
  Serial.println("Listo. Presiona los pulsadores (slots) y gira los pots (motor).");
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop() {
  if (!client.connected()) connectMQTT();
  client.loop();

  leerSlots();   // siempre se lee para respuesta inmediata a los botones

  // Publica cada 3 segundos sin bloquear con delay()
  if (millis() - lastPublish < 3000) return;
  lastPublish = millis();

  // --- Lectura y normalización del motor ---
  float temp = map(analogRead(PIN_TEMP), 0, 4095, 200,  900) / 10.0;  // 20.0 .. 90.0 C
  float vib  = map(analogRead(PIN_VIB),  0, 4095, 0,   1200) / 100.0; // 0.00 .. 12.00 mm/s
  float corr = map(analogRead(PIN_CORR), 0, 4095, 0,    200) / 10.0;  // 0.0  .. 20.0  A

  // --- Regla en el borde: estado = el peor de los tres ---
  int n = clasificar(temp, TEMP_ADV, TEMP_FALLA);
  n = max(n, clasificar(vib,  VIB_ADV,  VIB_FALLA));
  n = max(n, clasificar(corr, CORR_ADV, CORR_FALLA));
  const char* estadoMotor = nivelTexto(n);
  int alerta = (n > 0) ? 1 : 0;

  // --- Disponibilidad de slots ---
  int ocupados = 0;
  for (int i = 0; i < NUM_SLOTS; i++) if (slotOcupado[i]) ocupados++;
  int   libres      = NUM_SLOTS - ocupados;
  float ocupacionPct = (100.0 * ocupados) / NUM_SLOTS;

  seq++;

  // --- Armar y publicar JSON ---
  String json = construirJSON(temp, vib, corr,
                              ocupados, libres, ocupacionPct,
                              estadoMotor, alerta);

  client.publish(TOPIC, json.c_str());

  Serial.println("Publicado en " + String(TOPIC) + ":");
  Serial.println(json);
  Serial.println();
}
