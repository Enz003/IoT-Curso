# Plan de Ejecución — Trabajo Final Integrador

## Proyecto: Sistema IoT para Estacionamiento tipo Carrusel ("Smart Carousel Parking")

**Integrantes:** Enzo Fernández + [Nombre del compañero/a]
**Fecha tope de entrega:** 05/06 — 23:59 hs
**Sector:** Ciudades inteligentes / movilidad urbana (Semana 5)

---

## 0. Idea central (resumen del concepto)

Un estacionamiento tipo carrusel es una estructura rotatoria vertical que almacena varios vehículos en poco espacio. La propuesta IoT sensoriza cada *slot* (posición) del carrusel para saber en tiempo real si está libre u ocupado, monitorea el motor del carrusel (mantenimiento predictivo) y expone esa información en dos productos:

1. **Dashboard de control** → para el operador / municipio. Monitorea todos los carruseles de la ciudad, su disponibilidad, alertas y salud de los motores.
2. **Web app para usuarios (móvil)** → el conductor consulta desde el celular qué carrusel cercano tiene slots libres antes de llegar.

Los nodos ESP32 se simulan en **Wokwi**, publican por **MQTT** a un **broker local (Mosquitto)**, un **servidor local (Node.js o Python)** se suscribe, persiste los datos y los sirve a ambos frontends por **REST + WebSocket**.

> **Por qué IoT es adecuado:** el problema (saber en tiempo real qué slot está libre y evitar fallas del carrusel) requiere datos continuos de sensores distribuidos, baja latencia y visualización remota — exactamente lo que resuelve una arquitectura IoT de 4 capas.

---

## 1. Mapeo del proyecto a los temas del curso

Esto asegura que el trabajo "use lo aprendido en las semanas de clase" y cubra todos los criterios de evaluación.

| Semana | Tema | Cómo se aplica en el proyecto |
|---|---|---|
| **S1** | Arquitectura IoT 4 capas, verticales | Diseño de las 4 capas; el parking se ubica en la vertical *ciudades inteligentes* |
| **S2** | MQTT, topics, QoS, brokers, plataformas | Diseño de topics jerárquicos, QoS por tipo de dato, broker Mosquitto local |
| **S3** | Pipeline de datos, series temporales, BD, análisis Python | Ingesta→almacenamiento→visualización; histórico de ocupación; análisis en notebook |
| **S4** | IIoT, convergencia IT/OT (PLC, OPC-UA, Modbus), KPIs (OEE, MTBF, MTTR), mantenimiento predictivo | ESP32 como gateway IT/OT que lee del PLC; monitoreo de motor (temp/vibración) → mantenimiento predictivo; KPIs de disponibilidad |
| **S5** | Ciudades inteligentes, smart parking, eficiencia energética | Núcleo del caso: estacionamiento inteligente urbano; KPI de ocupación y energía |
| **S6** | LoRaWAN / NB-IoT, conectividad | Justificación de la red elegida (Wi-Fi vs NB-IoT) para nodos urbanos |
| **S7** | Seguridad (STRIDE), cifrado TLS, ética y privacidad | Matriz de riesgos, MQTT sobre TLS, reflexión ética sobre datos de usuarios |

---

## 2. Arquitectura propuesta (4 capas) + convergencia IT/OT

> **Alcance del proyecto (importante):** la mecánica del carrusel ya está controlada por un **PLC** (capa OT) que **no se modifica**. El proyecto cubre **solo la capa IoT**: el ESP32 actúa como **gateway de borde (edge)** que *lee/procesa las señales* del sistema de forma **no intrusiva** (solo lectura) y las publica a la plataforma. Esto es exactamente la **convergencia IT/OT** de la Semana 4: el PLC vive en el mundo OT y el ESP32 hace de puente hacia el mundo IT/IoT.

