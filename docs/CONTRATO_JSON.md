# Contrato JSON de CONECTA

Diseño documental del 20 de noviembre de 2026. **PROPUESTA PENDIENTE DE ACUERDO**, excepto la existencia de la ruta indicada como CONFIRMADA. No implementa persistencia, autenticación, radio ni acuses. Ejemplos exclusivamente ficticios.

Fuentes inspeccionadas: `docs/ai/CONTEXTO.md`, `docs/ai/V5_MANIFIESTO.md`, fases 2 y 4 de `docs/referencia/CONECTA_PROMPT_MAESTRO_CODEX_CLAUDE.md`, `buildPayload()`, `buildLoraPayload()`, catálogos y estado de `docs/referencia/CONECTA_redisenado_experiencia_v5.html`, y registro HTTP en `lib/Servidor/Servidor.cpp:87`. La instrucción de esta tarea limita el trabajo a estos dos documentos. Las propuestas requieren revisión de backend y PMU.

## 1. Rutas y evidencia

Todas las rutas son relativas al mismo origen. `{id}` representa el `idReporte` codificado como un segmento de ruta. No se mantiene una API paralela en plural.

| Método y ruta | Estado | Alcance |
| --- | --- | --- |
| `POST /api/reporte` | **CONFIRMADA solo su existencia** | Hoy devuelve siempre 200 y el JSON de espera mostrado abajo, sin leer, validar ni guardar el cuerpo. La semántica de respuesta del contrato futuro **NO está confirmada**. |
| `GET /api/reporte/{id}/estado` | **PROPUESTA PENDIENTE DE ACUERDO** | Consultar recepción en estación y eventual acuse PMU; no revelar datos personales. |
| `POST /api/reporte/{id}/verificar` | **PROPUESTA PENDIENTE DE ACUERDO** | Verificación en servidor, protocolo y protección pendientes. No habilitar con la comparación local de V5. |
| `POST /api/reporte/{id}/autorizaciones` | **PROPUESTA PENDIENTE DE ACUERDO** | Registrar otorgamientos y revocaciones con historial, tras verificación segura. |
| `POST /api/reporte/{id}/solicitudes` | **PROPUESTA PENDIENTE DE ACUERDO** | Solicitudes del titular y acuse persistente; no simular su recepción. |
| `PATCH /api/reporte/{id}` | **PROPUESTA PENDIENTE DE ACUERDO** | Ampliación o corrección autorizada con control de versión; conservar identidad e historial. |

Los métodos de `API` de V5 usan `BroadcastChannel` y promesas locales. Sus comentarios no acreditan rutas reales. Cuerpos y autenticación de las rutas adicionales quedan pendientes; este documento no autoriza usarlas.

## 2. Cuerpo propuesto del POST

`Content-Type: application/json`. `versionEsquema = "conecta-reporte-1"` es una versión propuesta, no negociada. Las claves siguientes conservan los nombres reales de `buildPayload()`, salvo las dos extensiones explícitas `versionEsquema` y `triajeSugerido`. No se añaden sinónimos raíz como `espera`, `needs` o `reporteId`.

Los límites nuevos son parámetros propuestos, no capacidades verificadas de la ESP32. Se propone un máximo de 16 KiB UTF-8 por cuerpo, a negociar con backend. Los máximos de texto se expresan en caracteres; aplicar también el máximo total en bytes. Rechazar valores fuera del dominio con errores por campo, nunca truncar silenciosamente una alerta. No convertir ausencia a cero ni a «No».

