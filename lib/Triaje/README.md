# Triaje portable de CONECTA

Implementación C++17 sin Arduino, heap, excepciones, RTTI, `std::string` ni
`std::vector` en el motor. No clasifica respuestas: recibe `Nivel` calculado con
`docs/REGLAS_TRIAJE.md` (`triaje-2026-11-20.1`) y conserva el máximo individual.
`requiereVerificacion` es transversal; nunca rebaja `critico`.

La persistencia, recuperación, validación del catálogo, canonización de cuerpos,
acuse HTTP y radio corresponden al backend. Los ejemplos usan datos ficticios y
dobles de almacenamiento. No se implementan rutas ni se modifica el servidor.

## API y tres operaciones diferentes

1. `MotorTriaje::evaluar(reporte, fuente)` consulta primero `idReporte` **o**
   `idempotencyKey`. Una identidad exacta devuelve `REINTENTO_MISMO_ENVIO`, el
   reporte, incidente, estado, banderas, motivo y versiones originales; no llama
   a `guardar` ni aumenta contadores. El tipo es lo único que cambia en un
   reintento idéntico. El estado del doble siempre es `sin_confirmar`.
2. Otro ID es `NUEVO`. `buscarCandidato` exige ID de ubicación conocido y exacto,
   categoría exacta y amplitud temporal estrictamente menor a la ventana.
   **Solo devuelve un candidato**, nunca deduplica ni asocia por semejanza.
   Sin una confirmación adicional crea incidente propio y devuelve `candidatoId`.
   Para asociar, el backend proporciona `incidenteConfirmado` y una
   `evidenciaAsociacion` no vacía después de comprobar que es el mismo evento.
   Se vuelven a comprobar ubicación, categoría y ventana antes de asociar.
   Dos familias así confirmadas producen un incidente y dos reportes conservados.
3. La ráfaga cuenta operaciones nuevas con la misma clave operativa
   `(ubicacionId, categoria)` en los últimos M segundos, inclusive. Con más de N
   reportes marca `RAFAGA`; por defecto N=10 y M=30. Un token `patron` coincidente
   en esa clave/ventana marca `PATRON_REPETITIVO` desde el quinto reporte.
   Los tokens los aporta el backend a partir de atributos operativos, sin IP,
   MAC, identidad de teléfono ni datos personales. No prueban automatización ni
   autenticidad. Cincuenta IDs independientes siguen siendo cincuenta reportes.

El patrón es opcional; sin él solo se evalúa la tasa y las otras señales. Ráfaga,
patrón, faltantes, inválidos y discrepancias activan `revisionAntifraude`. Esta
bandera solicita revisión de calidad/autenticidad, no acusa a una persona ni
suprime alertas. Las discrepancias de respuestas las suministra el adaptador;
el motor detecta además mezcla de necesidades desconocidas/concretas,
asociaciones incompatibles y zona discordante dentro de una asociación.

Para decidir una asociación antes del primer ingreso, el backend puede consultar
`fuente.buscarCandidato(reporte, ventana, salida)` y aportar después la evidencia
a `evaluar`. El doble devuelve el primer candidato compatible en orden de alta;
no afirma que sea el único ni el mejor. El backend persistente puede seleccionar
otro candidato con evidencia y pasarlo por ID. Si ya se ingresó un reporte en
incidente propio, reenviar su ID no lo reasocia: devuelve la operación original.
Una reasociación posterior requiere una transacción de revisión del backend,
fuera de esta API de ingreso, manteniendo historial.

Ubicación nula/vacía o demasiado larga queda por verificar y en incidente propio,
incluso si se proporciona una confirmación. No se infiere ubicación de zona,
referencia, estación ni teléfono. Las zonas desconocidas no son una clave de
agrupación. El backend valida los IDs de catálogo antes de llamar al motor.

La ventana predeterminada es 1200 segundos: 1199 puede ser candidato; 1200 ya no.
Se exige que todo el intervalo `[primeraRecepcion, ultimaRecepcion]`, incluido el
nuevo reporte, quepa en la ventana. No se prolonga indefinidamente comparando solo
con el último aviso. Se admiten tiempos fuera de orden bajo el mismo criterio.
La tasa usa tiempo de recepción confiable, en segundos, suministrado por backend.
Una fecha de cliente nunca acredita recepción. `Configuracion` permite ajustar
ventana y umbrales; `configuracionAplicada` queda en cada resultado persistible,
junto con `versionAgrupacion`. No es una modificación de la matriz de prioridad.

## Identidad y transacciones

`FuenteIncidentes` abstrae búsquedas por ID/clave, incidente, candidato y
estadísticas. Puede consultar índices en disco sin leer todo `reportes.jsonl` en
RAM. Distingue `AUSENTE` de `ERROR`: un error no autoriza una segunda operación.
Si ID y clave apuntan a operaciones distintas, el doble devuelve error y no muta.
El motor es de un solo escritor y no reentrante. Serializar evaluaciones con la
misma fuente, incluyendo lectura y escritura; el backend debe garantizar la
transacción ante concurrencia, caída eléctrica y reinicio.