```
─────────────  MUNDO OT (NO se toca — fuera de alcance)  ─────────────
CAPA 0 — Control industrial
  PLC del carrusel + sensores industriales de campo
    - Detectores de ocupación por slot (espira inductiva / barrera fotoeléctrica / sensor inductivo de proximidad 24 V)
    - Encoder rotativo industrial (posición del carrusel)
    - PT100 / RTD (temperatura del motor)  +  acelerómetro IEPE (vibración)
    - Transformador de corriente (consumo del motor)

        │  lectura NO intrusiva (solo lectura): Modbus TCP / OPC-UA
        │  (alternativa: tap paralelo de señales por entradas aisladas/optoacopladas)
        ▼
─────────────────────────  MUNDO IT / IoT  ─────────────────────────
CAPA 1 — Dispositivo / Gateway de borde
  Nodo ESP32 (gateway, simulado en Wokwi):
    - Lee los registros del PLC / señales de campo
    - Normaliza, arma el JSON y aplica reglas simples en el borde (edge)

        │  publica JSON por MQTT
        ▼
CAPA 2 — Conectividad / Red
  Wi-Fi / Ethernet industrial → MQTT (puerto 1883, o 8883 con TLS)
  Broker: Mosquitto local

        │
        ▼
CAPA 3 — Plataforma / Procesamiento  (SERVIDOR LOCAL)
  Servidor Node.js (Express + mqtt.js + ws)  ó  Python (Flask + paho-mqtt)
    - Cliente MQTT suscrito a los topics
    - Lógica: calcula slots libres, detecta alertas
    - Base de datos: SQLite (simple) o InfluxDB (series temporales)
    - API REST + WebSocket para los frontends

        │
        ├────────────► CAPA 4a — Dashboard de control (operador/ciudad)
        │                 HTML/JS, mapa de carruseles, estado de slots,
        │                 KPIs y alertas de mantenimiento
        │
        └────────────► CAPA 4b — Web app de usuarios (móvil)
                          "¿Dónde hay lugar?" → lista de carruseles
                          cercanos con slots libres
```

> **Tarea a ejecutar:** dibujar este diagrama en una herramienta (draw.io / Excalidraw / Canva) y exportarlo como imagen para el documento técnico, dejando clara la **línea divisoria OT (PLC) / IT (IoT)**.

---

## 3. Selección tecnológica justificada (a redactar en el doc)

Los sensores son de **grado industrial** (los que realmente usa un sistema de estacionamiento), pertenecen a la capa OT del PLC y **no se instalan en este proyecto**: el ESP32 solo lee sus señales. Se documentan para justificar la solución completa de extremo a extremo.

**Sensores industriales de campo (capa OT, del PLC):**

| Variable | Componente industrial | Justificación |
|---|---|---|
| Ocupación de slot | Espira inductiva / barrera fotoeléctrica / sensor inductivo de proximidad (PNP 24 V, IP67) | Estándar en parking y control de accesos; robusto, sin contacto, resiste intemperie y vehículos. No es un sensor de hobby |
| Posición del carrusel | Encoder rotativo industrial (incremental/absoluto) | Sabe en qué posición está cada slot; alta precisión |
| Temperatura del motor | PT100 / RTD | Precisión industrial para mantenimiento predictivo (Semana 4) |
| Vibración del motor | Acelerómetro IEPE / sensor de vibración industrial | Detecta desgaste de rodamientos antes de la falla |
| Consumo del motor | Transformador de corriente (CT) | Detecta sobreesfuerzo y anomalías de carga |

**Capa IoT (lo que sí construye el proyecto):**

| Componente          | Elección                                                          | Justificación                                                                      |
| ------------------- | ----------------------------------------------------------------- | ---------------------------------------------------------------------------------- |
| Gateway de borde    | ESP32                                                             | Wi-Fi/Ethernet, soporta Modbus TCP y MQTT; usado en el curso; rol de gateway IT/OT |
| Integración con PLC | Modbus TCP / OPC-UA (solo lectura)                                | Lee datos del PLC sin alterar el control; patrón de convergencia IT/OT (Semana 4)  |
| Protocolo IoT       | MQTT                                                              | Ligero, pub/sub, estándar de facto IoT (Semana 2)                                  |
| Broker              | Mosquitto (local)                                                 | Open source, ligero, corre en el servidor local                                    |
| Servidor            | **Node.js** (Express + mqtt.js + ws)                              | Recibe MQTT, persiste y sirve API; ideal para WebSocket en tiempo real             |
| Base de datos       | **InfluxDB**                                                      | Base de datos de series temporales; histórico de ocupación y telemetría (Semana 3) |
| Red                 | Wi-Fi/Ethernet industrial; discutir NB-IoT para despliegue urbano | Justificar vs LoRaWAN/NB-IoT (Semana 6)                                            |

> **Nota sobre la simulación en Wokwi:** Wokwi no puede simular un PLC ni señales industriales de 24 V. Por eso, en la simulación los **valores que normalmente llegarían del PLC se representan con elementos de Wokwi** (pulsadores = entradas digitales de ocupación; potenciómetros = temperatura/vibración del motor). En el documento se aclara que esto **representa la señal que el ESP32 leería del PLC vía Modbus/OPC-UA** en el sistema real. Así la simulación demuestra el concepto sin falsear el diseño industrial.

