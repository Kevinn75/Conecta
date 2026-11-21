# Reglas de triaje preliminar de CONECTA

**Diseño propuesto pendiente de validación del equipo de emergencias y del PMU.** `versionReglas: "triaje-2026-11-20.1"`. No es un motor implementado ni una decisión clínica. Prioridad sugerida con revisión y decisión humana del PMU; no promete atención ni plazos.

Fuentes: fase 2 y casos del prompt maestro, `initialState()`, `HAZARDS`, pantallas de necesidades/motivo/espera y `buildPayload()` de V5; contexto y manifiesto de `docs/ai/`. El contrato asociado es `docs/CONTRATO_JSON.md`. No se requieren mapas ni datos externos para estas reglas.

## 1. Entradas reales y normalización

Conservar la instantánea original; normalizar en otra representación. No rellenar respuestas de la persona ni confundir null, vacío, cero y «No sé». Las opciones se comparan por valores exactos de V5; normalizar espacios periféricos sin cambiar su significado.

| Campo real de V5 | Dominio y correspondencia |
| --- | --- |
| `danger` | `Sí`, `No`, `No sé`, null. Determina `ruta`; se conserva explícitamente en la extensión propuesta `triajeSugerido.respuestasOriginales`. |
| `hazard` | HAZARDS: `Derrumbe o deslizamiento` → `derrumbe`; `Actividad volcánica` → `volcanica`; `Terremoto o temblor` → `terremoto`; `Personas atrapadas` → `atrapadas`; `Otro peligro / No sé` → `otro`; null si no contestado. |
| `needs` | `Agua potable`, `Alimentos`, `Refugio o alojamiento`, `Apoyo en salud`, `Otra`, `No sé todavía`; array → `necesidades`. |
| `topNeed` | Necesidad seleccionada, `Ambas por igual`, `No sé` o null → `prioridad.top` normal. |
| `why` | Opciones por contexto abajo; null permitido por ser complemento opcional → `prioridad.motivo` normal. |
| `waiting` | `No podemos esperar`, `Dentro de las próximas 24 horas`, `Más de 24 horas`, `No sé`, null → `prioridad.espera` normal. |
| `unknownNeighborhood`, `location`, `reference`, `locationQuery` | Ubicación desconocida, selección del catálogo y referencia manual. Sin selección válida siempre por verificar; una referencia no acredita coordenadas. |
| `persons`, `rapidPersons`, `unknownPersons` | Controles originales en texto; usar `rapidPersons` si danger Sí, en otro caso persons salvo unknownPersons. Entero 1–10000 o null en el contrato propuesto. |
| `who` | `Solo a mí`, `A mi hogar`, `A varias familias`, `A otra persona o grupo`, `No sé`, null → `quienIncluye` normal. No existe cantidad de familias. |

Opciones reales de `why`:

| Contexto de necesidad | Respuestas |
| --- | --- |
| Agua potable | `Ya se acabó`, `Queda poca`, `Todavía tenemos`, `No sé` |
| Alimentos | `Ya se acabaron`, `Quedan pocos`, `Todavía tenemos`, `No sé` |
| Refugio o alojamiento | `No tenemos dónde`, `Tenemos un lugar inseguro`, `Sí tenemos dónde`, `No sé` |
| Apoyo en salud | `No puede esperar`, `Puede esperar algunas horas`, `Puede esperar más`, `No sé` |
| Otra necesidad | `Es urgente`, `Puede esperar`, `No sé` |
| Pantalla adicionales | `Se acabó lo que necesitamos`, `Queda muy poco`, `Aún tenemos algo`, `No sé` |

Para un motivo específico, usar `topNeed` si es una necesidad seleccionada. Si no lo es y hay exactamente una necesidad concreta, usar esa necesidad. Con varias necesidades y `Ambas por igual`, `No sé` o null, la asignación del motivo es ambigua: V5 puede elegir la primera necesidad para mostrar una pregunta, pero no se deduce que todas tengan esa severidad. Aplicar V4 y, si el motivo expresa severidad, A6; conservar evidencia. Los motivos genéricos de adicionales no se atribuyen a una necesidad individual.

Validar tipos y dominios antes de serializar. Cantidades negativas, cero, decimales, no numéricas o superiores al máximo propuesto son errores, no se corrigen a 1 ni se interpretan como ausencia de afectados. Una alerta crítica conserva la sugerencia crítica y marca V3 mientras se pide corregir el dato. No se envía un JSON inválido como si hubiera sido aceptado; errores de campos opcionales no justifican ocultar el peligro.

## 2. Evaluación determinista y configuración

