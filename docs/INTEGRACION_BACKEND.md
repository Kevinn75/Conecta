# Integración para la responsable de backend

Entrega documental de fase 5. **PROPUESTA PENDIENTE DE ACUERDO** para HTTP, almacenamiento y radio. Se conserva la clase `ServidorHotspot`; aquí no se modifica firmware. Referencias: `CONTRATO_JSON.md`, `REGLAS_TRIAJE.md`, `lib/Triaje/{Triaje.h,Triaje.cpp,README.md}` y `tests/triaje/integracion_servidor/main.cpp`.

## Código observado

- `lib/Servidor/Servidor.cpp:87–89`: `POST /api/reporte` responde siempre HTTP 200 con `{"status":"en_espera","mensaje":"Endpoint listo"}`. No lee, valida ni guarda el cuerpo; no acredita recepción persistente.
- `Servidor.cpp:37–38`: el comentario dice TTL 0, pero se ejecuta `setTTL(60)`. Es una discrepancia pendiente de revisión por backend, no una corrección realizada.
- `Servidor.cpp:48–100`: no se registran rutas de estado ni `GET /api/tablero`. El manejador desconocido de las líneas 92–99 devuelve 404 o redirige; no ofrece esas APIs.
- `Servidor.cpp:10–14`: monta LittleFS con `begin(true)`. Antes de alojar reportes, backend debe revisar la política ante fallos de montaje y recuperación; no se ha probado pérdida de energía.
- `Servidor.cpp:83–84`: sirve estáticos desde la raíz de LittleFS. La futura persistencia necesita separación y controles para impedir que los reportes se publiquen como archivos estáticos.
- `src/main.cpp` solo inicia el servidor, procesa DNS y registra el número de estaciones conectadas; no integra el motor.

Según `docs/ai/CONTEXTO.md`, en esta rama **`Data/` se renombró a `data/` y necesita aprobación de la responsable de backend**. Esta entrega no hace el renombrado ni modifica esas carpetas.

## Punto de integración y llamadas

En el procesamiento futuro del cuerpo de `POST /api/reporte` de `lib/Servidor/Servidor.cpp`, después de reunir y parsear el JSON completo y antes de responder, adaptar el reporte y llamar a `MotorTriaje::evaluar(reporte, fuentePersistente)`. Integrar ese paso en su clase existente, sin reescribirla. El callback actual no procesa cuerpos; la recepción por fragmentos, límites y aislamiento por petición siguen pendientes.

1. Conservar el cuerpo original completo. Validar esquema, tipos, UTF-8, ausencia de NUL embebido, catálogo, autorizaciones y coherencia según el contrato. Los límites del módulo son bytes UTF-8; los del contrato de entrada incluyen caracteres. No truncar identidades ni inventar valores faltantes. La excepción urgente sobre cantidad inválida se rige por `REGLAS_TRIAJE.md`: conservar evidencia y alerta sin rebajarla.
2. Validar/calcular el nivel con la matriz versionada antes del ingreso. **El motor C++ no calcula la matriz de respuestas**: recibe `Nivel`, conserva el máximo y agrega revisión. No confiar ciegamente en `triajeSugerido` del cliente.
3. Construir `ReporteEntrante`: `idReporte` estable; `idempotencyKey = idReporte` como en el ejemplo; ubicación validada, zona del catálogo, categoría, nivel, verificación, personas (`-1` desconocidas), bits de necesidades AG/AL/RF/SA/OT/NS, razones de la matriz y discrepancias. El backend aporta tiempo de recepción confiable y fecha UTC, token de contenido canónico y patrón operativo. No usar IP, MAC, contacto ni huellas personales. Los punteros prestados deben vivir durante toda la evaluación.
4. Mantener `MotorTriaje` estático y serializar toda la evaluación con su fuente: lecturas más escritura forman una única operación de un escritor. No compartir el parser mutable entre peticiones simultáneas sin aislamiento.
5. Opcionalmente consultar `FuenteIncidentes::buscarCandidato` y `esCandidato` para preparar una revisión. Igual ubicación/categoría e intervalo total menor que 1200 segundos solo hacen un candidato. Solo aportar `incidenteConfirmado` y `evidenciaAsociacion` tras confirmar el mismo evento. Sin evidencia, se conserva incidente propio. Ubicación desconocida nunca se agrupa por zona o texto.
6. Examinar `ResultadoIngreso`: tipo, `aplicado`, banderas, IDs y evidencia. `aplicado=true` no prueba por sí solo persistencia ni entrega al PMU. `nombreNivel` y `nombreMotivo` permiten representar los enums sin reinterpretarlos.
7. Para la futura vista PMU, recuperar el agregado con `buscarIncidente` y usar `serializarJson`. Consultar tamaño con `(nullptr, 0)` y comprobar `completo`; `requeridos` incluye NUL. Si no cabe, el buffer queda vacío: nunca enviar JSON parcial. `requeridos=0` señala contadores inválidos. Ver `ENTREGA_PMU.md`.

