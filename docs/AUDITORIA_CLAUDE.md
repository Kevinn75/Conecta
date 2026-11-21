# Auditoría independiente de Claude

Revisión de los cambios reales de T1 (`docs/CONTRATO_JSON.md`, `docs/REGLAS_TRIAJE.md`), T2 (`data/`, `tests/web/`) y T3 (`lib/Triaje/`, `tests/triaje/`), no de los resúmenes de Codex. Evidencia ejecutable: `tests/navegador/auditoria.spec.mjs` (Chromium real, viewport iPhone 13, V5 y `data/` lado a lado) y ejecuciones manuales en Node y g++.

## Ronda 1 — hallazgos (20 de noviembre de 2026)

| ID | Sev. | Hallazgo | Reproducción | Recomendación |
| --- | --- | --- | --- | --- |
| H1 | Alto | Al cambiar entre ruta urgente y normal quedan respuestas residuales que entran al envío y al triaje. Urgente→normal conserva `hazard: "Personas atrapadas"`; normal→urgente conserva `needs`, `waiting: "No podemos esperar"` y `persons`, y activa A1 en un aviso urgente. | Pruebas 2a y 2b de `tests/navegador`. | Al cambiar `danger` de ruta, borrar los campos exclusivos de la otra ruta; no tocar ubicación ni historial legal. |
| H2 | Alto | Un aviso urgente crítico no se puede crear si el campo opcional de personas es inválido (`0`, `-3`, `1.5`, `200000`) o si queda un campo residual incoherente (`who: "Solo a mí"` con varias personas): `construirPayload` lanza error y `issueReceipt` lo bloquea. | Prueba 10; `node -e` con `Triaje.construirPayload({danger:'Sí',hazard:'Personas atrapadas',rapidPersons:'0',…})` → «Indica un entero de 1 a 100000». | En ruta urgente, datos opcionales inválidos → `personas: null`, regla V3 y advertencia; nunca bloquear el aviso. Ruta normal sin cambios. |
| H3 | Medio | Regresión visual: al cargar, el foco programático en el `h2` de inicio dibuja un recuadro negro que V5 no tiene. | Prueba 9 (`inicio` difiere píxel a píxel; el resto de pantallas comparadas son idénticas). | No mover el foco en la carga inicial; los encabezados con `tabindex="-1"` no muestran anillo de foco. |
| H4 | Medio | Con el menú de sugerencias abierto, la parte superior del botón «No sé en qué barrio estoy» queda bajo el menú: un toque ahí elige un lugar. Existe igual en V5. | Prueba 11 (`[true,false,false]` en borde superior, centro, borde inferior). | Que el menú abierto no se superponga al botón; con el menú cerrado el diseño debe seguir idéntico a V5. |
| H5 | Medio | Tamaño 104 635 bytes en `data/` (gzip 34 258); objetivo < 100 000 no cumplido. `estilo.css` (18 327) es casi idéntico al de V5 y conserva reglas sobrescritas. | `node tests/web/medir.mjs`. | Eliminar CSS muerto y espacios sin cambiar el aspecto (prueba 9) ni textos; reportar el número exacto. |
| H6 | Info / bloqueo | En `?modo=estacion`, con acuse `guardado` válido, el comprobante muestra a la vez «Recibido y guardado en la estación» y el texto congelado de V5 «No se ha confirmado recepción por un servidor real.». | Prueba 3b (`contradiccionComprobanteGuardado: true`). | No modificar el texto legal: el modo de uso real sigue bloqueado (ver `BLOQUEANTES.md`). |
| H7 | Medio (preexistente) | Contraste insuficiente en pantallas legales (`color-contrast`, 14 nodos), idéntico en V5 y `data/`. | Prueba 5 (axe WCAG 2.2 AA): sin regresiones; mismas violaciones en ambas. | Requiere autorización del usuario para tocar el CSS legal. |

### Verificado sin hallazgos

- Flujos legales: 21 pantallas (privacidad, revisión con casilla marcada/desmarcada, permiso y datos de contacto, verificación con código erróneo y correcto, historial, revocaciones, solicitudes de corrección y supresión, salud, ruta urgente con art. 10c): texto visible y casillas idénticos a V5.
- `200 {status:"en_espera"}`, acuse con otro `idReporte` y respuesta no JSON → «Envío sin confirmar»; «guardado» solo con `estado:"guardado"`, `recibidoEnEstacion:true` y mismo ID.
- Doble clic → 1 POST; corte de red tras POST → reintento con el mismo ID y cuerpo idéntico; sin `crypto.subtle` el aviso se crea y no hay `hashClave`.
- Offline: ninguna petición fuera del origen local; 514 lugares cargados; búsqueda sin tildes («CHIPRE», «exito fundadores»).
- Sin scroll horizontal a 360 y 390 px; foco al encabezado tras cambiar de pantalla.
- C++: reintento idéntico (1 operación, conflicto si cambia el contenido), reporte de otra familia conservado y asociado solo con confirmación, ráfaga de 50 conservada y marcada, ubicación desconocida nunca agrupada, `TRIAJE_MAX_PERSONAS = 100000`; búferes grandes son miembros del motor (no en pila). Compila para `esp32-s3-devkitc-1` y con el `Servidor.h` real en `env:estacion` (RAM 15,9 %, flash 12,4 %) sin modificar `lib/Servidor`.
