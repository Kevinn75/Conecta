# Entrega al responsable del tablero PMU

**PROPUESTA PENDIENTE DE ACUERDO**. Contrato de vista agregada basado en `serializarJson` de `lib/Triaje/Triaje.cpp`, los límites de `Triaje.h`, la fase 5 del prompt maestro y `CONTRATO_JSON.md`. El contrato de reportes no define todavía una API aprobada de incidentes. **`GET /api/tablero` está pendiente**, sin ruta, respuesta envolvente, autenticación ni paginación implementadas.

## Esquema serializable actual del módulo

Todas las claves siguientes se emiten. Los límites de cadenas son bytes UTF-8 sin NUL. Los valores recibidos deben validarse antes de construir el agregado; el serializador no valida todo el dominio de entrada.

| Clave | Tipo y límites | Significado |
| --- | --- | --- |
| `incidenteId` | string, hasta 66 bytes | Nuevo incidente: `I-` + ID original. Ámbito entre estaciones por acordar. |
| `zona` | string hasta 96 bytes o null | Null si desconocida; no inferir lugar por estación o texto. |
| `tipo` | string hasta 24 bytes | Categoría: derrumbe, volcanica, terremoto, atrapadas, otro o necesidades. |
| `prioridadSugerida` | string: critico, alto, medio | Máximo individual recibido; sugerencia, no decisión del PMU. |
| `estadoVerificacion` | string: por_verificar, sin_verificar | Ningún valor acredita verificación humana. No existe nivel de prioridad «por verificar». |
| `cantidadReportes` | entero uint32 | Operaciones independientes conservadas; reintentos no suman. |
| `cantidadPersonasDeclaradas` | entero uint64 | Suma de cantidades válidas declaradas, **NO únicas**. Desconocidas o inválidas no suman; cero agregado no prueba ausencia de personas. |
| `necesidades` | objeto de seis contadores uint32 | Número de reportes que eligieron cada necesidad, no personas por necesidad. Claves literales abajo, incluidos ceros. |
| `razones` | array de hasta 8 strings de 160 bytes | Motivos únicos del triaje; no texto libre personal. |
| `reportes` | array de hasta 64 strings de 64 bytes | IDs individuales, **no objetos anidados** ni identidades de personas. |
| `primeraRecepcion`, `ultimaRecepcion` | string de hasta 24 bytes | Fechas UTC del backend; no son la `fecha` declarada por el cliente. Si faltan, el serializador puede emitir cadena vacía: mostrar «No disponible». |
| `revisionAntifraude` | boolean | Revisión de calidad/autenticidad; no acusación ni prueba de bots. |
| `banderas` | entero uint32, máscara de bits | Señales explicadas abajo. No rebajan prioridad ni eliminan alertas. |
| `versionReglas` | string | `triaje-2026-11-20.1` |
| `versionAgrupacion` | string | `agrupacion-2026-11-20.1` |

`necesidades` tiene exactamente `Agua potable`, `Alimentos`, `Refugio o alojamiento`, `Apoyo en salud`, `Otra` y `No sé todavía`. Apoyo operativo no equivale a dato personal de salud. La vista no incluye contacto, condiciones de salud, autorizaciones ni referencias libres.

El serializador no emite `ubicacionId`, `requiereVerificacion`, `inicioSegundos`, `finSegundos`, cantidades internas de arrays ni configuración. La verificación queda expresada en `estadoVerificacion`. Candidato, evidencia de asociación, estado de entrega y configuración se conservan en la operación del backend; no añadirlos a esta salida como si ya fueran parte del contrato.

Máscara: 1 `CAPACIDAD_AGOTADA`, 2 `DATOS_FALTANTES`, 4 `DATOS_INVALIDOS`, 8 `DISCREPANCIAS`, 16 `RAFAGA`, 32 `PATRON_REPETITIVO`, 64 `TEXTO_TRUNCADO`, 128 `ERROR_FUENTE`, 256 `CONFLICTO_IDEMPOTENCIA`. Puede combinar bits. Algunas banderas corresponden a ingresos fallidos y no necesariamente aparecerán en un agregado persistido; consultar resultado de operación por el mecanismo pendiente. `revisionAntifraude` no equivale a «cualquier bit distinto de cero».