`guardar(original, indice, agregado)` debe persistir **atómicamente** el original
completo, el resultado, ambos identificadores, la evidencia de asociación, la
configuración y el agregado, o no cambiar nada. `FuenteMemoria` conserva solo
índices, IDs y resultados; no conserva cuerpos JSON ni prueba persistencia. El
caller sigue siendo dueño del original durante toda la operación. Reemplazar
el doble en producción, no considerarlo una cola de reportes.

El ID de incidente nuevo es `I-` seguido de `idReporte`. Los IDs deben ser únicos
en el ámbito acordado entre estaciones. Si ese incidente ya existe y no hay
índice de operación correspondiente, el motor devuelve error sin sobrescribirlo.
No hay expiración de IDs en el doble. El backend define retención y consultas
persistentes; una caché vacía no acredita ausencia global de un reintento.

`contenidoCanonico` es un token de comparación de hasta 64 bytes que proporciona
el backend para el cuerpo original (canonización/digest seguro por acordar según
el contrato). No se calcula un hash casero ni se compara contenido para borrar
reportes con IDs distintos. Un reintento con token diferente devuelve el
resultado original marcado `CONFLICTO_IDEMPOTENCIA`, `aplicado=false`, sin escribir;
el backend resuelve el 409 del contrato y conserva ambos cuerpos para aclaración.
Sin token no se detectan cambios de contenido: usar solo el ID no prueba que el
cuerpo sea idéntico. Con clave estable y otro ID, se devuelve el ID original; no
se incorpora el alias a los índices. El backend debe exigir identidades estables.

`ResultadoIngreso.aplicado=true` solo confirma que la fuente aplicó la operación
o que se encontró la original. No significa entrega al PMU. `estadoOriginal` es
opaco y puede ser restituido por el adaptador persistente; este motor nunca
inventa `guardado` ni `confirmado_pmu`.

## Capacidades, truncamiento y memoria

Todas las longitudes son **bytes UTF-8, sin el NUL**, a diferencia de los máximos
en caracteres del contrato de entrada. El backend conserva el original completo.

| Capacidad pública | Valor | Comportamiento |
| --- | ---: | --- |
| `MAX_INCIDENTES_ACTIVOS` | 64 | Tope del doble predeterminado; sin expulsión |
| `MAX_IDS_RECIENTES` | 256 | Tope de operaciones del doble; sin expulsión |
| `MAX_REPORTES_INCIDENTE` | 64 | No omitir IDs si se llena la lista |
| `MAX_RAZONES` | 8 | Razones distintas acotadas; no omitir exceso |
| `MAX_ID` | 64 | ID, clave, ubicación y tokens |
| `MAX_INCIDENTE_ID` | 66 | Prefijo `I-` + ID |
| `MAX_ZONA` / `MAX_CATEGORIA` | 96 / 24 | Texto del agregado |
| `MAX_RAZON` | 160 | Motivo y evidencia de asociación |
| `MAX_FECHA` / `MAX_ESTADO` | 24 / 31 | Fecha ISO UTC y estado opaco |
| `NUM_NECESIDADES` | 6 | Conteo de reportes por necesidad; bits AG,AL,RF,SA,OT,NS |
| `TRIAJE_MAX_PERSONAS` | **100000** | Válidas exactamente 1–100000 |

Los valores de personas inválidos (incluidos cero y negativos distintos de -1)
se aceptan con `DATOS_INVALIDOS`; -1 se acepta con `DATOS_FALTANTES`. No aportan
personas al total. Una cantidad válida suma declaraciones: **no son personas
únicas ni una estimación de familias o teléfonos independientes**.

Texto descriptivo largo se trunca con `TEXTO_TRUNCADO`, sin partir UTF-8 válido,
y exige verificación. El adaptador valida UTF-8, cadenas NUL-terminadas y fechas
ISO antes de invocar. Identidades largas/ausentes/inválidas no se truncan para
deduplicar: devuelven `aplicado=false` con bandera; el backend conserva el aviso
pendiente de resolver identidad. ID de reporte: ASCII alfanumérico, `_`, `-`.
Categoría inválida no habilita asociación. El adaptador valida su enumeración.

Si se llena una tabla, lista, contador o conjunto de razones, se devuelve
`CAPACIDAD_AGOTADA` y `aplicado=false`. No se modifica ningún índice/agregado,
no se expulsa un reporte anterior y no se afirma que se aceptó en almacenamiento.
**El backend debe conservar el original entrante en almacenamiento pendiente o
resolver la capacidad y reintentarlo con el mismo ID.** Nunca traducir este
resultado a éxito ni descartar el cuerpo. El motor acotado no puede prometer
guardar un flujo ilimitado. Una transacción persistente puede ofrecer mayor
capacidad de índices; para listas por incidente se necesita paginación backend o
una decisión explícita, sin eliminar reportes del historial.

Estimación estática (depende de ABI, alineación y tamaño de `size_t`):

| Objeto | Bytes aproximados |
| --- | ---: |
| `IncidenteAgrupado` | 5832–5840 |
| `ResultadoIngreso` | 472 |
| `RegistroOperacion` | 832 |
| `MotorTriaje` (incluye área de trabajo) | 6688–6696 |
| `FuenteMemoria<64,256>` | 586256–586776 |
| `FuenteMemoria<2,8>` | 18336–18360 |