La matriz, los conjuntos de respuestas y el máximo de personas son configuración versionada; no un puntaje acumulativo. Un cambio de condición, precedencia, umbral o motivo exige una nueva `versionReglas`, revisión humana y actualización de casos esperados. No cambiar el significado de un ID existente silenciosamente.

Evaluar todas las reglas aplicables en una misma instantánea. Orden de salida: C1–C3, A1–A7, M1, V1–V5, S1. Elegir el nivel máximo `critico > alto > medio`; M1 solo se activa si no hay C ni A. Las reglas V agregan `requiereVerificacion=true` sin reducir nivel; A5 y A7 también obligan verificación. S1 solo añade contexto. No hay nivel «Por verificar».

Faltantes relevantes: danger; en urgente hazard; en normal needs vacío o waiting ausente. Personas desconocidas activan V2. Ubicación sin selección válida activa V1 aun con referencia suficiente. Campos opcionales why, who, topNeed, observado y apoyos ausentes no elevan solos ni bloquean el flujo urgente. `datosFaltantes` registra únicamente ausencias relevantes por ruta de campo; la respuesta explícita «No sé» conserva evidencia y marca incertidumbre, sin fingir que no fue respondida.

## 3. Matriz configurable

En la columna evidencia, guardar el nombre y valor de los campos usados, sin contacto ni condiciones de salud. Los motivos son textos españoles de explicación, no mensajes que afirmen recepción o atención.

| ID estable | Condición sobre entradas V5 | Nivel/efecto | Evidencia | Motivo |
| --- | --- | --- | --- | --- |
| C1 | `danger === 'Sí'` | critico | danger | Se declaró peligro inmediato. |
| C2 | `hazard === 'Personas atrapadas'`, incluso si danger discrepa | critico | hazard, danger | Se reportaron personas atrapadas. |
| C3 | danger Sí y hazard en derrumbe/deslizamiento, actividad volcánica o terremoto/temblor | critico | danger, hazard | Se declaró un evento grave con peligro inmediato para personas. |
| A1 | `waiting === 'No podemos esperar'` | alto | waiting | Se indicó que la ayuda no puede esperar. |
| A2 | Necesidad contextual de refugio y why `No tenemos dónde` o `Tenemos un lugar inseguro` | alto | needs, topNeed, why | Se reportó falta de refugio seguro. |
| A3 | Necesidad contextual de agua y why `Ya se acabó`; o alimentos y why `Ya se acabaron` | alto | needs, topNeed, why | Se reportó agotamiento de un recurso básico. |
| A4 | Necesidad contextual de salud y why `No puede esperar` o `Puede esperar algunas horas` | alto | needs, topNeed, why | Se solicitó atención de salud que requiere coordinación prioritaria. |
| A5 | `danger === 'No sé'` | alto + verificación | danger | No se pudo descartar peligro inmediato. |
| A6 | why `Es urgente`, `Se acabó lo que necesitamos` o `Queda muy poco`; o motivo específico severo sin contexto inequívoco | alto | needs, topNeed, why | Se declaró una necesidad severa cuya atención debe revisarse. |
| A7 | danger ausente; o en normal needs vacío o waiting null/`No sé`; o hazard grave seleccionado con danger distinto de Sí | alto + verificación | Campos faltantes o hazard/danger | Faltan datos para descartar urgencia o aclarar el evento declarado. |
| M1 | Ninguna C ni A activa | medio | danger, needs, waiting y why disponibles | No hay indicadores conocidos de prioridad crítica o alta en las respuestas actuales. |
| V1 | unknownNeighborhood true o sin location válido de catálogo | Marca transversal | unknownNeighborhood, location, presencia de referencia | La ubicación necesita verificación. |
| V2 | Cantidad de personas desconocida; o why `No sé`; o needs incluye `No sé todavía`; o hazard `Otro peligro / No sé` | Marca transversal | Campo y valor incierto | Hay información declarada como desconocida. |
| V3 | Dato inválido o contradicción: danger No con hazard no nulo; who Solo a mí con personas >1; motivo severo con waiting Más de 24 horas | Marca transversal y error si inválido | Pares de campos contradictorios o campo inválido | Hay datos que deben aclararse sin rebajar la alerta. |
| V4 | Motivo específico sin necesidad contextual inequívoca o motivo incompatible con su necesidad | Marca transversal | needs, topNeed, why | Falta aclarar a qué necesidad corresponde el motivo. |
| V5 | Faltante relevante según ruta, incluyendo hazard ausente en urgente | Marca transversal | Rutas de campos faltantes | Faltan respuestas relevantes para interpretar el reporte. |
| S1 | Personas válidas >1 o who `A varias familias` | Sin cambio de nivel | personas normalizadas, who | Se declaró afectación de un grupo; la cantidad no reemplaza las alertas vitales. |

