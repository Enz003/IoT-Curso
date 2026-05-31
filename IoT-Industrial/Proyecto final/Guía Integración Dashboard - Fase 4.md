# Guía de Integración — Dashboard de Control
## Estacionamiento Carrusel IoT — Fase 4

> **Para quién es esto:** guía completa para que el partner construya el dashboard con IA (Claude, ChatGPT, etc.) y luego Enzo lo alce en el servidor.

---

## 1. URL base del servidor

```js
const BASE_URL = "https://manner-refugees-resist-filed.trycloudflare.com";
```

> **Nota:** Si no conecta, pedile a Enzo la URL actualizada — puede cambiar si el servidor se reinicia.

---

## 2. Endpoints disponibles

### `GET /iot/api/disponibilidad`
Estado actual de todos los carruseles. **Endpoint principal del dashboard.**

```js
fetch(`${BASE_URL}/iot/api/disponibilidad`)
```

**Respuesta:**
```json
[
  {
    "carrusel": "carrusel-01",
    "libres": 4,
    "ocupados": 4,
    "total": 8,
    "ocupacion_pct": 50.0,
    "alerta": false,
    "slots": {
      "1": "libre",
      "2": "ocupado",
      "3": "ocupado",
      "4": "libre",
      "5": "libre",
      "6": "ocupado",
      "7": "libre",
      "8": "ocupado"
    }
  },
  { "carrusel": "carrusel-02", "...": "..." },
  { "carrusel": "carrusel-03", "...": "..." }
]
```

---

### `GET /iot/api/carruseles`
Igual que disponibilidad pero incluye también datos del motor.

```js
fetch(`${BASE_URL}/iot/api/carruseles`)
```

**Respuesta:**
```json
{
  "total": 3,
  "carruseles": [
    {
      "carrusel": "carrusel-01",
      "receivedAt": "2026-06-01T14:32:10Z",
      "libres": 4,
      "ocupados": 4,
      "total": 8,
      "ocupacion_pct": 50.0,
      "alerta": false,
      "slots": {
        "1": "libre",  "2": "ocupado", "3": "ocupado", "4": "libre",
        "5": "libre",  "6": "ocupado", "7": "libre",   "8": "ocupado"
      },
      "motor": {
        "temp_c": 41.2,
        "vibracion": 0.07,
        "corriente": 3.5,
        "estado": "NORMAL",
        "alerta": false
      }
    }
  ]
}
```

---

### `GET /iot/api/carrusel/:id`
Un solo carrusel por ID.

```js
fetch(`${BASE_URL}/iot/api/carrusel/carrusel-01`)
// Misma estructura que un item de /carruseles
```

---

### `GET /iot/api/historial/:id?horas=1`
Histórico de **ocupación** de un carrusel (últimas N horas). Sirve para el **gráfico de ocupación a lo largo del tiempo**.

```js
fetch(`${BASE_URL}/iot/api/historial/carrusel-01?horas=6`)
```

**Respuesta:**
```json
[
  {
    "_time": "2026-06-01T14:20:00Z",
    "carrusel": "carrusel-01",
    "libres": 6,
    "ocupados": 2,
    "total": 8,
    "ocupacion_pct": 25.0
  },
  {
    "_time": "2026-06-01T14:25:00Z",
    "carrusel": "carrusel-01",
    "libres": 4,
    "ocupados": 4,
    "total": 8,
    "ocupacion_pct": 50.0
  }
]
```

> Los puntos vienen ordenados por tiempo (más viejo primero). Usá `_time` para el eje X y `ocupacion_pct` para el eje Y del gráfico.

---

### `GET /iot/api/motor/:id?horas=1`
Histórico de **telemetría del motor** (temperatura, vibración y corriente) a lo largo del tiempo.

```js
fetch(`${BASE_URL}/iot/api/motor/carrusel-01?horas=6`)
```

**Respuesta:**
```json
[
  {
    "_time": "2026-06-01T14:20:00Z",
    "carrusel": "carrusel-01",
    "temp_c": 39.5,
    "vibracion": 0.03,
    "corriente": 3.1
  },
  {
    "_time": "2026-06-01T14:25:00Z",
    "carrusel": "carrusel-01",
    "temp_c": 44.2,
    "vibracion": 0.06,
    "corriente": 3.8
  }
]
```

> Usá `_time` para el eje X. Podés graficar `temp_c`, `vibracion` y `corriente` como tres líneas distintas en el mismo gráfico, o en gráficos separados.

---

### `GET /iot/api/alertas?horas=24`
Historial de eventos de alerta del motor (últimas N horas).

```js
fetch(`${BASE_URL}/iot/api/alertas?horas=24`)
```