El ejemplo `tests/triaje/integracion_servidor/main.cpp` muestra `procesarReporteEjemplo(json, n, contexto)`, ArduinoJson, el `Servidor.h` real y `FuenteMemoria<2,8>`. **No es handler HTTP, validador completo ni persistencia**. No copiar su aceptación del nivel del cliente como validación de producción. `FuenteMemoria` tampoco conserva originales completos.

## FuenteIncidentes sobre LittleFS: diseño por implementar

Mantener índices persistentes por ID/clave de operación y por incidente, y un índice temporal por ubicación/categoría para candidatos y estadísticas. Leer registros acotados mediante offsets o páginas; conservar solo el registro y agregado en uso. `reportes.jsonl` puede contener originales, pero no debe cargarse entero en RAM. Una recuperación puede recorrerlo incrementalmente con buffers acotados; un índice ausente o corrupto no acredita ausencia de una operación.

| Método | Obligación del adaptador persistente |
| --- | --- |
| `buscarOperacion(id, clave, salida)` | Consultar identidad durable, incluso tras reinicio y fuera de caché; devolver resultado original. Distinguir `AUSENTE` de `ERROR`. Si ID y clave apuntan a operaciones distintas, resolver conflicto sin escribir. |
| `buscarIncidente(id, salida)` | Leer un agregado confirmado, sin exponer transacciones parciales. |
| `buscarCandidato(reporte, ventana, salida)` | Consultar índice y comprobar ubicación exacta, categoría e intervalo completo; no decidir asociación por parecido. |
| `estadisticas(reporte, ventana, salida)` | Contar operaciones nuevas en la ventana inclusiva de recepción para la clave operativa y patrón; error de lectura devuelve false. |
| `guardar(original, operacion, agregado)` | Persistir original completo, índices, resultado, versiones, configuración y evidencia junto al agregado, atómicamente, o nada. |

`ReporteEntrante` no contiene el JSON completo: la fuente debe recibir por contexto de transacción el cuerpo conservado por el adaptador y copiarlo durablemente; no retener sus punteros. El estado de entrega se mantiene aparte y vinculado a la misma operación.

Propuesta de atomicidad: escribir una transacción identificada con original, operación y agregado, verificar todas las escrituras y su integridad, y publicar un marcador de confirmación durable. Los índices solo exponen transacciones confirmadas; deben poder reconstruirse incrementalmente tras reinicio. Alternativamente usar generaciones y manifiesto con protocolo de recuperación. Un append aislado a JSONL o renombrar varios archivos por separado **no garantiza** una transacción conjunta. Backend debe acordar y comprobar las garantías reales de LittleFS, el cierre/sincronización, fallos de escritura y cortes eléctricos antes del acuse.

Ante capacidad agotada no expulsar IDs ni perder originales. Conservar el entrante en almacenamiento pendiente si está disponible, o resolver capacidad y reintentar el mismo ID; no afirmar aceptación si no hubo escritura durable. El límite de 64 IDs por agregado requiere una decisión explícita de paginación/versionado futuro, no omitir reportes silenciosamente.

## Acuses, reintentos y errores propuestos

Solo después de confirmar persistencia atómica del original y su operación puede responderse HTTP 201 con `{"estado":"guardado","idReporte":"FICTICIO-R001","recibidoEnEstacion":true}`. El ID debe coincidir con el envío. Un reintento ya persistido usa HTTP 200 y el mismo acuse durable. Nunca traducir `estadoOriginal:"sin_confirmar"` del doble a guardado; el backend debe recuperar el estado real de entrega.