> **Decisiones:** (1) Servidor: ✅ **Node.js** (Express + mqtt.js + ws) — encaja mejor con web + WebSocket en tiempo real. (2) ⏳ ¿El ESP32 lee del PLC por **Modbus TCP / OPC-UA** (recomendado, lo más realista y no intrusivo) o por **tap paralelo de señales** con entradas aisladas? — a definir en Fase 2.

---

## 4. Flujo de datos y diseño de topics MQTT

**Recorrido:** `Sensores industriales → PLC (OT) → [Modbus/OPC-UA, solo lectura] → ESP32 gateway → Wi-Fi/Ethernet → MQTT → Mosquitto → Servidor → BD → Dashboard/Web app`

**Topics propuestos (jerárquicos, Semana 2):**

```
parking/asuncion/carrusel-01/slot/3/estado        → ocupado | libre
parking/asuncion/carrusel-01/disponibilidad        → nº de slots libres
parking/asuncion/carrusel-01/motor/temperatura
parking/asuncion/carrusel-01/motor/vibracion
parking/asuncion/carrusel-01/status                → LWT (online/offline)
```

**Frecuencia:** ocupación on-change + heartbeat cada 10 s; motor cada 5 s.
**QoS:** QoS 0 para telemetría de motor; QoS 1 para cambios de ocupación y alertas.

**Ejemplo de payload JSON (a incluir en el doc):**

```json
{
  "carrusel": "carrusel-01",
  "ts": "2026-06-01T14:32:10Z",
  "slots": { "1": "ocupado", "2": "libre", "3": "ocupado" },
  "libres": 1,
  "motor": { "temp_c": 41.2, "vibracion": 0.07 },
  "alerta": false
}
```

**Acciones/alertas generadas:**
- Slot cambia a libre → se actualiza disponibilidad en la web app.
- Temp motor > umbral o vibración alta → alerta de mantenimiento en el dashboard (clasificación simple: NORMAL / ADVERTENCIA / FALLA).

---

## 5. Seguridad y privacidad — matriz STRIDE (mínimo 3, Semana 7)

A completar en el doc; elegir al menos 3 de estos:

| Amenaza (STRIDE) | Riesgo en el proyecto | Mitigación |
|---|---|---|
| **Spoofing** | Un dispositivo falso publica datos falsos de ocupación | Autenticación MQTT con usuario/contraseña + certificados |
| **Tampering** | Alteración de mensajes en tránsito | Cifrado TLS (MQTT 8883) |
| **Info Disclosure** | Exposición de datos de usuarios (patente, horarios) | TLS + minimización de datos + control de acceso |
| **DoS** | Saturación del broker | Límite de conexiones, segmentación de red OT |
| **Elevation** | Acceso no autorizado al dashboard de control | Login con roles (operador vs usuario), credenciales fuera del código |

---

## 6. Reflexión ética (mínimo 2 puntos, Semana 7)

A desarrollar en el doc (sugerencias):
- **Qué datos se recolectan / sensibles:** ocupación, horarios de uso; si se registra patente, ¿es dato personal? → minimizar.
- **Vigilancia / uso secundario:** los patrones de estacionamiento pueden revelar rutinas de personas; definir retención y anonimización.
- **Privacidad de usuarios de la web app:** no exigir login innecesario; datos de ubicación solo en el dispositivo.
- **Impacto social positivo:** menos tiempo buscando lugar → menos congestión y emisiones.

---

## 7. Caso de negocio (Semana 4 y 5)

A estimar en el doc:
- **CAPEX:** ESP32 + sensores por carrusel, servidor local, instalación.
- **OPEX:** energía, conectividad, mantenimiento del software, hosting.
- **ROI:** mayor rotación de vehículos (más ingresos), mantenimiento predictivo reduce paradas, menos personal de control.
- **KPIs:** % de ocupación, disponibilidad del carrusel (uptime), reducción de tiempo de búsqueda de slot, reducción de fallas (MTBF↑ / MTTR↓), reducción de emisiones por menor circulación.

---

## 8. Plan de trabajo por fases (lo que vas a ejecutar)

### Fase 1 — Diseño y documentación base (Día 1)
- [x] Definir N de carruseles y slots por carrusel para la simulación → **3 carruseles × 8 slots** (24 posiciones totales).
- [x] Decidir servidor: **Node.js** (Express + mqtt.js + ws).
- [x] Dibujar el diagrama de arquitectura (draw.io) y exportarlo. → *Lo dibuja Enzo en draw.io; ver `Guía del diagrama (draw.io).md`.*
- [x] Redactar problema, contexto, objetivos y justificación de IoT. → *Ver `Documento Técnico - Base.md`.*

