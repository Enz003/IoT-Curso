# Fase 2 — Gateway ESP32 en Wokwi

**Smart Carousel Parking — Trabajo Final Integrador (IoT Avanzado)**
Integrantes: Enzo Fernández + Lucas González

Simulación del nodo de borde (carrusel-01) que lee las señales del carrusel, las normaliza, clasifica el estado del motor y publica la telemetría por MQTT.

---

## Archivos

```
Fase2-Wokwi/
├── sketch.ino       ← carrusel-01 (nodo principal)
├── diagram.json     ← circuito Wokwi (ESP32 + 8 pulsadores + 3 potenciómetros)
├── libraries.txt    ← PubSubClient
├── carrusel-02/     ← copia para el carrusel-02
└── carrusel-03/     ← copia para el carrusel-03
```

---

## Los 3 carruseles (un ESP32 por carrusel)

La arquitectura define **un nodo ESP32 por carrusel**. Cada subcarpeta es un proyecto Wokwi
idéntico salvo dos líneas (`DEVICE_ID` y `TOPIC`):

| Nodo | DEVICE_ID | Topic |
|---|---|---|
| carrusel-01 (raíz) | `carrusel-01` | `carrusel/carrusel-01/telemetria` |
| carrusel-02 | `carrusel-02` | `carrusel/carrusel-02/telemetria` |
| carrusel-03 | `carrusel-03` | `carrusel/carrusel-03/telemetria` |

Se corre cada uno en su propia pestaña/proyecto de Wokwi (los tres pueden publicar a la vez).

> **Importante (HiveMQ):** el *client ID* de MQTT es el `DEVICE_ID`, así que al ser distinto en cada
> nodo no hay conflicto. Si dos nodos usaran el mismo client ID, HiveMQ los desconectaría en bucle.
> El usuario/contraseña del cluster es el mismo para los tres.

---

## Qué simula

En el sistema real el carrusel lo gobierna un **PLC (mundo OT)** que no se modifica; el ESP32 solo **lee** sus señales (Modbus/OPC-UA). Como Wokwi no simula un PLC, las señales se representan con elementos del simulador:

| Elemento Wokwi | Pin | Representa |
|---|---|---|
| 8 pulsadores | 13, 4, 14, 27, 26, 25, 33, 23 | Ocupación de cada slot (libre / ocupado) |
| Potenciómetro "Temp" | 34 | Temperatura del motor |
| Potenciómetro "Vibración" | 35 | Vibración del motor |
| Potenciómetro "Corriente" | 32 | Corriente del motor |

> Los 3 potenciómetros usan pines **ADC1** (32/34/35), que funcionan con el WiFi activo (los ADC2 no).

El ESP32 cuenta slots libres/ocupados, clasifica el motor en **NORMAL / ADVERTENCIA / FALLA** (el peor de temperatura, vibración y corriente) y publica un **JSON anidado**.

---

## Conexión

- **Broker:** HiveMQ Cloud · **TLS puerto 8883**
- **Topic:** `carrusel/carrusel-01/telemetria`
- **Red de simulación:** `Wokwi-GUEST` (abierta)

En `sketch.ino` completar con los datos del cluster:
```cpp
const char* mqtt_server = "<tu-cluster>.s1.eu.hivemq.cloud";
const char* mqtt_user   = "...";
const char* mqtt_pass   = "...";
```

### Payload de ejemplo
```json
{
  "carrusel": "carrusel-01",
  "seq": 7,
  "slots": {
    "detalle": { "slot_1": 0, "slot_2": 1, "...": "..." },
    "ocupados": 2, "libres": 6, "ocupacion_pct": 25.0
  },
  "motor": { "temp": 45.3, "vib": 2.10, "corriente": 8.5, "estado": "NORMAL", "alerta": false }
}
```

---

## Cómo correrlo en Wokwi

1. Crear proyecto ESP32 en https://wokwi.com y pegar `sketch.ino`, `diagram.json` y `libraries.txt`.
2. Completar los datos de HiveMQ en el sketch.
3. **Play** → en el Serial Monitor:
   ```
   Conectando a WiFi Wokwi-GUEST...  OK  IP: ...
   Conectando a HiveMQ (...)...  CONECTADO
   Publicado en carrusel/carrusel-01/telemetria: {...}
   ```
4. Presionar los pulsadores (cambian slots) y girar los potenciómetros (motor). Girando el de temperatura al máximo se dispara `estado: FALLA` y `alerta: true`.

### Evidencias para el informe
- Link del proyecto Wokwi (Share).
- Captura del Serial Monitor con "CONECTADO" y publicaciones.
- (El dato sigue hacia el servidor de Enzo → InfluxDB → dashboard.)

---

> **Nota:** el broker MQTT corre sobre **TLS (8883)** — esto cubre el punto de *cifrado de comunicaciones* del análisis de seguridad (STRIDE) del documento técnico.
