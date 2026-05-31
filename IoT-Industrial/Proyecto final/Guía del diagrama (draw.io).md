 # Guía para dibujar el diagrama de arquitectura en draw.io

Objetivo: un diagrama de **4 capas (de arriba hacia abajo)** con una **línea divisoria clara entre el mundo OT (PLC) y el mundo IT/IoT**. Exportarlo como **PNG o SVG** para insertarlo en el documento técnico.

> Abrí [app.diagrams.net](https://app.diagrams.net) → *Create New Diagram* → *Blank*. Guardalo en esta misma carpeta. Al final: *File → Export as → PNG* (marcá "Transparent background" y zoom 200% para buena resolución).

---

## Layout general (orientación vertical, el dato fluye hacia abajo)

```
┌──────────────────────────────────────────────────────────┐
│  ZONA OT  (fondo rojo/naranja claro)  — "NO se modifica"  │
│  CAPA 0 — Control industrial                              │
│   [ PLC del carrusel ]  ── lee ──  [ Sensores de campo ]  │
└──────────────────────────────────────────────────────────┘
======= FRONTERA OT / IT  (línea gruesa punteada, etiquetada) =======
┌──────────────────────────────────────────────────────────┐
│  ZONA IT / IoT  (fondo azul claro)                        │
│  CAPA 1 — Borde:  [ESP32 #1] [ESP32 #2] [ESP32 #3]        │
│  CAPA 2 — Red:    Wi-Fi/Ethernet → [Broker Mosquitto]     │
│  CAPA 3 — Plataforma: [Servidor Node.js] ─ [InfluxDB]     │
│  CAPA 4 — Apps:   [Dashboard operador] [Web app móvil]    │
└──────────────────────────────────────────────────────────┘
```

Usá dos **contenedores grandes** (rectángulos de fondo) para las zonas OT e IT, y entre ellos una **línea horizontal gruesa y punteada** con la etiqueta **"Frontera OT / IT — lectura no intrusiva"**. Esa línea es lo más importante del diagrama: demuestra la convergencia IT/OT de la Semana 4.

---

## Qué poner en cada caja (texto sugerido)

**ZONA OT (fondo rojo/naranja claro) — rótulo: "MUNDO OT — no se modifica (fuera de alcance)"**

- **CAPA 0 — Control industrial**
  - Caja **PLC del carrusel** (usá el ícono de PLC o un rectángulo).
  - Caja **Sensores industriales de campo**, con viñetas dentro:
    - Detector de ocupación por slot (espira inductiva / barrera fotoeléctrica, ×8 por carrusel)
    - Encoder rotativo (posición del carrusel)
    - PT100/RTD (temperatura del motor) + acelerómetro IEPE (vibración)
    - Transformador de corriente (consumo del motor)
  - Flecha **Sensores → PLC**.

**FRONTERA OT / IT** (línea punteada gruesa). Sobre la flecha que cruza la línea, etiqueta:
**"Modbus TCP / OPC-UA — solo lectura"**.

**ZONA IT/IoT (fondo azul claro) — rótulo: "MUNDO IT / IoT (alcance del proyecto)"**

- **CAPA 1 — Dispositivo / Gateway de borde**
  - **3 cajas ESP32** (una por carrusel: *Carrusel-01, -02, -03*). Subtítulo: "lee registros del PLC, normaliza, arma JSON, aplica reglas en el borde".
  - Flecha de cada ESP32 hacia abajo etiquetada **"publica JSON por MQTT"**.

- **CAPA 2 — Conectividad / Red**
  - Etiqueta del medio: **Wi-Fi / Ethernet industrial**.
  - Caja **Broker MQTT — Mosquitto (local)**, puerto 1883 (u 8883 con TLS).

- **CAPA 3 — Plataforma / Procesamiento (servidor local)**
  - Caja **Servidor Node.js** (Express + mqtt.js + ws): "cliente MQTT suscrito, calcula slots libres y alertas".
  - Caja **InfluxDB** (series temporales) conectada al servidor.
  - Flechas de salida etiquetadas **"REST + WebSocket"**.

- **CAPA 4 — Aplicación** (dos cajas en paralelo)
  - **4a · Dashboard de control** (operador/municipio): mapa de los 3 carruseles, estado de slots, KPIs y alertas de mantenimiento.
  - **4b · Web app móvil** (conductor): "¿dónde hay lugar?" con carruseles cercanos y slots libres.

---

## Flechas y etiquetas (de arriba hacia abajo)

1. Sensores de campo → **PLC**
2. PLC → ESP32 : **"Modbus TCP / OPC-UA — solo lectura"** *(cruza la frontera OT/IT)*
3. ESP32 → Mosquitto : **"MQTT (JSON)"**
4. Mosquitto → Servidor Node.js : **"suscripción MQTT"**
5. Servidor ↔ InfluxDB : **"persistencia / consultas"**
6. Servidor → Dashboard y → Web app : **"REST + WebSocket"**

---

## Nota para incluir junto al diagrama (Wokwi)

Agregá un recuadro de nota: *"En la simulación de Wokwi, las señales que normalmente provienen del PLC se representan con elementos del simulador (pulsadores = ocupación de slots; potenciómetros = temperatura/vibración del motor). Esto emula los registros que el ESP32 leería del PLC vía Modbus/OPC-UA en el sistema real."*

---

## Sugerencia de colores (consistencia visual)

- Zona OT: relleno `#FCE4D6` (naranja claro), borde rojo.
- Zona IT/IoT: relleno `#DAE8FC` (azul claro), borde azul.
- Cajas de capa: blanco con borde gris, título en negrita.
- Línea divisoria: negra, gruesa (3 px), punteada.

Con esto el evaluador ve de un vistazo las 4 capas y, sobre todo, **dónde termina el OT y empieza el IoT**.
