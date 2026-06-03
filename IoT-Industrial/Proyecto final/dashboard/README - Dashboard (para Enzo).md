# Dashboard de Control — Handoff para Enzo

**Archivo:** `dashboard/index.html` (todo en uno: HTML + CSS + JS, sin build, sin dependencias locales).
**Hecho por:** Lucas · **Fase 4** del TFI — Estacionamiento Carrusel IoT.

---

## 1. Qué hace

Dashboard web de control para los 3 carruseles. Muestra:

- **3 cards** (una por carrusel): slots libres / ocupados / total, barra de % de ocupación, badge "⚠ ALERTA", grilla de los 8 slots (verde = libre, rojo = ocupado) y panel de motor (temp, vibración, corriente + estado NORMAL/ALERTA).
- **Gráfico de ocupación %** en el tiempo (las 3 series).
- **Gráfico de telemetría del motor** (temp / vibración / corriente) con selector de carrusel.
- **Tabla de alertas** de las últimas 24 h.
- Reloj en vivo + indicador de conexión. Tema oscuro, responsive.

Usa **Chart.js** y la fuente **Inter** desde CDN (requiere internet en el navegador del cliente). No necesita nada más.

---

## 2. Lo único que tenés que tocar

Arriba de todo del `<script>` hay dos variables:

```js
const BASE_URL = "https://manner-refugees-resist-filed.trycloudflare.com";
let   USE_MOCK = true;   // true = datos dummy · false = tu servidor real
```

Para conectarlo a tu servidor:

1. Poné `USE_MOCK = false`.
2. Actualizá `BASE_URL` con la URL vigente de tu túnel de Cloudflare (cambia si reiniciás el server).

Con `USE_MOCK = false` se activan solos el **WebSocket** (tiempo real) y el **polling** de respaldo. Nada más que cambiar.

> En `true` el dashboard corre con datos dummy (los de tu guía, ampliados a los 3 carruseles) y simula movimiento cada 3 s. Sirve para desarrollar y para las capturas del informe sin depender del server.

---

## 3. Endpoints que consume (tu API)

Todo pasa por una sola función `apiFetch(endpoint)`. Lo que espera de tu servidor:

| Endpoint | Cuándo se llama | Se usa para |
|---|---|---|
| `GET /iot/api/carruseles` | al cargar + cada 5 s | cards, grilla de slots, panel de motor |
| `GET /iot/api/historial/:id?horas=6` | al cargar + cada 30 s | gráfico de ocupación % |
| `GET /iot/api/motor/:id?horas=6` | al cargar + al cambiar el selector + cada 30 s | gráfico del motor |
| `GET /iot/api/alertas?horas=24` | al cargar + cada 30 s | tabla de alertas |

Las estructuras de respuesta son exactamente las de tu guía de integración (`carruseles` con `slots` y `motor`; histórico con `_time` + `ocupacion_pct`; motor con `temp_c`/`vibracion`/`corriente`; alertas con `_time`/`carrusel`/`estado`/`temp_c`/`vibracion`).

---

## 4. WebSocket (tiempo real)

Cuando `USE_MOCK = false`, el dashboard abre:

```
wss://<BASE_URL sin https>/iot/ws
```

Y maneja dos tipos de mensaje, tal como definiste:

- `{ type: "telemetria", payload: {...item de /carruseles...} }` → actualiza el card de ese carrusel al instante.
- `{ type: "estado_inicial", payload: [...] }` → carga inicial de todos los carruseles.

Si el WS se cae, reintenta solo cada 3 s. Igual hay un polling de respaldo a `/iot/api/carruseles` cada 5 s, así que aunque falle el WS el panel se mantiene actualizado.

---

## 5. Cómo alzarlo en el servidor

Es un estático puro, así que cualquiera de estas sirve:

- **Servirlo desde el mismo Node** (recomendado para evitar CORS): poné el `index.html` en una carpeta `public/` y agregá `app.use(express.static('public'))`. Queda servido en la misma URL/origen que la API.
- O cualquier estático: `python3 -m http.server`, Nginx, etc.

Estructura sugerida para el repo:

```
dashboard/
└── index.html
```

---

## 6. Checklist cuando lo conectes al server real

- [ ] `USE_MOCK = false` y `BASE_URL` con la URL de Cloudflare vigente.
- [ ] El indicador de conexión (arriba a la derecha) pasa a verde "En vivo (WebSocket)".
- [ ] Las 3 cards muestran datos reales y cambian al mover slots en el ESP32.
- [ ] Los dos gráficos se llenan (si están vacíos, revisá `/historial` y `/motor`).
- [ ] La tabla de alertas lista eventos cuando un motor entra en ALERTA.

---

## 7. Si algo no anda

- **Cards vacías / consola con error de fetch** → `BASE_URL` mal o el túnel de Cloudflare caído. Verificá abriendo `BASE_URL/iot/api/carruseles` en el navegador.
- **Error de CORS** → servir el `index.html` desde el mismo origen que la API (ver punto 5) o habilitar CORS en el server.
- **WS no conecta (queda en "Reconectando…")** pero las cards sí cargan → la API REST anda pero el `/iot/ws` no; el dashboard igual funciona por polling cada 5 s.
- **Gráficos en blanco** → los endpoints `/historial/:id` y `/motor/:id` devuelven `[]`; confirmá que haya histórico en InfluxDB para ese `:id`.

---

*Para volver a modo demo (capturas, presentación sin server): `USE_MOCK = true`.*