Son estimaciones de disposición, **no mediciones de placa**. La prueba host
imprime `sizeof` reales cuando el wrapper la ejecuta. Fórmula del doble:
`I*sizeof(IncidenteAgrupado) + R*sizeof(RegistroOperacion) + vptr + contadores + padding`.
El doble predeterminado es para PC y **no cabe razonablemente en SRAM interna de
ESP32-S3**; no instanciarlo así en firmware. Los sketches usan capacidades
reducidas y objetos estáticos. El backend real implementa `FuenteIncidentes`
contra índices persistidos. Presupuestar también pila, parser JSON, buffers y
servidor: no se ha medido uso de pila ni RAM del firmware completo. No hay heap
en el módulo; ArduinoJson y el servidor tienen sus propias políticas de memoria.

## Serialización JSON propuesta

`serializarJson(incidente, buffer, capacidad)` usa salida acotada y `snprintf`
para números y escapes. Devuelve `completo`, `escritos` (sin NUL) y `requeridos`
(con NUL). Puede consultarse el tamaño con `(nullptr, 0)`. Si no cabe, vacía el
buffer (cuando tiene capacidad), devuelve false y permite reintentar; jamás
entregar el prefijo como JSON válido. Contadores de listas corruptos producen
false y `requeridos=0`. Se escapan comillas, barras inversas y controles U+0000–001F;
las cadenas C no representan NUL embebido. El adaptador rechaza NUL embebido al
validar JSON, y conserva el cuerpo original para revisión.

`docs/CONTRATO_JSON.md` define reportes, niveles y cantidades, **pero aún no un
esquema de `IncidenteAgrupado`**. Esta serialización sigue los campos exigidos por
la fase 5 del prompt maestro y esta tarea; es una propuesta pendiente de acuerdo
con backend/PMU, no una API ya aprobada. Campos:

- `incidenteId`, `zona` (null si desconocida), `tipo`, `prioridadSugerida`.
- `estadoVerificacion`: `por_verificar` si existe incertidumbre o revisión;
  en otro caso `sin_verificar`. Nunca se infiere verificación humana.
- `cantidadReportes`, `cantidadPersonasDeclaradas` (suma declarada).
- `necesidades`: objeto con las seis etiquetas literales del contrato y el número
  de reportes que seleccionaron cada una. No es un conteo de personas por necesidad.
- `razones`: textos únicos del triaje recibido; `reportes`: IDs individuales.
- `primeraRecepcion`, `ultimaRecepcion`: fechas ISO UTC aportadas por backend.
- `revisionAntifraude`, `banderas`, `versionReglas`, `versionAgrupacion`.

Los motivos de asociación, candidatos, configuración y estados individuales se
persisten con `ResultadoIngreso`; no se pierden al crear la vista agregada.
El agregado no incluye contacto, salud, autorización ni referencia libre, y no
es un paquete LoRa. No serializar datos personales como razones o patrones.

## Integración y checks pendientes del wrapper

1. Parsear/validar el cuerpo con ArduinoJson en backend; mantener el JSON original.
   Validar catálogo, tipos, UTF-8, versión, coherencia y nivel calculado.
2. Crear `ReporteEntrante` con punteros válidos durante `evaluar`, contexto de
   recepción y, cuando corresponda, evidencia explícita de asociación.
3. Usar una implementación persistente de `FuenteIncidentes`; evaluar bajo la
   exclusión/transacción del backend. Resolver `aplicado=false` conservando el
   original. La fuente no debe retener punteros prestados.
4. Solo el backend puede enviar un acuse respaldado por persistencia real.

`tests/triaje/esp32_compila.cpp` usa `setup/loop` y la API portable.
`tests/triaje/integracion_servidor/main.cpp` incluye el `Servidor.h` real, crea
`ServidorHotspot servidor` como `src/main.cpp` y ofrece
`procesarReporteEjemplo(json, n, contexto)` tras parsear con ArduinoJson. Es una
demostración con índices RAM; no registra rutas ni valida todo el contrato.
Su parser conserva el límite de personas, las necesidades y los motivos; las
validaciones completas y el reemplazo de la fuente son trabajo del backend.

El wrapper ejecutará los comandos declarados por la tarea. El runner host
`python -I tests/triaje/ejecutar.py` exige g++ existente, compila con
`-std=c++17 -Wall -Wextra -Werror -O1 -fno-exceptions -fno-rtti` en
`.pio/triaje-host/`, ejecuta asserts propios y verifica JSON con `json.loads`.
No instala dependencias. Los fixtures cubren familias independientes, reintento,
clave estable, conflicto, 50 avisos, incertidumbre, límites de personas y memoria,
ventana, prioridad máxima, errores de fuente, recuperación simulada y escape JSON.

**No se ejecutaron estos checks en esta entrega.** Compilación ESP32/servidor,
comprobación de archivos protegidos y resultados host quedan al wrapper. Las
pruebas con dobles no acreditan persistencia, hardware, radio ni recepción PMU.
