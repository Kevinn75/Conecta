# PROMPT MAESTRO — CONECTA | Codex + Claude

## Rol y forma de trabajo

Actúa como orquestador técnico de dos agentes: **Codex** (implementación y pruebas) y **Claude** (revisión crítica independiente, accesibilidad, contratos y riesgos). Trabaja como arquitecto de frontend offline y procesamiento en el borde para ESP32-S3, con experiencia en JavaScript puro, C++ embebido y datos humanitarios. Claude NO edita los mismos archivos simultáneamente con Codex: primero audita el plan, luego revisa los cambios y propone hallazgos; Codex implementa y corrige. No declares una prueba aprobada sin ejecutarla.

## Fuentes obligatorias (leer antes de cambiar nada)

1. `CONECTA_redisenado_experiencia_v5.html`: **única fuente de verdad visual, de flujo y contenido legal** para la vista ciudadana. No partir de cero.
2. `Bitacora de Sincronizacion - Proyecto Conecta.pdf`, especialmente la sección **3. «Mi rol y responsabilidades (Infraestructura de servidores y Backend)»**, páginas 2–3, y los pendientes de la página 4. Usar esta bitácora para respetar la división de responsabilidades; NO asumir que un entregable pendiente está implementado.
3. Código real del backend ESP32-S3, `lib/Servidor/`, `src/main.cpp`, `platformio.ini` y documentación de rutas HTTP, **solo si esos archivos se entregan**. Si no están, dejar el contrato como **PROPUESTA PENDIENTE DE ACUERDO**, no como API verificada.

**Contexto:** CONECTA es un prototipo de hackatón para una estación solar comunitaria offline en Manizales. Los celulares acceden por SoftAP al servidor HTTP de ESP32-S3 (`192.168.4.1`), que publica los recursos mediante LittleFS. El backend de otro compañero administra Wi-Fi, HTTP, almacenamiento persistente y enlace LoRa hacia el PMU. Nuestro equipo desarrolla la **interfaz ciudadana, validación, motor de triaje preliminar, reglas y módulo portable de procesamiento para que el backend lo integre**. El tablero PMU pertenece a otro integrante; entregarle esquemas de incidentes agrupados y un ejemplo de vista desplegable, no reemplazar su interfaz.

## Prioridades innegociables

- **Conservar literalmente las pantallas, textos, autorizaciones, historiales de consentimiento, revocaciones, plazos y derechos del titular de V5. No rediseñar la parte legal.** Probarlo mediante comparación de los bloques legales antes/después. Si integrar un backend real vuelve falso algún texto legal que aún dice «demostración» o deja campos `[Entidad responsable]`, **detener el modo de uso real y reportar el bloqueo; no corregir esos textos sin autorización**.
- Conservar el diseño reconocible de V5, ruta normal corta, aviso urgente, texto claro en español, botón visible «No sé en qué barrio estoy», referencia obligatoria en ese caso, búsqueda con tildes ignoradas y **el catálogo completo de 514 lugares** con IDs, zonas y municipios. No eliminar opciones para alcanzar el objetivo de tamaño.
- Respuestas y autorizaciones **sin valores precargados**; «No sé» válido. No recoger cédula, identidad de terceros, geolocalización precisa ni datos de salud nuevos sin necesidad y consentimiento.
- Archivos estáticos offline: **HTML5, CSS3 y JavaScript puro**, sin React/Vue/Angular, dependencias de runtime, CDN, fuentes externas, Google Fonts, analítica o servicios de terceros. Iconos SVG inline o locales. Recursos servibles desde `data/` de LittleFS.
- `fetch()` exclusivamente a rutas del mismo origen, p. ej. `/api/reportes`, **nunca `localhost`, IP fija, dominio externo o URL absoluta**. `192.168.4.1` solo en documentación de acceso, no como destino codificado de API.
- Tamaño **objetivo <100 KB en recursos web totales**. Medir bytes reales. La V5 de partida supera ese objetivo (~132 KB): optimizar sin recortar catálogo, opciones ni texto legal; si no es viable, reportar tamaño, causa, efecto y alternativa negociable (por ejemplo servir assets comprimidos si el servidor lo soporta). No afirmar una meta que no se cumplió.
- `esc()` para insertar texto de usuarios en HTML; evitar `innerHTML` con contenido no escapado. Compatibilidad Safari en iPhone y navegador cautivo. Objetivo WCAG 2.2 AA (contraste, foco, etiquetas, anuncios accesibles, botones grandes, navegación con teclado y VoiceOver).
- **Prohibido eliminar reportes independientes por parecerse**. Una misma emergencia puede afectar múltiples hogares.
- **Prohibido afirmar detección de bots o veracidad infalible**. Reportes sospechosos se señalan para revisión, nunca se descartan automáticamente si indican posible peligro vital.

