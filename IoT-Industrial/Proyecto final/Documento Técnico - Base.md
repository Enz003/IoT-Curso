# Documento Técnico — Base (Fase 1)
## Sistema IoT para Estacionamiento tipo Carrusel — "Smart Carousel Parking"

**Integrantes:** Enzo Fernández + Lucas Gonzalez
**Asignatura / Sector:** IoT Industrial — vertical *Ciudades inteligentes y movilidad urbana* (Semana 5)
**Fecha de entrega:** 05/06/2026
**Configuración de la simulación:** 3 carruseles × 8 slots (24 posiciones) · Servidor **Node.js** (Express + mqtt.js + ws) · Broker **Mosquitto** · Base de datos **InfluxDB**

> *Borrador de trabajo de la Fase 1.* Reúne las secciones de **problema, contexto, objetivos y justificación de IoT**. En la Fase 6 se compaginará con el resto de las secciones (arquitectura, flujo de datos, STRIDE, ética, caso de negocio y evidencias) en el documento final (PDF/Word, ≤ 5 páginas).

---

## 1. Descripción del problema y contexto

Estacionar es uno de los cuellos de botella menos visibles, pero más costosos, de la movilidad urbana. Distintos estudios cuantifican el problema: según INRIX, un conductor promedio en Estados Unidos pierde alrededor de **17 horas al año buscando estacionamiento**, lo que equivale a unos **USD 345 anuales por conductor** en tiempo, combustible y emisiones, y a cerca de **USD 72.700 millones por año** a nivel país. En grandes ciudades la cifra se dispara: en Nueva York el promedio supera las 100 horas anuales por conductor. A esto se suma que, en zonas céntricas congestionadas con cupos mal aprovechados, los estudios clásicos de Donald Shoup encontraron que **en promedio cerca del 30 % del tránsito** corresponde a vehículos "dando vueltas" buscando lugar (*cruising for parking*), con un rango muy amplio según la ciudad. Ese tránsito improductivo agrava la congestión, eleva las emisiones y desgasta la infraestructura.

Frente a la escasez de suelo urbano, el **estacionamiento tipo carrusel** aparece como una respuesta atractiva: es una estructura rotatoria vertical que almacena varios vehículos en una superficie mínima, ideal para ciudades densas donde el metro cuadrado es caro y escaso. Sin embargo, este tipo de instalación arrastra dos problemas operativos que hoy no están resueltos:

**(a) Falta de visibilidad de la disponibilidad en tiempo real.** El conductor no tiene forma de saber, *antes de llegar*, si el carrusel más cercano tiene una posición libre. Termina trasladándose hasta el lugar para descubrir que está completo, reproduciendo exactamente el *cruising* y la congestión que la solución pretendía evitar. Del lado del operador o del municipio, tampoco existe un panel que muestre el estado de todos los carruseles de la ciudad de un vistazo.

**(b) Dependencia de un único sistema electromecánico crítico.** A diferencia de una playa convencional, el carrusel solo entrega o retira un vehículo si su **motor y mecanismo rotativo funcionan**. Si el motor falla, *toda* la estructura queda fuera de servicio y los vehículos almacenados quedan retenidos hasta la reparación. Hoy el mantenimiento de estos equipos suele ser **correctivo** (se interviene cuando la falla ya ocurrió) o **preventivo por calendario**, y no se basa en la condición real del equipo —temperatura, vibración y consumo del motor—, que es justamente lo que anticipa una avería.

**Contexto del caso.** El proyecto se sitúa en un entorno urbano (se usa Asunción como referencia en los *topics*), con un operador o municipio que administra varios carruseles distribuidos en la ciudad. La mecánica de cada carrusel **ya está gobernada por un PLC** (mundo OT) que **no se modifica**: el proyecto se concentra en la capa IoT que *lee* esas señales de forma no intrusiva y las pone en valor.

**El problema, en una frase:** no existe hoy visibilidad en tiempo real de la ocupación por posición, ni un monitoreo de la condición del motor que permita anticipar fallas, lo que se traduce en tiempo perdido y congestión para el conductor y en paradas no planificadas y costos para el operador.

---

## 2. Objetivos de la solución

**Objetivo general.** Diseñar e implementar un prototipo —simulado en Wokwi— de un sistema IoT de cuatro capas que monitoree en tiempo real la **ocupación de los slots** y la **salud del motor** de estacionamientos tipo carrusel, y que exponga esa información a sus dos usuarios clave: el **operador/municipio**, mediante un dashboard de control, y el **conductor**, mediante una web app móvil que responde "¿dónde hay lugar?".

**Objetivos específicos.**