| Nombre | Tipo | Obligatorio | Null permitido | Rango/enumeración | Semántica |
| --- | --- | --- | --- | --- | --- |
| `idReporte` | string | Sí | No | 1–64; letras ASCII, dígitos, `_`, `-` | ID estable creado antes del primer envío; también clave de idempotencia. No es una credencial. |
| `torre` | string | Sí | No | 1–6; mayúsculas ASCII y dígitos, propuesto | Identificador de estación; V5 toma `TOWER_ID`. Dato declarado por cliente, no prueba de origen. |
| `ruta` | string | Sí | No | `urgente`, `normal` | V5 usa urgente solo si `danger === 'Sí'`. |
| `categoria` | string | Sí | No | `derrumbe`, `volcanica`, `terremoto`, `atrapadas`, `otro`, `necesidades` | Códigos reales de HAZARDS; normal usa `necesidades`. |
| `ubicacion` | objeto | Sí | No | Variantes de §3 | Lugar declarado; no GPS ni IP del usuario. |
| `referencia` | string | Sí | Sí | 1–160 si presente; mínimo 6 sin ubicación conocida | Descripción del lugar, sin nombres ni salud. |
| `personas` | entero | Sí | Sí | 1–10000 propuesto | Estimación declarada; null significa cantidad desconocida. Límite sujeto a revisión operativa. |
| `quienIncluye` | string | Sí | Sí | `Solo a mí`, `A mi hogar`, `A varias familias`, `A otra persona o grupo`, `No sé` | `state.who`; urgente envía null. No representa una cantidad de familias. |
| `observado` | string | Sí | Sí | `Ahora`, `Me lo contaron`, `No sé`, o `Hace aproximadamente` seguido de opción temporal | Opciones: `10 minutos`, `15 minutos`, `30 minutos`, `31 a 60 minutos`, `Más de una hora`. Vacío de V5 se normaliza a null. |
| `necesidades` | array de string | Sí | No | Sin repetidos, 0–6; catálogo N | `state.needs`; urgente puede usar `[]`, normal requiere al menos una opción. |
| `prioridad` | objeto | Sí | No | §3 | Respuestas y textos derivados de V5; no decisión del PMU. |
| `apoyos` | array de string | Sí | No | Sin repetidos, 0–4; catálogo A | Apoyos opcionales; no diagnóstico ni prueba de vulnerabilidad. |
| `otraNecesidad` | string | Sí | Sí | 1–100; solo con `Otra` | No contiene nombres, contactos ni datos de salud. Null si no corresponde. |
| `autorizaciones` | objeto | Sí | No | Tres booleanos, §3 | Permisos independientes; nunca precargados como otorgados. |
| `consents` | array de objeto | Sí | No | 0–100 eventos propuesto; §3 | Historial explícito; no inferir aceptación por enviar. No descartar eventos para cumplir un límite. |
| `fecha` | string | Sí | No | ISO 8601 UTC, por ejemplo `2026-11-20T15:00:00.000Z` | Hora del cliente; no acredita hora de recepción. Conservar igual en reintentos. |
| `versionEsquema` | string | Sí | No | `conecta-reporte-1` | Extensión propuesta para negociación de esquema. |
| `triajeSugerido` | objeto | Sí | No | §4 | Extensión propuesta, trazable y no vinculante. |
| `contacto` | objeto | No; solo si hay datos y autorización vigente | No | `alias`: string 1–100 o null; `telefono`: string 1–24 o null | Nombres reales de V5; al menos uno no nulo. Solo propio contacto autorizado; omitir íntegramente al revocar. |
| `salud` | string | No; solo si hay selección y autorización vigente | No | Unión con `, ` de selecciones únicas del catálogo S | Forma real de V5, sin texto clínico libre; omitir al revocar. |

Catálogo N: `Agua potable`, `Alimentos`, `Refugio o alojamiento`, `Apoyo en salud`, `Otra`, `No sé todavía`. Seleccionar «No sé todavía» junto con necesidades concretas exige revisión, no borrar las respuestas originales.

Catálogo A: `Artículos para bebés`, `Apoyo para niñas o niños`, `Apoyo para personas mayores`, `Ayuda para el traslado`.

Catálogo S: `Embarazo o lactancia`, `Medicamentos de uso continuo`, `Movilidad reducida`, `Otra necesidad de salud (sin detalles)`. La necesidad operativa «Apoyo en salud» no equivale a autorizar datos personales de salud.

## 3. Objetos de V5 y coherencia