## Ejemplo ficticio coherente con la maqueta

```json
{
  "incidenteId": "I-FICTICIO-R001",
  "zona": "Zona de demostración",
  "tipo": "necesidades",
  "prioridadSugerida": "alto",
  "estadoVerificacion": "por_verificar",
  "cantidadReportes": 2,
  "cantidadPersonasDeclaradas": 7,
  "necesidades": {
    "Agua potable": 2,
    "Alimentos": 1,
    "Refugio o alojamiento": 0,
    "Apoyo en salud": 0,
    "Otra": 0,
    "No sé todavía": 0
  },
  "razones": ["Se indicó que la ayuda no puede esperar."],
  "reportes": ["FICTICIO-R001", "FICTICIO-R002"],
  "primeraRecepcion": "2026-11-20T15:00:00.000Z",
  "ultimaRecepcion": "2026-11-20T15:02:00.000Z",
  "revisionAntifraude": true,
  "banderas": 8,
  "versionReglas": "triaje-2026-11-20.1",
  "versionAgrupacion": "agrupacion-2026-11-20.1"
}
```

Se simula una asociación previamente confirmada con evidencia y una discrepancia pendiente; no se afirma un evento real ni que similitud por sí sola autorice agrupar. El segundo reporte conserva su ID. Dos reportes pueden mencionar las mismas personas: 7 declaraciones no prueban 7 personas únicas. Con este límite de 64 reportes y 100000 por reporte, la suma válida cabe exactamente en un Number de JavaScript; una futura ampliación que alcance valores mayores que `Number.MAX_SAFE_INTEGER` necesitará negociación de representación, sin redondeo silencioso.

## Tarjeta autónoma y detalle individual

Abrir `tools/pmu-maqueta/index.html` localmente. HTML, CSS y JS están en un archivo, sin peticiones, fuentes ni recursos externos. Aviso permanente de datos ficticios y de que no reemplaza el tablero del integrante PMU. Prioridad con texto y forma, verificación separada, revisión antifraude explicada, contadores y botón nativo con `aria-expanded`/`aria-controls` para desplegar todas las solicitudes. Diseño adaptable a 390 px y foco visible.

La maqueta mantiene el objeto agregado exactamente en el esquema anterior. Un diccionario local separado, indexado por los IDs de `reportes[]`, contiene únicamente la proyección ficticia `idReporte`, `personas` y `necesidades` de cada reporte para mostrar el detalle. **No es una nueva API ni el cuerpo completo del contrato**. Las fechas mostradas pertenecen al agregado. En producción se necesita una consulta protegida de esa proyección, permisos y manejo de detalles ausentes, aún por acordar; nunca enviar el original completo al navegador del tablero por comodidad.

Mostrar razones y cada necesidad sin deduplicar IDs diferentes por parecido. La revisión no oculta alertas ni degrada prioridad. No ofrecer acciones simuladas de atención, confirmación o verificación como si tuvieran persistencia. El tablero final, sus decisiones humanas y su historial corresponden al integrante PMU.

## Pendientes y verificación

Pendientes de backend/PMU: aprobación del esquema, `GET /api/tablero`, envolvente y paginación, detalle seguro, permisos, fuente LittleFS, estados de entrega, revisión humana, transporte y acuses auténticos. Ver `INTEGRACION_BACKEND.md` para límites del motor y lista de lo no implementado.

Esta entrega implementa solo documentación y maqueta ficticia. El wrapper ejecutará los checks declarados; no se afirman resultados. Navegación real a 390 px, teclado y lector de pantalla quedan por verificar en navegador; no se acredita hardware, radio ni recepción PMU.