### Fase 2 — Prototipo: gateway ESP32 en Wokwi (Día 1–2)
- [ ] Crear proyecto Wokwi con ESP32 y elementos que **representan las señales del PLC** (pulsadores = ocupación de slots; potenciómetros = temperatura/vibración del motor).
- [ ] Programar el `.ino`: leer las entradas, normalizar, armar JSON, conectar Wi-Fi y publicar por MQTT.
- [ ] Aclarar en comentarios y en el doc que estas entradas **emulan los registros que el ESP32 leería del PLC vía Modbus/OPC-UA** en el sistema real.
- [ ] Usar un broker público de prueba (broker.hivemq.com) o el Mosquitto local.
- [ ] Capturar el Serial Monitor mostrando las publicaciones.

### Fase 3 — Servidor local + broker (Día 2–3) ✅ COMPLETADA
- [x] Broker MQTT: **HiveMQ Cloud (free tier)** — cluster privado con autenticación usuario/contraseña y TLS (puerto 8883). Permite al compañero publicar desde Wokwi (casa) sin exponer puertos en la Raspberry, y a la vez cubre los requisitos de seguridad de la consigna (autenticación + cifrado).
- [x] Crear el servidor Node.js: suscripto a HiveMQ Cloud vía `mqtts://` con credenciales, parsea JSON, calcula slots libres y detecta alertas de motor.
- [x] Persistir en **InfluxDB** (series temporales) — mediciones: `disponibilidad`, `motor`, `slots`. Timestamp: hora real del servidor (no del ESP32, que no tiene RTC en Wokwi).
- [x] API REST expuesta vía nginx + Cloudflare tunnel:
  - `GET /iot/api/health`
  - `GET /iot/api/carruseles`
  - `GET /iot/api/disponibilidad`
  - `GET /iot/api/carrusel/:id`
  - `GET /iot/api/historial/:id?horas=N`
  - `GET /iot/api/alertas`
- [x] WebSocket en `/iot/ws` — broadcast en tiempo real a cada mensaje MQTT recibido.

**Implementación en Raspberry Pi (Docker):**
```
~/docker-iot/
├── docker-compose.yml       ← InfluxDB + IoT Server (red externa iot-net)
└── server/
    ├── Dockerfile
    ├── package.json
    └── src/
        ├── index.js          ← Express + WebSocketServer + arranca handler MQTT
        ├── mqtt-handler.js   ← suscripción mqtts:// + credenciales + parseo + broadcast WS
        ├── influx.js         ← escritura (timestamp servidor) y consulta InfluxDB
        └── routes/api.js     ← endpoints REST
```

**Red Docker:** se creó la red externa `iot-net`; el contenedor nginx existente fue conectado a ella para poder resolver `iot-server` por nombre.

**Broker MQTT — datos de conexión para el ESP32 del compañero:**
```
Host:    bd6de059ad85437bb98161dc2061832b.s1.eu.hivemq.cloud
Puerto:  8883  (TLS/SSL)
Usuario: parking-carrusel
Pass:    Carrusel123
Topic:   parking/asuncion/carrusel-01/telemetria  (o carrusel-02 / carrusel-03)
```

**Código ESP32 para conectar al broker seguro:**
```cpp
#include <WiFiClientSecure.h>
WiFiClientSecure wifiClient;
wifiClient.setInsecure();  // acepta TLS sin verificar CA (válido para prototipo)
PubSubClient mqtt(wifiClient);
mqtt.setServer("bd6de059ad85437bb98161dc2061832b.s1.eu.hivemq.cloud", 8883);
mqtt.connect("ESP32_Carrusel_01", "parking-carrusel", "Carrusel123");
```

**Payload esperado:**
```json
{
  "carrusel": "carrusel-01",
  "ts": "2026-06-01T14:32:10Z",
  "slots": { "1": "ocupado", "2": "libre", ... },
  "libres": 3,
  "motor": { "temp_c": 41.2, "vibracion": 0.07 },
  "alerta": false
}
```

**Seguridad implementada:** autenticación por usuario/contraseña + cifrado TLS en tránsito. Cubre las amenazas Spoofing, Tampering e Information Disclosure de la matriz STRIDE (sección 5).

