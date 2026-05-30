# Documentación — LAB 2: Nodo IIoT con MQTT Bidireccional

**Plataforma:** ESP32  
**Protocolo:** MQTT sobre WiFi  
**Nube:** ThingsBoard Cloud (`mqtt.thingsboard.cloud`)  
**Lenguaje:** C++ (Arduino Framework)

---

## Tabla de Contenidos

1. [Descripción General](#descripción-general)
2. [Hardware](#hardware)
3. [Credenciales y Configuración](#credenciales-y-configuración)
4. [Arquitectura del Sistema](#arquitectura-del-sistema)
5. [Lógica de Estados](#lógica-de-estados)
6. [Telemetría](#telemetría)
7. [Comandos RPC](#comandos-rpc)
8. [Funciones Clave](#funciones-clave)
9. [Flujo de Ejecución](#flujo-de-ejecución)
10. [Dependencias](#dependencias)

---

## Descripción General

Este sistema implementa un **nodo de monitoreo industrial** basado en ESP32 que:

- Mide **temperatura**, **humedad** y **carga de ventilador** en tiempo real.
- Publica telemetría cada **3 segundos** hacia ThingsBoard Cloud vía **MQTT**.
- Recibe y ejecuta **comandos RPC remotos** (ajuste de alarma, control de ventilador).
- Activa **indicadores visuales** (LEDs) según el estado del sistema.
- Aplica **media móvil de 5 muestras** para suavizar lecturas ruidosas del sensor de carga.

---

## Hardware

| Componente | Pin GPIO | Función |
|---|---|---|
| DHT22 | GPIO 4 | Temperatura (°C) y Humedad (%) |
| Potenciómetro | GPIO 34 | Simula corriente del ventilador (0–100%) |
| LED Verde | GPIO 25 | Estado: OK |
| LED Amarillo | GPIO 26 | Estado: WARNING |
| LED Rojo | GPIO 27 | Estado: FAULT |
| LED Azul | GPIO 33 | Actuador: Ventilador de emergencia (`fanOn`) |

> **Nota:** No hay buzzer físico en LAB 2. El LED azul actúa como actuador de emergencia.

---

## Credenciales y Configuración

```cpp
#define WIFI_SSID  "Wokwi-GUEST"   // Red WiFi
#define WIFI_PASS  ""               // Contraseña (vacía para Wokwi)
#define TB_TOKEN   <<TOKEN>> // Token de autenticación ThingsBoard

#define TB_HOST    "mqtt.thingsboard.cloud"
#define TB_PORT    1883
```

### Umbrales del sistema

| Parámetro | Valor por Defecto | Descripción |
|---|---|---|
| `TEMP_WARN_DEFAULT` | 28.0 °C | Umbral de advertencia de temperatura |
| `TEMP_FAULT_OFFSET` | 4.0 °C | Offset sobre WARN para entrar en FAULT |
| `LOAD_WARN_THRESH` | 60.0 % | Umbral de carga para WARNING |
| `LOAD_FAULT_THRESH` | 80.0 % | Umbral de carga para FAULT |
| `SAMPLE_SIZE` | 5 | Muestras para la media móvil |
| `LOOP_DELAY_MS` | 3000 ms | Intervalo de telemetría |
| `MQTT_RECONNECT_MS` | 5000 ms | Intervalo de reconexión MQTT |

---

## Arquitectura del Sistema

```
┌─────────────────────────────────────────────────┐
│                    ESP32                        │
│                                                 │
│  ┌──────────┐   ┌──────────┐   ┌────────────┐  │
│  │  DHT22   │   │   POT    │   │  4 x LEDs  │  │
│  │ Temp/Hum │   │ GPIO 34  │   │ G/Y/R/Azul │  │
│  └────┬─────┘   └────┬─────┘   └─────▲──────┘  │
│       │              │               │          │
│       └──────┬───────┘               │          │
│              ▼                       │          │
│      ┌───────────────┐     ┌─────────┴──────┐  │
│      │  Procesamiento│────►│ applyOutputs() │  │
│      │  + Media Móvil│     └────────────────┘  │
│      └───────┬───────┘                         │
│              │                                  │
└──────────────┼──────────────────────────────────┘
               │  WiFi + MQTT
               ▼
┌──────────────────────────────────┐
│     ThingsBoard Cloud            │
│                                  │
│  ◄── PUBLISH telemetría (3s)    │
│  ──► SUBSCRIBE RPC commands     │
└──────────────────────────────────┘
```

### Topics MQTT

| Topic | Dirección | Descripción |
|---|---|---|
| `v1/devices/me/telemetry` | ESP32 → Cloud | Publicación de telemetría |
| `v1/devices/me/rpc/request/+` | Cloud → ESP32 | Recepción de comandos RPC |
| `v1/devices/me/rpc/response/{id}` | ESP32 → Cloud | Respuesta a comandos RPC |

---

## Lógica de Estados

El sistema evalúa temperatura y carga filtrada en cada ciclo y determina uno de tres estados:

```
┌─────────────────────────────────────────────────────┐
│               evaluateState(temp, load)             │
│                                                     │
│  temp > (alarmThreshold + 4°C)                      │
│  OR load > 80%          ──────────►  FAULT          │
│                                      LED Rojo       │
│                                                     │
│  temp > alarmThreshold                              │
│  OR load > 60%          ──────────►  WARNING        │
│                                      LED Amarillo   │
│                                                     │
│  (ninguna condición)    ──────────►  OK             │
│                                      LED Verde      │
└─────────────────────────────────────────────────────┘
```

> El `alarmThreshold` es **dinámico**: se inicializa en 28.0 °C y puede modificarse en tiempo real mediante el comando RPC `setAlarm`.

---

## Telemetría

Publicada cada 3 segundos en formato JSON hacia `v1/devices/me/telemetry`.

### Ejemplo de payload

```json
{
  "temp_c": 27.5,
  "humidity_pct": 60.0,
  "load_raw_pct": 45.2,
  "load_pct": 44.8,
  "samples_used": 5,
  "state": "WARNING",
  "fault_temp": false,
  "fault_load": false,
  "alert_count": 3,
  "alarm_thresh": 28.0,
  "fan_on": false
}
```

### Descripción de campos

| Campo | Tipo | Descripción |
|---|---|---|
| `temp_c` | float | Temperatura en °C leída del DHT22 |
| `humidity_pct` | float | Humedad relativa en % |
| `load_raw_pct` | float | Valor crudo del potenciómetro (0–100%) |
| `load_pct` | float | Carga filtrada con media móvil |
| `samples_used` | int | Muestras activas en el buffer (1–5) |
| `state` | string | Estado actual: `"OK"`, `"WARNING"`, `"FAULT"` |
| `fault_temp` | bool | `true` si temperatura supera umbral FAULT |
| `fault_load` | bool | `true` si carga supera 80% |
| `alert_count` | int | Contador acumulado de eventos fuera de OK |
| `alarm_thresh` | float | Umbral de temperatura activo actualmente |
| `fan_on` | bool | Estado del ventilador de emergencia (LED azul) |

---

## Comandos RPC

El sistema recibe comandos JSON desde ThingsBoard con la siguiente estructura:

```json
{ "method": "<nombre>", "params": { ... } }
```

### Comandos disponibles

#### `setAlarm` — Ajustar umbral de temperatura

```json
{ "method": "setAlarm", "params": { "value": 30.5 } }
```

- Actualiza `alarmThreshold` al valor indicado.
- El nuevo umbral se refleja inmediatamente en la lógica de estados y en la telemetría.

#### `fanOn` — Encender ventilador de emergencia

```json
{ "method": "fanOn" }
```

- Enciende el **LED Azul** (GPIO 33).
- Independiente del estado OK/WARNING/FAULT.

#### `fanOff` — Apagar ventilador de emergencia

```json
{ "method": "fanOff" }
```

- Apaga el **LED Azul** (GPIO 33).

### Respuesta RPC

Todos los comandos generan una respuesta automática en `v1/devices/me/rpc/response/{requestId}`:

```json
{ "result": "OK" }
```

---

## Funciones Clave

### `connectWiFi()`
Conecta al AP configurado con reintentos bloqueantes. Imprime la IP asignada.

### `connectMQTT()`
Conecta al broker ThingsBoard usando el token como username. Suscribe al topic de RPC.

### `mqttCallback(topic, payload, length)`
Callback invocado por `mqtt.loop()`. Convierte el payload a C-string y delega en `handleRPC()`.

### `handleRPC(topic, payload, length)`
- Extrae el `requestId` del topic.
- Parsea el JSON con `ArduinoJson`.
- Ejecuta la acción del método indicado.
- Publica la respuesta `{"result":"OK"}`.

### `publishTelemetry()`
1. Lee temperatura y humedad del DHT22.
2. Lee el potenciómetro y calcula carga en porcentaje.
3. Actualiza el buffer circular y calcula la media móvil.
4. Evalúa el estado del sistema.
5. Actualiza los LEDs de estado.
6. Construye y publica el JSON de telemetría.

### `evaluateState(temp, load)`
Retorna `STATE_OK`, `STATE_WARNING` o `STATE_FAULT` según los umbrales definidos.

### `applyOutputs(state)`
Enciende exclusivamente el LED correspondiente al estado. El LED azul **no** es gestionado aquí.

### `movingAverage(n)`
Calcula el promedio de las últimas `n` muestras del buffer circular `loadSamples[]`.

---

## Flujo de Ejecución

```
setup()
  ├── Inicializar pines (LEDs)
  ├── Inicializar DHT22
  ├── connectWiFi()
  ├── mqtt.setServer() + mqtt.setCallback()
  └── connectMQTT()

loop()
  ├── ¿MQTT desconectado?
  │     └── reconnect cada 5 s → connectMQTT()
  ├── mqtt.loop()          ← procesa RPC entrante
  └── ¿Han pasado 3 s?
        └── publishTelemetry()
              ├── Leer DHT22
              ├── Leer potenciómetro
              ├── Media móvil
              ├── evaluateState()
              ├── applyOutputs()  ← actualiza LEDs G/Y/R
              └── mqtt.publish()  ← envía JSON a ThingsBoard
```

---

## Dependencias

| Librería | Versión recomendada | Uso |
|---|---|---|
| `WiFi.h` | Arduino ESP32 | Conectividad WiFi |
| `PubSubClient` | ≥ 2.8 | Cliente MQTT |
| `DHT` (Adafruit) | ≥ 1.4 | Sensor DHT22 |
| `ArduinoJson` | ≥ 6.x | Serialización/deserialización JSON |

---

*Documentación generada automáticamente a partir del código fuente del LAB 2.*