**Respuesta:**
```json
[
  {
    "_time": "2026-06-01T14:30:00Z",
    "carrusel": "carrusel-01",
    "estado": "ALERTA",
    "temp_c": 52.4,
    "vibracion": 0.13,
    "alerta": true
  }
]
```

---

## 3. WebSocket — actualizaciones en tiempo real

El WebSocket envía un mensaje **cada vez que llega un dato nuevo del ESP32**, sin necesidad de recargar la página.

```js
const wsUrl = BASE_URL.replace('https://', 'wss://') + '/iot/ws';
const ws = new WebSocket(wsUrl);

ws.onmessage = (event) => {
  const msg = JSON.parse(event.data);

  if (msg.type === 'telemetria') {
    // msg.payload tiene la misma estructura que un item de /carruseles
    const datos = msg.payload;
    console.log(datos.carrusel, datos.libres, datos.motor.estado);
    // → actualizar cards, grilla de slots y valores del motor en tiempo real
  }

  if (msg.type === 'estado_inicial') {
    // Se recibe al conectar por primera vez
    // msg.payload = objeto con todos los carruseles activos
    console.log(msg.payload);
  }
};

ws.onerror = (e) => console.error('WS error', e);
ws.onclose = () => setTimeout(() => location.reload(), 3000); // reconectar
```

---

## 4. Gráficos temporales — cómo usarlos

Se recomienda usar **Chart.js** (librería gratuita, se incluye con un solo `<script>`):

```html
<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
```

### Gráfico de ocupación (% en el tiempo)
```js
// 1. Obtener datos históricos
const datos = await fetch(`${BASE_URL}/iot/api/historial/carrusel-01?horas=6`).then(r => r.json());

// 2. Preparar ejes
const labels = datos.map(d => new Date(d._time).toLocaleTimeString());
const values = datos.map(d => d.ocupacion_pct);

// 3. Crear gráfico
new Chart(document.getElementById('grafico-ocupacion'), {
  type: 'line',
  data: {
    labels,
    datasets: [{
      label: '% Ocupación — Carrusel 01',
      data: values,
      borderColor: '#3b82f6',
      backgroundColor: 'rgba(59,130,246,0.1)',
      fill: true,
      tension: 0.4
    }]
  },
  options: { scales: { y: { min: 0, max: 100 } } }
});
```

### Gráfico de temperatura del motor
```js
const motorData = await fetch(`${BASE_URL}/iot/api/motor/carrusel-01?horas=6`).then(r => r.json());

new Chart(document.getElementById('grafico-motor'), {
  type: 'line',
  data: {
    labels: motorData.map(d => new Date(d._time).toLocaleTimeString()),
    datasets: [
      {
        label: 'Temperatura (°C)',
        data: motorData.map(d => d.temp_c),
        borderColor: '#ef4444',
        tension: 0.4
      },
      {
        label: 'Vibración',
        data: motorData.map(d => d.vibracion),
        borderColor: '#f59e0b',
        tension: 0.4
      },
      {
        label: 'Corriente (A)',
        data: motorData.map(d => d.corriente),
        borderColor: '#8b5cf6',
        tension: 0.4
      }
    ]
  }
});
```

---

## 5. Reglas de visualización

| Valor del campo | Significado | Color sugerido |
|---|---|---|
| `slots["N"] === "libre"` | Slot disponible | 🟢 Verde |
| `slots["N"] === "ocupado"` | Slot ocupado | 🔴 Rojo |
| `motor.estado === "NORMAL"` | Motor funcionando bien | 🟢 Verde |
| `motor.estado === "ALERTA"` | Temperatura o vibración alta | 🔴 Rojo |
| `alerta === true` | Carrusel con problema activo | Badge / borde rojo |
| `ocupacion_pct` | Porcentaje ocupado (0 – 100) | Barra de progreso |

---

## 6. Datos dummy para desarrollar sin el servidor

Guardá este bloque en tu HTML o en un archivo `mock-api.js`:

```js
// Datos dummy — simulan las respuestas del servidor real
const MOCK = {
  carruseles: [
    {
      carrusel: "carrusel-01", libres: 3, ocupados: 5, total: 8,
      ocupacion_pct: 62.5, alerta: false,
      receivedAt: new Date().toISOString(),
      slots: {
        "1":"ocupado","2":"libre","3":"ocupado","4":"ocupado",
        "5":"libre","6":"ocupado","7":"libre","8":"ocupado"
      },
      motor: { temp_c: 41.2, vibracion: 0.04, corriente: 3.2, estado: "NORMAL", alerta: false }
    },
    {
      carrusel: "carrusel-02", libres: 7, ocupados: 1, total: 8,
      ocupacion_pct: 12.5, alerta: false,
      receivedAt: new Date().toISOString(),
      slots: {
        "1":"libre","2":"libre","3":"libre","4":"libre",
        "5":"libre","6":"libre","7":"ocupado","8":"libre"
      },
      motor: { temp_c: 38.5, vibracion: 0.02, corriente: 2.8, estado: "NORMAL", alerta: false }
    },
    {
      carrusel: "carrusel-03", libres: 0, ocupados: 8, total: 8,
      ocupacion_pct: 100, alerta: true,
      receivedAt: new Date().toISOString(),
      slots: {
        "1":"ocupado","2":"ocupado","3":"ocupado","4":"ocupado",
        "5":"ocupado","6":"ocupado","7":"ocupado","8":"ocupado"
      },
      motor: { temp_c: 54.1, vibracion: 0.14, corriente: 5.1, estado: "ALERTA", alerta: true }
    }
  ],

  // Datos históricos dummy para los gráficos (últimas 6 horas simuladas)
  historial: {
    "carrusel-01": [
      { _time: "2026-06-01T08:00:00Z", ocupacion_pct: 25,  libres: 6, ocupados: 2 },
      { _time: "2026-06-01T09:00:00Z", ocupacion_pct: 37.5,libres: 5, ocupados: 3 },
      { _time: "2026-06-01T10:00:00Z", ocupacion_pct: 62.5,libres: 3, ocupados: 5 },
      { _time: "2026-06-01T11:00:00Z", ocupacion_pct: 75,  libres: 2, ocupados: 6 },
      { _time: "2026-06-01T12:00:00Z", ocupacion_pct: 50,  libres: 4, ocupados: 4 },
      { _time: "2026-06-01T13:00:00Z", ocupacion_pct: 62.5,libres: 3, ocupados: 5 }
    ],
    "carrusel-02": [
      { _time: "2026-06-01T08:00:00Z", ocupacion_pct: 0,   libres: 8, ocupados: 0 },
      { _time: "2026-06-01T09:00:00Z", ocupacion_pct: 12.5,libres: 7, ocupados: 1 },
      { _time: "2026-06-01T10:00:00Z", ocupacion_pct: 25,  libres: 6, ocupados: 2 },
      { _time: "2026-06-01T11:00:00Z", ocupacion_pct: 12.5,libres: 7, ocupados: 1 },
      { _time: "2026-06-01T12:00:00Z", ocupacion_pct: 0,   libres: 8, ocupados: 0 },
      { _time: "2026-06-01T13:00:00Z", ocupacion_pct: 12.5,libres: 7, ocupados: 1 }
    ],
    "carrusel-03": [
      { _time: "2026-06-01T08:00:00Z", ocupacion_pct: 75,  libres: 2, ocupados: 6 },
      { _time: "2026-06-01T09:00:00Z", ocupacion_pct: 87.5,libres: 1, ocupados: 7 },
      { _time: "2026-06-01T10:00:00Z", ocupacion_pct: 100, libres: 0, ocupados: 8 },
      { _time: "2026-06-01T11:00:00Z", ocupacion_pct: 100, libres: 0, ocupados: 8 },
      { _time: "2026-06-01T12:00:00Z", ocupacion_pct: 87.5,libres: 1, ocupados: 7 },
      { _time: "2026-06-01T13:00:00Z", ocupacion_pct: 100, libres: 0, ocupados: 8 }
    ]
  },

  // Historial del motor dummy
  motor: {
    "carrusel-01": [
      { _time: "2026-06-01T08:00:00Z", temp_c: 38.0, vibracion: 0.02, corriente: 2.9 },
      { _time: "2026-06-01T09:00:00Z", temp_c: 39.5, vibracion: 0.03, corriente: 3.1 },
      { _time: "2026-06-01T10:00:00Z", temp_c: 41.2, vibracion: 0.04, corriente: 3.4 },
      { _time: "2026-06-01T11:00:00Z", temp_c: 44.8, vibracion: 0.06, corriente: 3.8 },
      { _time: "2026-06-01T12:00:00Z", temp_c: 48.3, vibracion: 0.09, corriente: 4.2 },
      { _time: "2026-06-01T13:00:00Z", temp_c: 52.4, vibracion: 0.14, corriente: 5.1 }
    ]
  }
};

// Función para simular las llamadas al servidor
async function fetchMock(endpoint) {
  const id = endpoint.match(/carrusel-0[123]/)?.[0];
  if (endpoint.includes('/iot/api/carruseles'))   return { total: 3, carruseles: MOCK.carruseles };
  if (endpoint.includes('/iot/api/disponibilidad')) return MOCK.carruseles;
  if (endpoint.includes('/iot/api/historial'))    return MOCK.historial[id] || [];
  if (endpoint.includes('/iot/api/motor'))        return MOCK.motor[id]    || [];
  if (endpoint.includes('/iot/api/alertas'))      return [];
  return [];
}
```