«Motivo específico severo» significa exactamente `No tenemos dónde`, `Tenemos un lugar inseguro`, `Ya se acabó`, `Ya se acabaron`, `No puede esperar`, `Puede esperar algunas horas`. Si tiene contexto inequívoco aplican A2/A3/A4; si no, A6 y V4. No interpretar texto libre para inferir atrapamientos, colapso o diagnósticos. No sé sobre waiting se cubre por A7; No sé sobre danger se cubre por A5. No sé sobre un detalle opcional puede coexistir con medio y verificación cuando sí se descartó peligro y no hay otra señal alta.

La categoría por sí sola no prueba exposición humana actual. C3 exige danger Sí; cuando falta esa afirmación se aplica A7 y revisión. C2 conserva cautela ante atrapamiento incluso con una contradicción. No se exige un mínimo de personas para C1–C3. «Queda poca» agua o «Quedan pocos» alimentos sin otra señal urgente no equivalen por sí solos a agotamiento; no se inventa una cantidad disponible.

No se usan barrios críticos, capas GIS no entregadas, ubicación precisa inferida de comercios o la estación Wi-Fi. No se infiere vulnerabilidad por barrio ni por cantidad de personas. `apoyos` ayuda a coordinar, pero no revela automáticamente una condición médica ni altera la prioridad por sí solo. Familias es una señal cualitativa; jamás multiplicar personas por familias ni inventar un número de hogares.

## 4. Salida propuesta

```json
{
  "nivel": "alto",
  "requiereVerificacion": true,
  "reglasActivadas": ["A5"],
  "motivos": ["No se pudo descartar peligro inmediato."],
  "versionReglas": "triaje-2026-11-20.1",
  "datosFaltantes": []
}
```

Ejemplo ficticio para danger No sé, lugar seleccionado válido, una persona, agua como necesidad, waiting Más de 24 horas y sin otros indicadores. La extensión `respuestasOriginales` del contrato acompaña esta salida al construir el POST. Conservar entradas originales para revisión y recalcular toda la salida cuando cambia una respuesta antes de enviar; no acumular reglas obsoletas. Una decisión humana posterior debe registrar su motivo e historial en backend, integración pendiente.

## 5. Casos esperados, no ejecutados

Datos ficticios. Base B para abreviar: danger `No`, hazard null, needs `[Agua potable]`, topNeed null, why null, waiting `Más de 24 horas`, location válido del catálogo, unknownNeighborhood false, persons `1`, unknownPersons false, who null. Referencia manual ficticia suficiente donde se indique. Cambios de cada fila sustituyen la base, no se acumulan. En urgente se usa rapidPersons `1` salvo indicación y no se exigen needs/waiting. «Personas» en la tabla significa el valor normalizado desde el control de la ruta. Los conjuntos de reglas listados son completos para estas entradas.

| Caso | Entrada sobre B → contexto | Nivel esperado | requiereVerificacion | Reglas |
| --- | --- | --- | --- | --- |
| 1 | B: necesidad ordinaria y una persona | medio | false | M1 |
| 2 | danger Sí, hazard Personas atrapadas, needs [], waiting null | critico | false | C1, C2 |
| 3 | danger Sí, hazard Derrumbe o deslizamiento, personas 8, who A varias familias, needs [], waiting null | critico | false | C1, C3, S1 |
| 4 | Caso 2 sin location, unknownNeighborhood true y referencia suficiente | critico | true | C1, C2, V1 |
| 5 | waiting No podemos esperar, sin location, referencia suficiente | alto | true | A1, V1 |
| 6 | danger No sé; resto B | alto | true | A5 |
| 7 | danger null, needs [], waiting null, personas null, sin location | alto | true | A7, V1, V2, V5 |
| 8 | needs [Agua potable, Alimentos], topNeed Agua potable, why Ya se acabó, waiting Dentro de las próximas 24 horas | alto | false | A3 |
| 9 | needs [Refugio o alojamiento], why No tenemos dónde, waiting Dentro de las próximas 24 horas | alto | false | A2 |
| 10 | needs [Refugio o alojamiento], why Tenemos un lugar inseguro, waiting No podemos esperar | alto | false | A1, A2 |
| 11 | needs [Apoyo en salud], why No puede esperar, waiting No podemos esperar | alto | false | A1, A4 |
| 12 | needs [Apoyo en salud], why Puede esperar algunas horas, waiting Dentro de las próximas 24 horas | alto | false | A4 |
| 13 | waiting Dentro de las próximas 24 horas; resto B | medio | false | M1 |
| 14 | waiting No sé; resto B | alto | true | A7 |
| 15 | danger No, hazard Personas atrapadas | critico | true | C2, V3 |
| 16 | danger No, hazard Derrumbe o deslizamiento | alto | true | A7, V3 |
| 17 | danger Sí, hazard Otro peligro / No sé, personas null, needs [], waiting null | critico | true | C1, V2 |
| 18 | B con personas 100 y who A varias familias | medio | false | M1, S1 |
| 19 | B con needs [No sé todavía], personas null | medio | true | M1, V2 |
| 20 | B con why No sé | medio | true | M1, V2 |
| 21 | needs [Agua potable, Refugio o alojamiento], topNeed Ambas por igual, why No tenemos dónde, waiting Dentro de las próximas 24 horas | alto | true | A6, V4 |
| 22 | why Ya se acabó, waiting Más de 24 horas | alto | true | A3, V3 |
| 23 | danger Sí, hazard null, needs [], waiting null | critico | true | C1, V5 |
| 24 | B con personas -2 | medio provisional; error de validación | true | M1, V3 |
| 25 | danger Sí, hazard Personas atrapadas, personas -2, needs [], waiting null | critico; error de validación | true | C1, C2, V3 |
| 26 | Antes: caso 6; después corrige danger a No, resto B | alto → medio | true → false | A5 → M1 |
| 27 | Antes: B; después danger Sí, hazard Personas atrapadas, needs [], waiting null | medio → critico | false → false | M1 → C1, C2 |
| 28 | B sin location, texto manual ficticio de más de 6 caracteres | medio | true | M1, V1 |
| 29 | B con who Solo a mí y personas 4 | medio | true | M1, V3, S1 |
| 30 | danger Sí, hazard Actividad volcánica, needs [], waiting null | critico | false | C1, C3 |
| 31 | danger Sí, hazard Terremoto o temblor, needs [], waiting null | critico | false | C1, C3 |

