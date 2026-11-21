# Contexto mínimo de CONECTA

Inspección del 20 de noviembre de 2026 sobre `main` en `307e52d` («Actualiza servidor y archivos de la estacion», responsable de backend) de https://github.com/Kevinn75/Conecta.
Releer los archivos si el repositorio evoluciona; este inventario es una instantánea.

## Comprobado en el repositorio

| Archivo | Estado en la base inspeccionada | Responsable |
| --- | --- | --- |
| `platformio.ini` | ESP32-S3, `board_build.filesystem = littlefs`, particiones `default_16MB.csv`, PSRAM, entornos `estacion` y `concentrador` | Backend |
| `lib/Servidor/Servidor.{h,cpp}` | Clase `ServidorHotspot`: SoftAP abierto `punto_de_ayuda_conecta` en `192.168.4.1`, DNS cautivo, sondas de portal (302), `serveStatic("/", LittleFS, "/")` | Backend |
| `POST /api/reporte` | **Responde siempre `200 {"status":"en_espera","mensaje":"Endpoint listo"}`. No lee, valida ni guarda el cuerpo.** | Backend |
| `src/main.cpp` | Crea `ServidorHotspot`, procesa DNS en `loop()` y registra estaciones conectadas | Backend |
| `lib/Lora/Lora.h`, `lib/Protocol/Protocol.h`, `lib/Pins/Config.h` | Vacíos | GIS/LoRa |
| `data/Data.sql` | Vacío. En esta rama `Data/` se renombró a `data/` (pendiente de aprobación del backend) | Backend |

No existen rutas de estado, verificación, autorizaciones ni solicitudes del titular; tampoco persistencia (`reportes.jsonl`), confirmación de guardado, deduplicación ni enlace LoRa. No se ha compilado el firmware completo ni probado la placa desde esta rama.

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