| Nombre | Tipo | Obligatorio | Null permitido | Rango/enumeración | Semántica |
| --- | --- | --- | --- | --- | --- |
| `ubicacion.id` | string | Sí | Sí | ID exacto del catálogo V5 de 514 lugares; máximo 64 propuesto | Null cuando no hay selección válida. No inventar IDs. |
| `ubicacion.nombre` | string | Si id no nulo | No | Valor exacto del catálogo | Nombre asociado al ID. |
| `ubicacion.zona`, `ubicacion.municipio` | string | Con ID o barrio desconocido | Sí, solo sin ID | Valores del catálogo; null si desconocidos | No inferir zona por torre o referencia libre. |
| `ubicacion.texto` | string | Sin ID | No | 6–160 propuesto | Descripción manual; si barrio desconocido, igual a `referencia` recortada en extremos. |
| `ubicacion.barrioDesconocido` | boolean | Si se eligió barrio desconocido | No | `true` en esa variante | Ausente en variantes originales con selección o texto manual. |
| `prioridad.top` | string | Sí | Sí | Urgente: etiqueta HAZARDS; normal: necesidad seleccionada, `Ambas por igual`, `No sé` | `state.hazard` o `state.topNeed`. |
| `prioridad.motivo` | string | Sí | Sí | Urgente: `Peligro inmediato declarado`; normal: opciones de `why` de REGLAS_TRIAJE.md | Respuesta original normal, no reinterpretar como diagnóstico. |
| `prioridad.espera` | string | Sí | Sí | Urgente: `Revisión inmediata sugerida`; normal: `No podemos esperar`, `Dentro de las próximas 24 horas`, `Más de 24 horas`, `No sé` | `state.waiting` en normal; no promesa de plazo. Null solo como dato incompleto, sujeto a revisión. |
| `prioridad.revisionSugerida`, `prioridad.razon` | string | Sí | No | Textos derivados de V5, máximo 240 cada uno propuesto | Compatibilidad; no son el nivel del nuevo motor ni deben prevalecer sobre él. |
| `prioridad.ubicacionPorVerificar` | boolean | Sí | No | true sin selección válida o barrio desconocido | Marca derivada, comprobar coherencia con ubicación. |
| `autorizaciones.solicitud`, `.contacto`, `.salud` | boolean | Sí | No | true/false | Normal requiere solicitud otorgada; urgente permite false sin imponer consentimiento previo. Contacto/salud siempre independientes. |
| `consents[].tipo` | string | Por evento | No | `solicitud`, `contacto`, `salud` | Finalidad del evento. |
| `consents[].otorgada` | boolean | Por evento | No | true/false | Otorgamiento o revocación. |
| `consents[].fecha` | string | Por evento | No | ISO 8601 UTC | Fecha declarada por cliente, no sello probatorio del servidor. |
| `consents[].versionTexto` | string | Por evento | No | `v4-2026-11-20` en V5 | Versión legal literal existente. |
| `consents[].textoMostrado` | string | Por evento | No | Texto literal de `CONSENT_TEXT[tipo]` para esa versión | No resumir ni modificar los textos legales. |

La última decisión explícita por finalidad debe coincidir con `autorizaciones`; una revocación invalida el permiso anterior. Orden e integridad del historial deben verificarse en servidor mediante el protocolo pendiente: el historial del cliente no autentica a nadie. Una inconsistencia impide aceptar datos opcionales sensibles; comunicar el error sin registrarlos en trazas. No bloquear el aviso urgente por omisión de datos opcionales.

El ID de catálogo es la fuente de verdad para nombre/zona/municipio. Si se duplican `ubicacion.texto` y `referencia` en la variante desconocida, validar igualdad. No generar dos versiones de una misma dirección.

## 4. Triaje y respuestas originales

V5 no serializa directamente `danger`, y en ruta urgente descarta algunas respuestas complementarias. No se puede reconstruir «No sé» a partir de `ruta:'normal'`. Se propone `triajeSugerido.respuestasOriginales` como extensión explícita para preservar evidencia mínima, sin campos sensibles ni texto libre adicional. Es una copia de auditoría de las entradas, no una segunda fuente editable; se genera junto con el payload desde una única instantánea. Toda discrepancia con los campos raíz produce error de coherencia y revisión, nunca una prioridad menor automática.