---

## 7. Cómo conectar el servidor real cuando Enzo alce el dashboard

Solo cambiás **una función**:

```js
// Durante desarrollo — datos dummy:
async function apiFetch(endpoint) {
  return fetchMock(endpoint);
}

// En producción — servidor real (cambiar esto cuando Enzo te confirme):
async function apiFetch(endpoint) {
  return fetch(`${BASE_URL}${endpoint}`).then(r => r.json());
}
```

---

## 8. Checklist del dashboard

- [ ] **Cards** con los 3 carruseles: libres / ocupados / % ocupación + barra de progreso
- [ ] **Grilla de 8 slots** por carrusel — 🟢 verde = libre, 🔴 rojo = ocupado
- [ ] **Panel del motor** por carrusel: temperatura, vibración, corriente, estado NORMAL / ALERTA
- [ ] **Badge de alerta** visible si `alerta === true`
- [ ] **Actualización automática** vía WebSocket o `setInterval` cada 5 segundos
- [ ] **Gráfico de ocupación % en el tiempo** por carrusel (datos de `/iot/api/historial/:id`)
- [ ] **Gráfico de temperatura del motor en el tiempo** (datos de `/iot/api/motor/:id`)
- [ ] **Gráfico de vibración y corriente** en el mismo gráfico que temperatura o separado
- [ ] **Log o tabla de alertas** (datos de `/iot/api/alertas?horas=24`)

---

## 9. Prompt completo para generar el dashboard con IA

Copiá esto y pegalo en Claude o ChatGPT:

```
Creá un dashboard web completo (un solo archivo index.html con CSS y JS incluidos)
para un sistema de estacionamiento tipo carrusel IoT.

El sistema tiene 3 carruseles (carrusel-01, carrusel-02, carrusel-03), cada uno con 8 slots.

SECCIONES QUE DEBE TENER EL DASHBOARD:

1. HEADER: título "Dashboard — Estacionamiento Carrusel IoT" y hora actual

2. RESUMEN GENERAL (3 cards, una por carrusel):
   - Nombre del carrusel
   - Slots libres / ocupados / total
   - Barra de progreso con % de ocupación
   - Badge rojo "⚠ ALERTA" si alerta === true
   - Temperatura del motor y estado (NORMAL en verde, ALERTA en rojo)

3. GRILLA DE SLOTS (por cada carrusel):
   - 8 casillas numeradas del 1 al 8
   - Verde con "LIBRE" si slots[N] === "libre"
   - Rojo con "OCUPADO" si slots[N] === "ocupado"

4. PANEL DEL MOTOR (por cada carrusel):
   - Temperatura (°C) con indicador de color
   - Vibración (valor numérico)
   - Corriente (A)
   - Estado: "NORMAL" (verde) o "ALERTA" (rojo)

5. GRÁFICOS TEMPORALES (usar Chart.js desde CDN):
   - Gráfico de línea: % ocupación en el tiempo para cada carrusel
     → datos de GET /iot/api/historial/:id?horas=6
     → eje X: hora, eje Y: 0-100%
   - Gráfico de línea: temperatura del motor en el tiempo
     → datos de GET /iot/api/motor/:id?horas=6
     → mostrar temp_c, vibracion y corriente como 3 líneas

6. TABLA DE ALERTAS:
   → datos de GET /iot/api/alertas?horas=24
   → columnas: hora, carrusel, temperatura, vibración, estado

DATOS Y CONEXIÓN:
- Usar los datos dummy que te adjunto mientras se desarrolla
- La actualización en tiempo real se hace con WebSocket en wss://[BASE_URL]/iot/ws
  que manda { type: "telemetria", payload: { carrusel, libres, slots, motor, alerta } }
- Una sola variable BASE_URL para cambiar entre dummy y producción

DISEÑO:
- Tema oscuro (#0f172a fondo, #1e293b cards)
- Tipografía moderna (Inter o similar)
- Responsive (funciona en desktop y tablet)
- Usar Chart.js desde CDN para los gráficos

DATOS DUMMY (para desarrollar sin servidor):
[pegá aquí el objeto MOCK completo del punto 6 de esta guía]
```

---

## 10. Estructura de archivos para entregar a Enzo

Cuando el dashboard esté listo, mandárselo así:

```
dashboard/
├── index.html      ← archivo principal (puede ser todo en uno)
├── style.css       ← estilos (o incluidos en el HTML)
└── app.js          ← lógica (o incluida en el HTML)
```

Enzo lo sube al servidor y queda disponible públicamente en la URL de Cloudflare.