Un valor inválido no autoriza usar el nivel provisional para desatender un aviso: se presenta el error y se conservan señales válidas. El máximo de personas es negociable; un número superior requiere aclaración, no eliminación automática del reporte.

## 6. Casos complementarios del prompt maestro

Estos escenarios pertenecen también a transporte, agrupación o privacidad. Son expectativas de diseño, no capacidades probadas del navegador ni nuevas reglas basadas en campos inventados.

| Escenario ficticio | Resultado esperado y límite |
| --- | --- |
| Dos teléfonos, familias diferentes del mismo deslizamiento, entradas del caso 3 e IDs distintos | Ambos critico por C1/C3, señal S1. Con evidencia compartida suficiente, 1 incidente y 2 reportes conservados; agrupación en backend pendiente, nunca deduplicación por semejanza. |
| Doble clic o dos envíos del mismo ID y contenido | Una operación persistente cuando exista idempotencia del servidor; mismo triaje y mismo acuse. Hoy no está implementado. |
| Cincuenta avisos idénticos en ráfaga con IDs distintos | Señalar anomalía para revisión compartida, sin afirmar bots ni borrar reportes. Si tienen personas atrapadas, mantener critico. Señal de abuso externa al triaje V5; no fingir que un teléfono observa a los otros. |
| Reporte sospechoso de atrapados | Conservar C2 y critico; revisión de autenticidad separada. La sospecha no reduce prioridad ni suprime el reporte. |
| Corte de Wi-Fi justo después del POST | Sin confirmar; reintentar la instantánea con el mismo ID. No recalcular sobre respuestas nuevas ni afirmar pérdida o guardado. |
| HTTP 200 con status en_espera | Sin confirmar; no guardado ni confirmado por PMU, independientemente del nivel. |
| Autorización de contacto o salud revocada | Omitir esos datos en envíos posteriores, conservar el evento legal literal y tramitar revocación por mecanismo pendiente. No afirmar borrado remoto por cambio local. |
| Resumen LoRa con reporte que tenía contacto y salud autorizados | Solo lista cerrada id/t/c/u/n/p/r del contrato; nunca contacto, condiciones de salud ni texto libre. |
| Entorno con o sin crypto.subtle | No incluir clave privada ni hashClave. Su disponibilidad no autentica ni confirma el reporte; protocolo seguro pendiente. |
| Cambio de respuestas antes de enviar | Recalcular reglas sobre la última instantánea, como casos 26/27; preservar origen para auditoría sin mezclar motivos antiguos. |

## 7. Verificación pendiente

Se entrega únicamente diseño documental. La matriz, los 31 casos y escenarios complementarios son resultados esperados, **no pruebas ejecutadas**. El wrapper ejecutará los checks declarados. No se acredita funcionamiento de firmware, hardware, LoRa, persistencia ni recepción del PMU. La validación operativa de prioridades, límites y protocolo corresponde a los responsables del proyecto antes de implementar y habilitar uso real. No se modifican textos legales ni se dan por completados los campos pendientes de V5.