| Nombre dentro de `triajeSugerido` | Tipo | Obligatorio | Null permitido | Rango/enumeración | Semántica |
| --- | --- | --- | --- | --- | --- |
| `nivel` | string | Sí | No | `critico`, `alto`, `medio` | Prioridad sugerida, decisión humana PMU. |
| `requiereVerificacion` | boolean | Sí | No | true/false | Marca transversal, nunca cuarto nivel. |
| `reglasActivadas` | array de string | Sí | No | IDs de REGLAS_TRIAJE.md, sin repetidos | Orden estable de la matriz. |
| `motivos` | array de string | Sí | No | Un motivo español por regla, en el mismo orden | Explicación sin datos sensibles. |
| `versionReglas` | string | Sí | No | `triaje-2026-11-20.1` | Versión de matriz propuesta. |
| `datosFaltantes` | array de string | Sí | No | Rutas de campos faltantes, sin repetidos | Ausencia relevante, no opciones deliberadas «No sé». |
| `respuestasOriginales` | objeto | Sí | No | Campos de la tabla siguiente | Evidencia mínima del estado V5 antes de normalizar. |

| Nombre en `respuestasOriginales` | Tipo | Obligatorio | Null permitido | Rango/enumeración | Semántica |
| --- | --- | --- | --- | --- | --- |
| `danger` | string | Sí | Sí | `Sí`, `No`, `No sé` | Declaración original; null si falta. |
| `hazard` | string | Sí | Sí | Etiqueta HAZARDS de REGLAS_TRIAJE.md | No sustituir etiqueta por una inferencia geográfica. |
| `needs` | array de string | Sí | No | Catálogo N | Copia de entrada, coherente con necesidades en normal. |
| `topNeed` | string | Sí | Sí | Catálogo N, `Ambas por igual`, `No sé` | Necesidad priorizada, sin rellenar ausencias. |
| `why` | string | Sí | Sí | Opciones V5 documentadas en reglas | Motivo explícito original. |
| `waiting` | string | Sí | Sí | Cuatro opciones normales de espera | Puede faltar en aviso urgente. |
| `unknownNeighborhood` | boolean | Sí | No | true/false | Selección original de ubicación desconocida. |
| `persons`, `rapidPersons` | string | Sí | No | Vacío o dígitos decimales dentro del límite de personas | Cadenas originales de los controles, sin convertir vacío a cero. |
| `unknownPersons` | boolean | Sí | No | true/false | En normal determina cantidad desconocida. |
| `who` | string | Sí | Sí | Enumeración de quienIncluye | Señal cualitativa de hogares; no conteo. |

### Ejemplo completo ficticio de aviso urgente

```json
{
  "versionEsquema": "conecta-reporte-1",
  "idReporte": "FICTICIO-R001",
  "torre": "E01",
  "ruta": "urgente",
  "categoria": "atrapadas",
  "ubicacion": {"id": null, "texto": "Cancha ficticia, acceso oriental", "zona": null, "municipio": null, "barrioDesconocido": true},
  "referencia": "Cancha ficticia, acceso oriental",
  "personas": 1,
  "quienIncluye": null,
  "observado": null,
  "necesidades": [],
  "prioridad": {"top": "Personas atrapadas", "motivo": "Peligro inmediato declarado", "espera": "Revisión inmediata sugerida", "revisionSugerida": "Revisión inmediata sugerida", "razon": "Peligro inmediato declarado por quien reporta", "ubicacionPorVerificar": true},
  "apoyos": [],
  "otraNecesidad": null,
  "autorizaciones": {"solicitud": false, "contacto": false, "salud": false},
  "consents": [],
  "fecha": "2026-11-20T15:00:00.000Z",
  "triajeSugerido": {
    "nivel": "critico",
    "requiereVerificacion": true,
    "reglasActivadas": ["C1", "C2", "V1"],
    "motivos": ["Se declaró peligro inmediato.", "Se reportaron personas atrapadas.", "La ubicación necesita verificación."],
    "versionReglas": "triaje-2026-11-20.1",
    "datosFaltantes": [],
    "respuestasOriginales": {"danger": "Sí", "hazard": "Personas atrapadas", "needs": [], "topNeed": null, "why": null, "waiting": null, "unknownNeighborhood": true, "persons": "", "rapidPersons": "1", "unknownPersons": false, "who": null}
  }
}
```

