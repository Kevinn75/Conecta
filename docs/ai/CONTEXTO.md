# Contexto mínimo de CONECTA

Inspección del 20 de noviembre de 2026 sobre `main` en `047e96a` («estacion y pmu», responsable de backend; antes `307e52d`) de https://github.com/Kevinn75/Conecta.
Releer los archivos si el repositorio evoluciona; este inventario es una instantánea.

## Comprobado en el repositorio

| Archivo | Estado en la base inspeccionada | Responsable |
| --- | --- | --- |
| `platformio.ini` | ESP32-S3, `board_build.filesystem = littlefs`, particiones `default_16MB.csv`, PSRAM, entornos `estacion` y `concentrador` | Backend |
| `lib/Servidor/Servidor.{h,cpp}` | Clase `ServidorHotspot`: SoftAP abierto `punto_de_ayuda_conecta` en `192.168.4.1`, DNS cautivo, sondas de portal (302), `serveStatic("/", LittleFS, "/")` | Backend |
| `POST /api/reporte` | Lee un JSON plano y numérico (máx. 2048 bytes), crea un `PaqueteConecta` y lo encola por ESP-NOW: `200 encolado_espnow`, `503 recibido_sin_enlace`, `400`, `413`. **No persiste. Formato incompatible con `data/` (B13).** | Backend |
| `src/main.cpp` | Crea `ServidorHotspot`, procesa DNS en `loop()` y registra estaciones conectadas | Backend |
| `lib/Protocol/Protocol.h`, `lib/Pins/Config.h`, `lib/EspNowLink/` | `PaqueteConecta` empaquetado (≤ 250 bytes), categorías 1–6, canal y MAC del concentrador, enlace ESP-NOW | Backend |
| `lib/Lora/Lora.h` | Vacío | GIS/LoRa |
| `data/Data.sql` | Vacío. En esta rama `Data/` se renombró a `data/` (pendiente de aprobación del backend) | Backend |

No existen rutas de estado, verificación, autorizaciones ni solicitudes del titular; tampoco persistencia (`reportes.jsonl`), confirmación de guardado ni deduplicación. El enlace hacia el PMU es ESP-NOW (no LoRa). No se ha compilado el firmware completo ni probado la placa desde esta rama.

## Responsabilidades (bitácora, sección 3)

- **Backend (no modificar):** `lib/Servidor/`, `src/main.cpp`, `platformio.ini`, persistencia en LittleFS, confirmación de recepción, endpoints HTTP y enlace LoRa.
- **Este equipo:** interfaz ciudadana en `data/`, validación, triaje preliminar explicable y módulo C++ portable `lib/Triaje/` para que el backend lo integre.
- **Tablero PMU:** otro integrante; se le entrega contrato de incidentes agrupados y una maqueta fuera de `data/`.

## Fuentes de verdad

- `docs/referencia/CONECTA_redisenado_experiencia_v5.html`: interfaz, flujo y textos legales (congelada).
- `docs/referencia/CONECTA_PROMPT_MAESTRO_CODEX_CLAUDE.md`: fases y criterios de aceptación.
- `docs/ai/V5_MANIFIESTO.md`: requisitos inalterables y registro de problemas de V5.
- `tests/web/base_legal.json` y `tests/web/verificar_legal.mjs`: 130 fragmentos legales que deben seguir literalmente en `data/`.

## Pendientes ajenos a este equipo

Contrato HTTP aprobado, persistencia, acuse real, idempotencia en servidor, autenticación de reportes, formato LoRa, `[Entidad responsable]` y demás campos legales. No resolverlos con supuestos invisibles en el frontend.
