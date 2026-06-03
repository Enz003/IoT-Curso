# Análisis de Datos — Fase 5

**Smart Carousel Parking — Trabajo Final Integrador (IoT Avanzado)**
Integrantes: Enzo Fernández + Lucas González

Análisis de la telemetría del estacionamiento tipo carrusel: patrones de ocupación, uso por carrusel y detección de anomalías del motor (mantenimiento predictivo).

---

## Contenido de la carpeta

```
analisis/
├── analisis_carrusel.ipynb   ← notebook (pandas / matplotlib)
├── ocupacion.csv             ← histórico de ocupación (3 carruseles)
└── motor.csv                 ← telemetría del motor (temp / vibración / corriente)
```

---

## Qué hace el notebook

1. **Carga de datos** — usa `ocupacion.csv` y `motor.csv`. Si no existen, los genera de forma sintética (7 días, 3 carruseles, muestra cada 15 min) y los exporta.
2. **Patrones de ocupación** — ocupación promedio por hora del día, mapa de calor día × hora y % de uso por carrusel.
3. **Salud del motor + anomalías** — series de temperatura/vibración/corriente y detección combinada (**umbral absoluto + z-score** sobre media móvil) que captura una degradación progresiva.
4. **KPIs** — ocupación promedio, carrusel más usado, hora pico, % de tiempo saturado, temp. máxima y nº de anomalías.
5. **Conclusiones** redactadas.

---

## Cómo verlo / ejecutarlo

[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/Enz003/IoT-Curso/blob/main/IoT-Industrial/Proyecto%20final/analisis/analisis_carrusel.ipynb)

- **Online (nbviewer):** https://nbviewer.org/github/Enz003/IoT-Curso/blob/main/IoT-Industrial/Proyecto%20final/analisis/analisis_carrusel.ipynb
- **Local:**
  ```bash
  pip install pandas numpy matplotlib jupyter
  jupyter notebook analisis_carrusel.ipynb
  ```

> Si GitHub muestra "An error occurred" al previsualizar el `.ipynb`, es un bug del visor de GitHub, no del archivo. Usá el link de nbviewer o Colab.

---

## Usar datos reales

El notebook trae datos sintéticos para demostrar el análisis. Para correrlo sobre datos de producción, reemplazá `ocupacion.csv` y `motor.csv` por el export real de InfluxDB (vía la API del servidor):

- `GET /iot/api/historial/:id?horas=N` → ocupación (`_time`, `ocupacion_pct`, `libres`, `ocupados`)
- `GET /iot/api/motor/:id?horas=N` → motor (`_time`, `temp_c`, `vibracion`, `corriente`)

Las columnas ya coinciden con lo que espera el notebook, así que no hay que tocar el código.