- Mismo ID y contenido canónico: `REINTENTO_MISMO_ENVIO`, sin escribir otra operación ni aumentar contadores. Conservar ID, fecha y cuerpo al reintentar; comprobar identidad antes de aplicar límites a operaciones nuevas. El token canónico de hasta 64 bytes necesita algoritmo y canonización acordados; sin token no se detectan cambios de contenido.
- **409**: `CONFLICTO_IDEMPOTENCIA`, contenido diferente para la identidad existente. No sobrescribir; conservar original y nuevo cuerpo para aclaración bajo controles de acceso. No crear otro ID para eludir el conflicto.
- **429**: límite temporal explícito de backend, con `reintentarEnSegundos` según contrato (30 es ejemplo). No lo genera automáticamente `RAFAGA`. La señal antifraude pide revisión, no prueba bots ni autoriza descartar una alerta vital.
- **503**: indisponibilidad, error de fuente o capacidad sin confirmación durable. Conservar respuestas y reintentar el mismo ID. `ERROR_FUENTE` nunca equivale a `AUSENTE`.
- JSON ilegible: 400; error de validación: 422, respetando la excepción urgente documentada. No confundir validación con falta de almacenamiento.
- Timeout/corte después del POST: resultado incierto, incluso si pudo persistirse. Reintentar la misma operación; no afirmar pérdida ni recepción. Un 202 solo se consideraría con cola persistente acordada, nunca por RAM.

Guardado local no significa transmisión, recepción del PMU, atención ni plazo. Un estado `confirmado_pmu` necesita un acuse auténtico ligado al ID y un protocolo todavía pendiente.

## Entrega a lib/Lora y lib/Protocol

La estructura propuesta de **`construirResumenRadio`** es la lista cerrada de `CONTRATO_JSON.md`, sección 6. Ese nombre describe el constructor que se debe conectar; **no es una función de `lib/Triaje`** y no se implementa aquí. El agregado JSON tampoco es una trama LoRa.

| Campo | Contenido permitido |
| --- | --- |
| `id` | `idReporte` validado |
| `t` | Código de estación validado |
| `c` | Categoría del contrato |
| `u` | ID de catálogo, zona codificada acordada o `POR_VERIFICAR`; nunca referencia manual |
| `n` | Array sin repetidos de AG, AL, RF, SA, OT, NS |
| `p` | Entero 1–100000 o null; personas declaradas, no únicas |
| `r` | 0 normal o 1 urgente; no es nivel de prioridad |

Ejemplo ficticio: `{"id":"FICTICIO-R001","t":"E01","c":"atrapadas","u":"POR_VERIFICAR","n":[],"p":1,"r":1}`. `SA` representa una necesidad operativa, no una condición de salud. Excluir contacto, salud personal, texto libre, alias, referencia, razones, historial legal y credenciales, aunque exista autorización. No serializar el original hacia radio.

Entregar al responsable de `lib/Lora` y `lib/Protocol` este resumen validado y el ID durable para correlación. Quedan por acordar codificación/versionado, tamaño máximo, fragmentación, cola de salida, reenvíos y autenticidad de acuses. Encolar o transmitir no basta para declarar recepción PMU.

## Memoria y límites de evidencia

Datos medidos **comunicados por la tarea**, no reproducidos en esta entrega: `env:estacion`, RAM **15,9 %**, flash **12,4 %**; `IncidenteAgrupado` **5840 bytes**, `MotorTriaje` **6696 bytes**, `FuenteMemoria<64,256>` **586776 bytes**, este último **solo para pruebas en PC**. No extrapolar porcentajes a una integración completa ni a consumo máximo de pila. El README distingue tamaños según ABI. Presupuestar además parser, buffers, servidor, índices y concurrencia; no instanciar el doble grande en SRAM de ESP32.

## Lo que NO está implementado

- Lectura/validación del POST y conexión real del servidor con el motor; cálculo completo de la matriz en backend.
- Fuente LittleFS transaccional, índices persistentes, recuperación, retención y canonización acordada; idempotencia HTTP durable.
- Acuse `guardado` respaldado por almacenamiento y respuestas 409/429/503 con esta semántica.
- Rutas de estado, verificación segura, autorizaciones, solicitudes del titular y PATCH del contrato.
- `GET /api/tablero`, consulta protegida de detalles, paginación y revisión humana con historial.
- Integración de radio, protocolo de entrega y acuse auténtico del PMU.
- Resolución de campos legales pendientes y aprobación del renombrado `Data/` a `data/`.

Sí existe el motor portable y su serializador; su integración productiva sigue pendiente. Los checks declarados los ejecutará el wrapper. Esta entrega no ejecuta ni afirma pruebas aprobadas, compilación, ensayo de placa, persistencia eléctrica, radio o recepción PMU.