No hay `hashClave` ni clave privada en el cuerpo. El SHA-256 del código más un PIN de 6 dígitos de V5 es recuperable por fuerza bruta sobre un millón de posibilidades; no protege una credencial. Además, `crypto.subtle` puede estar ausente en el contexto HTTP local no seguro de la estación: V5 devuelve null en ese caso. Su presencia tampoco vuelve seguro el PIN. No usar hashes caseros, comparación local ni PIN en logs. El mecanismo seguro de verificación requiere acuerdo con backend antes de habilitar consulta sensible o modificaciones. Esta limitación no se resuelve agregando secretos al POST.

## 5. Respuestas y mensajes de UI

El caso actual es evidencia de código; todos los demás son **PROPUESTA PENDIENTE DE ACUERDO**. Cada JSON es ilustrativo y no acredita que haya ocurrido una recepción real.

| Caso | JSON ficticio o respuesta literal actual | Mensaje de UI propuesto |
| --- | --- | --- |
| **Actual**, HTTP 200 | `{"status":"en_espera","mensaje":"Endpoint listo"}` | «Envío sin confirmar. La estación todavía no confirma que guardó tu reporte.» |
| HTTP 201, primera persistencia confirmada | `{"estado":"guardado","idReporte":"FICTICIO-R001","recibidoEnEstacion":true}` | «Reporte recibido y guardado en la estación. Falta confirmación del PMU.» |
| HTTP 200, reintento idempotente de lo ya guardado | `{"estado":"guardado","idReporte":"FICTICIO-R001","recibidoEnEstacion":true}` | «Tu reporte ya estaba guardado en la estación. Conservamos el mismo código.» |
| HTTP 202, solo con cola persistente formalmente acordada | `{"estado":"en_cola_persistente","idReporte":"FICTICIO-R001","recibidoEnEstacion":false}` | «La estación lo dejó en una cola persistente. Aún no confirma el guardado del reporte ni la recepción del PMU.» |
| HTTP 400, JSON mal formado | `{"error":"json_invalido","mensaje":"No se pudo leer el reporte"}` | «No se pudo leer el envío. Conserva tus respuestas y vuelve a intentarlo.» |
| HTTP 422, campo inválido | `{"error":"validacion","idReporte":"FICTICIO-R001","campos":[{"nombre":"personas","motivo":"Debe ser un entero positivo o null"}]}` | «Revisa la cantidad de personas. Si no la sabes, indícalo.» |
| HTTP 409, mismo ID con contenido diferente | `{"error":"conflicto_idempotencia","idReporte":"FICTICIO-R001"}` | «Ese código tiene otro contenido registrado. Conserva tus respuestas; hace falta resolver el conflicto.» |
| HTTP 429, límite de tasa | `{"error":"limite_temporal","idReporte":"FICTICIO-R001","reintentarEnSegundos":30}` | «La estación tiene muchos envíos. Reintenta en 30 segundos con el mismo código.» |
| HTTP 503, estación no disponible | `{"error":"estacion_no_disponible","idReporte":"FICTICIO-R001","reintentable":true}` | «La estación no pudo confirmar el guardado. Conserva tus respuestas y reintenta con el mismo código.» |

`201/200` solo permiten mostrar guardado cuando el contrato se implemente y el cuerpo válido incluya `estado:'guardado'`, `idReporte` igual al enviado y `recibidoEnEstacion:true`, respaldados por persistencia real del servidor. El código HTTP por sí solo no basta. Una cola en RAM nunca justifica 202; mientras no se acuerde la cola persistente, un 202 se trata como sin confirmar.

La estación debe persistir atómicamente reporte e identidad de operación antes del acuse. Igual ID y mismo contenido canónico devuelve el resultado original sin duplicar; igual ID y contenido distinto produce 409. Canonización, recuperación tras reinicio, retención de IDs y ámbito entre estaciones quedan por acordar. No deduplicar por semejanza, dirección IP o contenido: dos familias pueden producir dos reportes legítimos del mismo incidente.

### Estados separados