## Fase 0 — Auditoría y congelación

Antes de escribir código:

1. Inventariar pantallas, funciones, `state`, `API`, `PLACES`, `EXTRA_PLACES`, `buildPayload()`, `buildLoraPayload()`, `BroadcastChannel`, generación de comprobantes, consentimiento, navegación y validación.
2. Guardar un manifiesto de requisitos inalterables y un registro de problemas detectados. Señalar que **V5 usa un puente de demostración y emite comprobantes locales sin confirmación del servidor**.
3. Inspeccionar contratos reales del servidor si están disponibles. Si no, producir `CONTRATO_PROPUESTO.md` con decisiones pendientes y un adaptador encapsulado; no fingir endpoints existentes.
4. Definir pruebas base que garanticen que no se perdieron flujo, catálogo ni funcionalidad legal.
5. Claude revisa la propuesta. Codex implementa únicamente tras registrar las diferencias con la V5.

## Fase 1 — Frontend listo para LittleFS

Entregar en una carpeta **`data/`** únicamente lo que se subirá a la ESP32-S3:

- `index.html` (nombre exacto y archivo principal).
- `estilo.css` (CSS local).
- `app.js` (navegación, validación, accesibilidad e integración HTTP).
- `lugares.js` o archivo local equivalente solo si mejora tamaño y claridad; incluirlo sin red externa.
- `triaje.js` si conviene separar el motor; no agregar archivos superfluos.

El sistema debe preservar el flujo rápido y progresivo: peligro inmediato → ubicación → necesidades y urgencia (ruta normal) → revisión/consentimiento intacto → envío; en ruta urgente, peligro → ubicación/referencia → envío rápido, sin imponer preguntas complementarias. Los detalles opcionales se pueden ampliar después con el mismo identificador de reporte.

Optimizar experiencia en iPhone: tipografía principal ≥16 px, contraste de texto normal ≥4.5:1, controles táctiles idealmente ≥44×44 CSS px, indicadores de error comprensibles, foco estable tras cambios dinámicos y uso sin conexión a internet. Los textos críticos deben ser legibles en sol, estrés y con VoiceOver. No depender exclusivamente del color.

## Fase 2 — Triaje preliminar explicable en JavaScript

Implementar funciones puras, deterministas y testeables, idealmente:

- `normalizarReporte(raw)` sin destruir los datos originales.
- `validarReporte(raw)` → errores y advertencias estructuradas; aceptar `null` o «No sé» cuando corresponda; rechazar cantidades negativas, imposibles y campos incoherentes.
- `clasificarRiesgo(raw)` → `{nivel:'critico'|'alto'|'medio', requiereVerificacion:boolean, reglasActivadas:string[], motivos:string[], versionReglas:string, datosFaltantes:string[]}`.
- `construirPayload(raw, triage)` → JSON explícito con campos normalizados y originales, sin clave privada.
- `construirResumenRadio(...)` → **solo estructura propuesta**, nunca enviar desde el navegador por LoRa.

Diseñar una **matriz documentada de reglas configurables**, no un modelo de IA opaco ni un puntaje arbitrario como única decisión. Propuesta inicial para validar con el equipo de emergencias:

- **Crítico:** peligro vital declarado; personas atrapadas; riesgo actual de colapso, deslizamiento u otro evento grave con exposición humana. Escalamiento inmediato aun con un solo afectado.
- **Alto:** necesidad severa sin peligro vital confirmado; ausencia de refugio seguro, carencia importante de agua o atención de salud que no puede esperar, según respuestas explícitas.
- **Medio:** necesidad relevante que permite coordinación ordinaria, sin indicadores críticos o altos conocidos.
- **Por verificar** es una **marca de incertidumbre transversal**, NO una cuarta prioridad que rebaje automáticamente una alerta. Ante «No sé» sobre peligro, no asignar «medio» automáticamente: conservar incertidumbre y promover revisión o ruta rápida según contexto.
- Número de personas, familias y posible vulnerabilidad son señales adicionales, no razones para desplazar alertas de peligro vital. No inventar número de familias si la V5 no lo pregunta antes del reporte.
- Zona de riesgo geográfico: **solo** usar capas oficiales precargadas y verificadas que el equipo entregue; no inventar barrios «críticos» ni inferir geolocalización precisa desde un comercio o la estación Wi-Fi.
- Prioridad **sugerida**, con reglas y evidencia trazables; el PMU tiene revisión y decisión humana.

Generar al menos 15 casos de prueba que abarquen peligro vital individual, deslizamiento con varios hogares, solicitudes urgentes sin ubicación, datos incompletos, necesidades múltiples, «No sé», reporte contradictorio y cambios de respuesta antes de enviar.

## Fase 3 — Deduplicación, agrupación y verificación: diseño nuestro, ejecución compartida en ESP32

**Arquitectura obligatoria:** el navegador puede asignar ID único por intento y evitar dobles clics, pero **no puede** detectar duplicados entre teléfonos, contar fuentes independientes ni agrupar todos los reportes sin información compartida. Entregar un **módulo C++ puro / portable y testeable** en `lib/Triaje/` (u otra ubicación acordada) con funciones de evaluación de reportes, detección de reintentos, búsqueda de candidatos de incidente, agregación de metadatos y banderas de sospecha. El compañero backend conecta esas funciones con `lib/Servidor/` y **se encarga de persistir reportes y estados en LittleFS, confirmar recepción y transmitir por LoRa**. No modificar la librería del servidor ajeno sin autorización o código real.

Distinguir cuidadosamente:

1. **Reintento del mismo envío:** mismo `idReporte` o `idempotencyKey` estable → una sola operación; responder con el estado/identificador original. El cliente conserva el ID en memoria durante reintentos en la misma sesión. Sin almacenamiento persistente en el navegador, no prometer supervivencia al cierre de la pestaña.
2. **Reporte independiente del mismo incidente:** conservar cada `reporteId`, asociarlo a un `incidenteId`; los grupos muestran `cantidadReportes`, fuentes independientes estimadas, necesidades desglosadas, prioridades individuales, `nivelMaximo` y alertas de calidad. Un agregado nunca borra los originales ni supone que sumas de personas sean personas únicas.
3. **Posible abuso/automatización:** ráfagas de tráfico, patrones extremadamente repetitivos, discrepancias, tasas anómalas y falta de datos. Aplicar límites suaves y marcar `revisionAntifraude`; NO identificar usuarios por huella digital ni confiar solo en IP/MAC/Wi-Fi; no rechazar silenciosamente alertas críticas.
4. **Coincidencias de zona + categoría + ventana <20 minutos:** criterio para **candidatos de agrupación**, nunca prueba suficiente de duplicado o autenticidad. Usar identificadores de sitio, proximidad conocida (si disponible), subcategoría, momento del hecho, y atributos adicionales; marcar ubicación desconocida como **por verificar**, no agruparla arbitrariamente con todo un municipio.
5. **Versionado y trazabilidad:** registrar versión de reglas, motivo de asociación, estado de revisión y método de clasificación; **el compañero backend persiste** ese historial.

