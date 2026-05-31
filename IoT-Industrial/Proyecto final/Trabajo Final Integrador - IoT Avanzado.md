# Trabajo Final Integrador — IoT Avanzado

## Descripción general

Como cierre del curso IoT Avanzado, cada estudiante o equipo deberá desarrollar y presentar un proyecto integrador de solución IoT aplicada a un sector específico. El trabajo debe demostrar la capacidad de diseñar una solución IoT completa, desde la identificación de un problema real hasta la propuesta técnica, arquitectura, flujo de datos, seguridad, análisis ético y caso de negocio.

El proyecto puede estar orientado a sectores como:

- manufactura inteligente;
- agricultura de precisión;
- logística y cadena de suministro
- salud conectada;
- energía y edificios inteligentes;
- ciudades inteligentes;
- telecomunicaciones;
- otro sector relevante, previa justificación.

El trabajo podrá realizarse de manera individual o en equipos de hasta 2 personas.

**Fecha tope de entrega:** 05/06 hasta las 23:59 hs.

## Objetivos del trabajo

El Trabajo Final Integrador busca que el estudiante sea capaz de:

- Identificar un problema real en una industria, organización o sector.
- Diseñar una solución IoT aplicable al problema identificado.
- Proponer una arquitectura IoT completa basada en el modelo de 4 capas:
  - dispositivos/sensores;
  - conectividad/red;
  - plataforma/procesamiento;
  - aplicación/visualización.
- Justificar la selección de sensores, protocolo de comunicación y plataforma IoT.
- Diseñar el flujo de datos de extremo a extremo.
- Analizar riesgos de seguridad y privacidad.
- Incorporar una reflexión ética sobre el uso de datos, impacto social, privacidad e inclusión.
- Elaborar un caso de negocio básico con CAPEX, OPEX, ROI estimado y KPIs.
- Implementar un prototipo funcional o simulación que demuestre el concepto de la solución.

## Requisitos mínimos del proyecto

El trabajo deberá incluir, como mínimo:

### 1. Problema y contexto

Describir claramente:

- el sector o industria elegida;
- el problema que se busca resolver;
- los usuarios o actores involucrados;
- el impacto esperado de la solución;
- la justificación de por qué IoT es una alternativa adecuada.

### 2. Arquitectura IoT completa

Incluir un diagrama de arquitectura que muestre:

- sensores o dispositivos utilizados;
- microcontrolador, gateway o nodo IoT;
- broker, plataforma IoT o sistema de almacenamiento;
- dashboard, aplicación o sistema de visualización.

### 3. Selección tecnológica justificada

Justificar la elección de:

- sensores;
- actuadores, si aplica.

### 4. Flujo de datos

Explicar cómo viajan los datos:

`Sensor → Nodo IoT → Red → Broker/Plataforma → Base de datos → Dashboard/Usuario`

También se debe indicar (si aplica):

- frecuencia de medición;
- formato del payload;
- ejemplo de mensaje JSON;
- variables medidas;
- acciones o alertas generadas.

### 5. Seguridad y privacidad

El trabajo debe incluir una matriz básica de riesgos de seguridad. Puede utilizarse el modelo STRIDE u otro criterio equivalente.

Se deben analizar, como mínimo 3:

- autenticación de dispositivos;
- cifrado de comunicaciones;
- gestión de credenciales;
- segmentación de red;
- protección de datos;
- riesgos de acceso no autorizado;
- medidas de mitigación propuestas.

### 6. Reflexión ética

Incluir una reflexión breve sobre, al menos 2:

- qué datos se recolectan;
- si existen datos sensibles;
- quién puede acceder a la información;
- cuánto tiempo deberían almacenarse los datos;
- riesgos de vigilancia o uso secundario;
- impacto ambiental o social;
- cómo se protegería la privacidad de los usuarios.

### 7. Caso de negocio

Incluir una estimación básica de:

- CAPEX: inversión inicial estimada;
- OPEX: costos operativos recurrentes;
- posible retorno de inversión;
- impacto operativo, económico o social.

Ejemplos de KPIs:

- reducción de consumo energético;
- reducción de fallas;
- mejora de disponibilidad;
- ahorro de agua;
- reducción de pérdidas logísticas;
- disminución de tiempos de respuesta;
- mejora de productividad;
- reducción de emisiones;
- mejora en calidad o trazabilidad.

### 8. Prototipo funcional o simulación

El proyecto debe incluir una demostración funcional, que puede ser:

- simulación en Wokwi;
- nodo ESP32 publicando datos por MQTT;
- dashboard en ThingsBoard u otra plataforma;
- análisis de datos en Python/Colab.

Como mínimo, el prototipo debe demostrar:

- lectura o simulación de al menos una variable;
- publicación o almacenamiento de datos;
- visualización en dashboard o consola;
- generación de una alerta o clasificación simple.

## Entregables esperados

Cada estudiante o equipo deberá subir a EDUCA los siguientes entregables:

### A) Documento técnico

Archivo en formato PDF o Word, con una extensión sugerida de hasta 5 páginas, incluyendo:

- portada;
- integrantes del equipo;
- descripción del problema;
- objetivos de la solución;
- arquitectura IoT propuesta;
- selección y justificación tecnológica;
- flujo de datos;
- ejemplo de payload;
- análisis de seguridad;
- reflexión ética;
- caso de negocio;
- evidencias del prototipo;
- conclusiones.

### B) Prototipo o simulación

Subir evidencia del prototipo, que puede incluir:

- link de Wokwi;
- código fuente .ino, .py o notebook .ipynb;
- capturas del Serial Monitor;
- capturas del dashboard.

## Criterios de evaluación

El trabajo será evaluado considerando:

| Criterio | Descripción |
|---|---|
| Identificación del problema | Claridad, relevancia y justificación del caso seleccionado. |
| Arquitectura IoT | Coherencia técnica del diseño de extremo a extremo. |
| Selección tecnológica | Justificación adecuada de sensores, red, protocolo y plataforma. |
| Flujo de datos | Claridad del recorrido de la información y estructura del payload. |
| Seguridad y privacidad | Identificación de riesgos y mitigaciones realistas. |
| Reflexión ética | Consideración de privacidad, impacto social, datos sensibles e inclusión. |
| Caso de negocio | Estimación razonable de costos, beneficios, ROI y KPIs. |
| Prototipo funcional | Evidencia de simulación o implementación operativa. |
| Conclusiones | Coherencia entre problema, solución propuesta e impacto esperado. |
