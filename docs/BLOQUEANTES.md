# Bloqueantes y dependencias externas

Lo que impide el uso real o depende de otras personas. Nada de esto se resolvió con supuestos en el código.

| # | Bloqueante | Depende de | Efecto hoy |
| --- | --- | --- | --- |
| B1 | **Uso real bloqueado.** Los textos legales de V5 siguen con `[Entidad responsable]`, `[NIT]`, `[plazo …]` y avisos de «demostración». No se modificaron. | Responsable legal / usuario | `data/` funciona en modo demo por defecto; `?modo=estacion` es solo para pruebas técnicas. |
| B2 | `POST /api/reporte` **existe pero no guarda**: responde siempre `200 {"status":"en_espera"}` sin leer el cuerpo (`lib/Servidor/Servidor.cpp`). La semántica de respuesta (`estado:"guardado"`, idempotencia, 409/429/503) es **PROPUESTA PENDIENTE DE ACUERDO** (`docs/CONTRATO_JSON.md`). | Backend | La interfaz muestra «Envío sin confirmar»; nunca «recibido». |
| B3 | Contradicción si el backend confirma: con un acuse `guardado` válido, el comprobante muestra «Recibido y guardado en la estación» junto al texto congelado de V5 «No se ha confirmado recepción por un servidor real.». | Responsable legal + backend | Hallazgo H6: corregirlo exige autorizar el cambio de ese texto. |
| B4 | Rutas de estado, verificación de clave, autorizaciones, solicitudes del titular y `PATCH` no existen. La verificación de código y clave sigue siendo local (V5). | Backend | Esas pantallas funcionan solo en la sesión del navegador. |
| B5 | Verificación segura de la clave: `crypto.subtle` no está disponible en `http://192.168.4.1` (contexto no seguro) y un PIN de 6 dígitos con SHA-256 es reversible. No se envía `hashClave`. | Backend | Falta acordar un mecanismo en servidor. |
| B6 | Persistencia en LittleFS, `FuenteIncidentes` persistente, recuperación tras reinicio y transmisión LoRa. `lib/Triaje` ofrece la interfaz y un doble en memoria solo para pruebas. | Backend / LoRa | La agrupación entre teléfonos no funciona todavía en la placa. |
| B7 | Renombrar `Data/` a `data/` (commit propio en esta rama) para que PlatformIO encuentre la carpeta en Linux/macOS. | Backend (aprobación) | En Windows ya funcionaba; revertir el commit si no se aprueba. |
| B8 | Tamaño < 100 000 bytes no cumplido (103 597). Alternativa: servir `.gz`. | Acuerdo de equipo + prueba en placa | Ninguno funcional; LittleFS tiene 3,5 MB. |
| B9 | Contraste insuficiente en pantallas legales (14 nodos, igual que V5). | Autorización del usuario para tocar el CSS legal | Riesgo de legibilidad al sol. |
| B10 | Tablero PMU: `GET /api/tablero` no existe y el esquema de incidentes agrupados (`docs/ENTREGA_PMU.md`) es PROPUESTA PENDIENTE DE ACUERDO. Hay una maqueta autónoma en `tools/pmu-maqueta/` con datos ficticios. | Integrante PMU + backend | El tablero real no recibe datos. |
| B12 | **Riesgo de privacidad:** `serveStatic("/", LittleFS, "/")` (`Servidor.cpp:84`) publica toda la raíz de LittleFS. Si `reportes.jsonl` se guarda ahí, cualquiera conectado al Wi-Fi podría descargarlo con un GET. | Backend | Hoy no hay reportes guardados; debe resolverse antes de implementar la persistencia (p. ej. `serveStatic` restringido a los archivos web o reportes en otra ruta no servida). |
| B11 | Pruebas en hardware, teléfonos reales (Safari, VoiceOver, portal cautivo Samsung), radio y PMU: **NO EJECUTADAS**. | Equipo con la placa | Solo hay evidencia de compilación y navegador de escritorio. |