1. Leer de forma **no intrusiva (solo lectura)** las señales que el PLC ya gestiona —estado de cada una de las 8 posiciones por carrusel— y publicarlas por MQTT en tiempo real.
2. Monitorear las variables del motor (temperatura, vibración y consumo) y clasificar su condición en tres niveles —**NORMAL / ADVERTENCIA / FALLA**— como base de un esquema de **mantenimiento predictivo**.
3. Diseñar la **arquitectura IoT de 4 capas** con **convergencia IT/OT**, usando el ESP32 como *gateway* de borde entre el mundo OT (PLC) y el mundo IT/IoT.
4. Implementar el **pipeline de datos** de extremo a extremo: ESP32 → MQTT → broker Mosquitto → servidor Node.js → InfluxDB → frontends, con **REST + WebSocket** para la actualización en vivo.
5. Construir los dos productos: un **dashboard de control** (estado de los 3 carruseles, mapa de slots, KPIs y alertas de mantenimiento) y una **web app móvil** para conductores con la disponibilidad cercana.
6. Calcular **KPIs** de ciudad inteligente y de mantenimiento: porcentaje de ocupación, disponibilidad/*uptime* del carrusel, y MTBF / MTTR del motor.
7. Incorporar **seguridad** (matriz STRIDE y MQTT sobre TLS) y una **reflexión ética** sobre los datos que el sistema recolecta.

---

## 3. Justificación de IoT (por qué IoT es la solución adecuada)

El problema planteado tiene tres características que, juntas, descartan una solución manual o aislada y apuntan directamente a una arquitectura IoT: requiere **datos continuos** provenientes de **sensores distribuidos** (cada slot y cada motor, en varios carruseles repartidos por la ciudad), exige **baja latencia** (el estado "libre/ocupado" pierde valor si llega tarde al conductor) y necesita **visualización remota** simultánea para dos audiencias distintas. Una planilla, una inspección periódica o un tablero local en cada carrusel no resuelven ninguno de los tres puntos: no son en tiempo real, no son remotos y no escalan a una red de carruseles.

Una arquitectura IoT de **cuatro capas** encaja exactamente con esas necesidades:

- **Capa de dispositivo / borde.** El ESP32 actúa como *gateway*: lee las señales del PLC, normaliza los valores, arma el JSON y aplica reglas simples en el borde (por ejemplo, marcar una alerta de motor) antes de transmitir. Procesar en el borde reduce el tráfico y la latencia.
- **Capa de conectividad / red.** El nodo publica por **MQTT**, un protocolo *pub/sub* ligero y estándar de facto en IoT, ideal para muchos sensores que emiten mensajes pequeños y frecuentes hacia un **broker Mosquitto**.
- **Capa de plataforma / procesamiento.** Un servidor **Node.js** se suscribe a los *topics*, calcula slots libres y alertas, persiste el histórico en **InfluxDB** (una base de series temporales, natural para telemetría con marca de tiempo) y sirve los datos por **REST + WebSocket**.
- **Capa de aplicación.** Los dos frontends —dashboard del operador y web app del conductor— consumen esa información y la convierten en decisiones: dónde estacionar y cuándo intervenir un motor.

A esto se suma el concepto de **convergencia IT/OT** (Semana 4), que es el corazón del diseño: la mecánica del carrusel pertenece al mundo **OT** y la gobierna un **PLC** que no se toca. El ESP32 hace de **puente hacia el mundo IT/IoT**, leyendo los registros del PLC por Modbus TCP / OPC-UA **solo en modo lectura**. Así se obtiene el dato sin alterar ni poner en riesgo el control industrial —un patrón realista y seguro, muy distinto de "cablear sensores nuevos a un microcontrolador".

Finalmente, el caso pertenece de lleno a la vertical de **ciudades inteligentes y movilidad urbana** (Semana 5): el mismo dato de ocupación que ayuda al conductor a evitar el *cruising* le sirve al municipio para medir uso, planificar y reducir congestión y emisiones. El monitoreo del motor aporta el ángulo industrial (IIoT y mantenimiento predictivo). En conjunto —tiempo real, sensores distribuidos, procesamiento en plataforma y doble visualización remota— el problema cae justo en el terreno donde IoT aporta más valor que cualquier alternativa.

---

## Fuentes (datos citados en la sección 1)

- INRIX — *Searching for Parking Costs Americans $73 Billion a Year* (2017): [inrix.com/press-releases/parking-pain-us](https://inrix.com/press-releases/parking-pain-us/) · cobertura: [GeekWire](https://www.geekwire.com/2017/inrix-study-find-americans-waste-73-billion-per-year-looking-parking-spot/)
- Donald Shoup — *Cruising for Parking* (≈30 % promedio del tránsito en zonas céntricas congestionadas): [eScholarship, UC](https://escholarship.org/uc/item/55s7079f)