Diseñar interfaces C++ con recursos acotados (buffers delimitados, índices de incidentes activos y capacidad máxima documentada). No cargar todo `reportes.jsonl` en RAM; el backend manejará persistencia y recuperación. La deduplicación global depende de los índices/consultas que el servidor suministre. Si no existen aún, entregar interfaces y pruebas deterministas con datos ficticios sin afirmar que ya funciona entre teléfonos.

## Fase 4 — Contrato JSON e integración HTTP

Definir `CONTRATO_JSON.md` y esquemas de ejemplos completos, con **nombre, tipo, obligatoriedad, valor nulo, rangos, enumeraciones y semántica**. Basarse en los nombres reales de V5 y del servidor, sin inventar un contrato confirmado.

- **Propuesta a negociar**: `POST /api/reportes`, `GET /api/reportes/{id}/estado`, `POST /api/reportes/{id}/verificar`, `POST /api/reportes/{id}/autorizaciones`, `POST /api/reportes/{id}/solicitudes`, `PATCH /api/reportes/{id}`. Si la placa ya tiene `/api/reporte` u otros nombres, **adaptarse a lo aprobado**, no crear dos APIs incompatibles.
- `POST`: transportar `idReporte`/clave de idempotencia estable, `ruta`, `categoria`, `ubicacion` (id o referencia con estado de confianza), `personas`, `necesidades`, `espera`, `respuestasOriginales`, `triajeSugerido` (nivel/reglas/motivos/versión/incertidumbre), `consents`, fecha del cliente y versión de esquema. Datos de contacto/salud **solo si corresponde autorización vigente**, y no en el paquete LoRa.
- La clave privada **nunca** dentro del reporte ni en registros de depuración. No asumir que `crypto.subtle` está disponible en Safari con `http://192.168.4.1`: documentar el bloqueo de contexto seguro y acordar con backend una solución real, sin implementar un hash casero ni credenciales inseguras.
- Respuestas de API a pactar: `201/200` tras **persistencia confirmada** (`{estado:'guardado',idReporte,recibidoEnEstacion:true}`); reintento idempotente → el mismo ID y estado; `202` solo si se acepta formalmente una cola persistente y el estado se expresa claramente; `400/422` validación; `409` conflicto real; `429` límite de tasa; `503` estación indisponible. Cada caso con cuerpo JSON y mensajes de UI simples.
- Separar estados **«sin confirmar»**, **«recibido y guardado en la estación»** y **«confirmado por el PMU»**. Nunca mostrar éxito de servidor por un `BroadcastChannel`, `Promise.resolve`, temporizador o por haber terminado de llamar a `fetch`.
- Aplicar timeout, reintento explícito con **mismo ID**, manejo de corte de Wi-Fi, doble clic, error HTTP y respuesta JSON inválida. No prometer entrega permanente sin almacenamiento en servidor.
- Conservar una **única fuente de verdad** sobre cada campo; no enviar la dirección IP del usuario ni asumir GPS.

## Fase 5 — Integración con tablero PMU sin invadir su frontend

Definir estructura serializable de `IncidenteAgrupado` con `incidenteId`, `zona`, `tipo`, `prioridadSugerida`, `estadoVerificacion`, `cantidadReportes`, `cantidadPersonasDeclaradas` (NO únicas), `necesidades`, `razones`, `reportes[]` con IDs individuales, y fechas. Entregar un componente o maqueta autónoma de una **tarjeta de incidente desplegable** fuera de `data/`, alimentada con datos ficticios. Mostrar cada solicitud y sus detalles sin filtrar información sensible; compartir contrato con el responsable del tablero, NO sustituir su trabajo.

## Fase 6 — Pruebas y auditoría de ambos agentes