1. **Sin confirmar:** respuesta actual, timeout, corte de Wi-Fi, JSON inválido, respuesta inesperada o error sin prueba de guardado. Puede haber ocurrido persistencia sin que llegue el acuse; no afirmar tampoco que se perdió. Mensaje: «Envío sin confirmar».
2. **Recibido y guardado en la estación:** exclusivamente acuse de persistencia validado. No equivale a entrega por radio ni a atención. Mensaje: «Recibido y guardado en la estación; falta confirmación del PMU».
3. **Confirmado por el PMU:** solo con acuse auténtico vinculado al mismo ID, protocolo y procedencia acordados. Mensaje: «El PMU confirmó la recepción; esto no confirma atención ni un plazo». Ejemplo de estado futuro, **PROPUESTA PENDIENTE DE ACUERDO**: `{"estado":"confirmado_pmu","idReporte":"FICTICIO-R001","recibidoEnEstacion":true,"confirmadoPorPMU":true}`. No existe este acuse hoy.

El frontend debe bloquear doble clic durante un intento, congelar ID, fecha y cuerpo, aplicar timeout configurable (15 s como propuesta) y ofrecer reintento explícito del mismo envío. Cancelar la espera no revierte una operación del servidor. El estado solo se conserva en memoria en este diseño: no prometer supervivencia al cierre de la pestaña. Correcciones antes del primer envío recalculan el triaje; después de un resultado incierto no cambiar silenciosamente el cuerpo del mismo ID. Primero resolver su estado mediante el protocolo pendiente; modificaciones posteriores usan PATCH acordado. No inventar un nuevo ID para sortear un 409. Los errores no deben borrar respuestas ni suprimir silenciosamente una alerta vital.

`BroadcastChannel`, `Promise.resolve`, temporizadores y resolución de `fetch` nunca prueban persistencia ni recepción PMU. No mostrar demo como envío real. El uso real permanece bloqueado mientras sigan pendientes el contrato operativo y los campos legales de V5; los textos legales no se modifican en esta entrega.

## 6. Resumen LoRa: estructura propuesta

**PROPUESTA PENDIENTE DE ACUERDO** basada en `buildLoraPayload()`. No es trama binaria aprobada ni transmisión desde el navegador. Backend decide codificación, presupuesto de bytes, fragmentación, retransmisión y acuses.

| Nombre | Tipo | Obligatorio | Null permitido | Rango/enumeración | Semántica |
| --- | --- | --- | --- | --- | --- |
| `id` | string | Sí | No | `idReporte` validado | Identidad de reporte, nunca credencial. |
| `t` | string | Sí | No | `torre` validada | Estación declarada. |
| `c` | string | Sí | No | Enumeración de categoria | Categoría codificada. |
| `u` | string | Sí | No | ID de catálogo, código de zona acordado o `POR_VERIFICAR` | V5 usa id, zona o marcador; solo admitir zona de catálogo codificada, nunca texto manual. |
| `n` | array de string | Sí | No | `AG`, `AL`, `RF`, `SA`, `OT`, `NS`; sin repetidos | Códigos respectivos de N. `SA` es solicitud de apoyo, no condición de salud. |
| `p` | entero | Sí | Sí | 1–10000 propuesto | Personas declaradas, no únicas. |
| `r` | entero | Sí | No | 0 normal, 1 urgente | Ruta de captura, no acuse ni nivel PMU. |

```json
{"id":"FICTICIO-R001","t":"E01","c":"atrapadas","u":"POR_VERIFICAR","n":[],"p":1,"r":1}
```

Lista cerrada: no contacto, datos de salud, alias, teléfono, referencia, otraNecesidad, motivos libres, historial de consentimiento, clave ni hash. No añadir campos por serializar el reporte completo. La función V5 no transporta el nuevo nivel de triaje; cualquier ampliación debe negociarse y versionarse.

## 7. Límites de esta entrega

Solo documentación de diseño. No se alteraron firmware, frontend ni textos legales. No se implementaron ni verificaron por ejecución las rutas propuestas, almacenamiento, idempotencia, autorizaciones, transmisión LoRa o acuses PMU. Los checks declarados los ejecutará el wrapper; no se declara resultado ni pruebas de hardware. Revisar este contrato y las reglas con backend, responsables de datos y PMU antes de implementarlo.