### Fase 4 — Frontends (Día 3–4)
- [x] **Dashboard de control:** estado de todos los carruseles, slots, KPIs y alertas de mantenimiento.
- [ ] **Web app de usuarios:** vista móvil simple "¿dónde hay lugar?" con carruseles y slots libres.
- [x] Capturar pantallas de ambos funcionando.

### Fase 5 — Análisis de datos (opcional pero suma, Día 4)
- [ ] Exportar histórico de ocupación a CSV.
- [ ] Notebook Python (pandas/matplotlib): patrones de ocupación por hora, % de uso, detección simple de anomalías en el motor.

### Fase 6 — Documento técnico final (Día 5)
- [ ] Armar el PDF/Word (hasta 5 páginas) con todas las secciones requeridas (ver checklist abajo).
- [ ] Insertar diagrama, payload, matriz STRIDE, caso de negocio y evidencias (capturas + link Wokwi).
- [ ] Conclusiones coherentes con problema→solución→impacto.

### Fase 7 — Entrega (antes del 05/06 23:59)
- [ ] Subir a EDUCA: documento + link Wokwi + código (`.ino`, servidor, notebook) + capturas.

---

## 9. Checklist de entregables (según la consigna)

**A) Documento técnico (PDF/Word, ≤5 págs):**
- [ ] Portada e integrantes
- [ ] Descripción del problema y contexto
- [ ] Objetivos de la solución
- [ ] Arquitectura IoT (diagrama)
- [ ] Selección y justificación tecnológica
- [ ] Flujo de datos + ejemplo de payload JSON
- [ ] Análisis de seguridad (matriz STRIDE, ≥3)
- [ ] Reflexión ética (≥2 puntos)
- [ ] Caso de negocio (CAPEX/OPEX/ROI/KPIs)
- [ ] Evidencias del prototipo (capturas + link)
- [ ] Conclusiones

**B) Prototipo / simulación:**
- [ ] Link de Wokwi
- [ ] Código fuente: `.ino` (ESP32), servidor (Node/Python), notebook `.ipynb` (opcional)
- [ ] Capturas del Serial Monitor
- [ ] Capturas del dashboard y de la web app

---

## 10. Decisiones (actualizado al 31/05/2026)

1. **Nombre del compañero/a** de equipo (para la portada). → ⏳ **PENDIENTE** (confirmar).
2. **Servidor:** ✅ **Node.js** (Express + mqtt.js + ws).
3. **Cantidad de carruseles y slots:** ✅ **3 carruseles × 8 slots** (24 posiciones).
4. **Base de datos:** ✅ **InfluxDB** (series temporales).
5. **Integración ESP32–PLC:** ⏳ Modbus TCP / OPC-UA vs. tap paralelo → a definir en Fase 2.
6. **Documento técnico / código base:** Fase 1 iniciada → `Documento Técnico - Base.md` + `Guía del diagrama (draw.io).md`.
7. **Broker MQTT:** ✅ **HiveMQ Cloud free tier** — cluster privado con usuario/contraseña + TLS puerto 8883. Elegido sobre broker público para cubrir requisitos de seguridad de la consigna sin necesidad de exponer puertos en la Raspberry.
8. **Conectividad remota ESP32→Raspberry:** ✅ HiveMQ Cloud como punto de encuentro seguro. ESP32 publica con credenciales; la Raspberry se suscribe con las mismas credenciales. Tráfico cifrado en ambos sentidos.
9. **API pública:** ✅ accesible vía Cloudflare tunnel en `https://[url].trycloudflare.com/iot/api/*` — rutas añadidas a nginx existente.
10. **Timestamp en InfluxDB:** ✅ se usa la hora real del servidor (no el campo `ts` del payload) — el ESP32 en Wokwi no tiene RTC y envía fechas estáticas.

---

## 11. Estado de avance al 31/05/2026

| Fase                                     | Estado       | Responsable |
| ---------------------------------------- | ------------ | ----------- |
| Fase 1 — Diseño y documentación base     | ✅ Completada | Enzo        |
| Fase 2 — Prototipo ESP32 en Wokwi        | ⏳ Pendiente  | Lucas       |
| Fase 3 — Servidor + broker + BD + API    | ✅ Completada | Enzo        |
| Fase 4 — Frontends (dashboard + web app) | ✅ Completada | Lucas       |
| Fase 5 — Análisis de datos (notebook)    | ⏳ Pendiente  |             |
| Fase 6 — Documento técnico final         | ⏳ Pendiente  |             |
| Fase 7 — Entrega                         | ⏳ Pendiente  |             |