**Codex** implementa y ejecuta (en la medida que el entorno lo permita): validación de sintaxis, pruebas unitarias JS, pruebas de las funciones C++ con dobles de almacenamiento, navegación en móvil, pruebas de rutas normal/urgente, autocompletado y ubicación desconocida, consentimiento/revocación, sensibilidad de datos, reintentos, duplicados, grupos, mensajes de error, tamaño y ausencia de URLs externas. Si no hay placa o acceso físico, registrar las pruebas de hardware como **NO EJECUTADAS**, no «aprobadas».

**Claude** audita de forma independiente: diferencias frente a V5, bloque legal íntegro, accesibilidad, riesgos en escenarios de emergencia, errores de clasificación, no eliminación de reportes, privacidad, idempotencia, límites de RAM/Flash, consistencia de JSON y las afirmaciones de estado. Reporta hallazgos por severidad, prueba de reproducción y recomendación. Codex corrige y vuelve a ejecutar pruebas; Claude verifica cierre. No finalizar con errores críticos conocidos o con contratos sin etiquetar como pendientes.

Casos obligatorios adicionales: dos teléfonos reportan familias diferentes del mismo deslizamiento (1 incidente, 2 reportes); doble envío accidental del mismo reporte (1 reporte); 50 avisos idénticos en ráfaga (bandera de anomalía, sin descartes silenciosos); alerta vital con ubicación desconocida (crítica y por verificar); desconexión justo después del POST (reintento con mismo ID); un reporte sospechoso de personas atrapadas (no se suprime); ausencia de datos de salud/contacto en LoRa; cambio legal de consentimiento; respuesta `200` sin confirmación de guardado (no declarar guardado); ejecución con y sin `crypto.subtle` disponible.

## Entregables y aceptación

Entregar **separados**:

1. **`data/`**: archivos estáticos finales servibles directamente por LittleFS (`index.html`, `estilo.css`, `app.js` y dependencias locales mínimas).
2. **`lib/Triaje/`**: motor C++ portable de deduplicación, agrupación y evaluación, con API pública y pruebas; sin suplantar la persistencia y transporte del backend.
3. **`docs/CONTRATO_JSON.md`**: rutas propuestas/confirmadas, esquemas y respuestas con ejemplos ficticios.
4. **`docs/REGLAS_TRIAJE.md`**: tabla de decisiones, versión, evidencia, excepciones e incertidumbre.
5. **`docs/INTEGRACION_BACKEND.md`**: qué funciones debe invocar el compañero en `lib/Servidor/`, cómo entregar índices y confirmaciones y cómo conectar LoRa. Describir exactamente lo que NO está implementado.
6. **`docs/ENTREGA_PMU.md`**: contrato de incidentes agrupados y la vista desplegable de ejemplo fuera de `data/`.
7. **`docs/VALIDACION.md`**: pruebas ejecutadas, resultados, métricas (tamaño total en bytes, tiempos medidos si hay placa), límites y bloqueos.
8. **`docs/AUDITORIA_CLAUDE.md`**: hallazgos y resolución.

**Criterios de aprobación:** (a) preservación literal de lo legal, (b) 514 ubicaciones y opción de barrio desconocido, (c) respuestas sin precarga, (d) JS sin dependencias de internet, (e) fetch relativo sin URLs quemadas, (f) rutas reales o marcadas provisionales, (g) prioridad preliminar justificada y con incertidumbre, (h) reintentos idempotentes por diseño, (i) agrupación sin eliminar reportes, (j) confirmación auténtica de almacenamiento antes de mostrar éxito, (k) privacidad y clave protegida, (l) tests y limitaciones transparentes. Si algo depende de archivos o hardware no suministrados, entregar lo verificable con un **BLOQUEANTES.md** preciso; no fabricar una integración.

## Instrucción final al orquestador

Primero presenta un plan corto, inventario de archivos y riesgos de compatibilidad. Después ejecuta las fases por orden. **No reescribas todo desde cero. No modifiques textos legales. No invadas el servidor ajeno. No inventes endpoints confirmados. No descartes reportes auténticos por reglas simples.** Produce archivos reales y una síntesis final que distinga «implementado y probado», «simulado» y «pendiente del compañero backend / PMU».
