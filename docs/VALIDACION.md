# Validación de CONECTA (frontend LittleFS, triaje y lib/Triaje)

Ejecución final del 20 de noviembre de 2026 sobre la rama `ai/claude-codex-orquestacion`, después de T2b. Las pruebas las ejecutó Claude; no se toman de los resúmenes de Codex.

## Resultados

| Área | Comando | Resultado |
| --- | --- | --- |
| Sintaxis JS | `node --check data/{app,triaje,lugares}.js` | 3/3 sin errores |
| Unitarias JS (Codex) | `node --test "tests/web/*.test.mjs"` | **80/80** |
| Textos legales literales | `node tests/web/verificar_legal.mjs data` | **130/130** fragmentos, `CONSENT_VERSION v4-2026-11-20` |
| Auditoría en navegador (Claude) | `node tests/navegador/node_modules/@playwright/test/cli.js test --config tests/navegador/playwright.config.mjs` | **19/19** en Chromium 1255, viewport iPhone 13 |
| C++ en PC | `python -I tests/triaje/ejecutar.py` (g++ 16.2, `-Wall -Wextra -Werror`) | Todos los casos; JSON validado con `json.loads` |
| C++ para ESP32-S3 | `pio ci tests/triaje/esp32_compila.cpp --lib lib/Triaje --board esp32-s3-devkitc-1` | SUCCESS · RAM 9,8 % · flash 7,4 % |
| Integración con el `Servidor.h` real (`047e96a`) | `pio ci tests/triaje/integracion_servidor/main.cpp --lib lib/Triaje --lib lib/Servidor --lib lib/Pins --lib lib/Protocol --project-conf platformio.ini -e estacion` | SUCCESS · RAM 15,9 % (51 988 B) · flash 12,5 % (con `307e52d`: 51 972 B · 12,4 %) |
| Imagen LittleFS | `pio run -t buildfs -e estacion` | SUCCESS · `littlefs.bin` 3 538 944 B con `app.js`, `Data.sql`, `estilo.css`, `index.html`, `lugares.js`, `triaje.js` |
| Backend sin cambios | `git diff --stat origin/main HEAD -- lib/Servidor src platformio.ini lib/Lora lib/Protocol lib/Pins` | Sin diferencias |

## Qué cubre la auditoría en navegador (`tests/navegador/auditoria.spec.mjs`)

1. Flujos legales en V5 y `data/` lado a lado: 21 pantallas (privacidad, consentimiento marcado/desmarcado/marcado, permiso y datos de contacto, verificación fallida y correcta, historial, revocación de contacto y salud, solicitudes de corrección y supresión, salud, ruta urgente art. 10c). Texto visible y estado de casillas idénticos.
2. Cambio de ruta urgente↔normal sin respuestas residuales en el envío ni en el triaje.
3. HTTP simulado por prueba (no hay backend real): `200 {status:"en_espera"}`, acuse con otro ID y respuesta no JSON → «Envío sin confirmar»; «guardado» solo con acuse válido y mismo ID; doble clic → 1 POST; corte tras POST → reintento con el mismo ID y cuerpo idéntico; sin `crypto.subtle` el aviso se crea y no hay `hashClave`.
4. Offline: ninguna petición fuera del origen local; 514 lugares cargados.
5. axe-core 4.14 (WCAG 2.0/2.1/2.2 A y AA): sin violaciones nuevas frente a V5 (persisten 14 nodos de contraste en pantallas legales, idénticos a V5).
6. Foco al encabezado al cambiar de pantalla; orden de tabulación.
7. Sin scroll horizontal a 360 y 390 px.
8. Buscador sin tildes y «No sé en qué barrio estoy» con referencia obligatoria.
9. Diseño idéntico a V5 píxel a píxel en inicio, ubicación, necesidades, espera, privacidad y urgencia.
10. Aviso urgente crítico con cifra opcional inválida: se envía como crítico con `personas: null` y V3.
11. El menú de sugerencias no tapa el botón «No sé en qué barrio estoy».

## Tamaño de `data/`

| Archivo | Bytes | gzip -9 |
| --- | --- | --- |
| `index.html` | 1 909 | 995 |
| `estilo.css` | 16 837 | — |
| `app.js` | 51 073 | — |
| `lugares.js` | 17 887 | 6 613 |
| `triaje.js` | 15 891 | — |
| **Total** | **103 597** | ≈ 34 000 |

**Objetivo < 100 000 bytes: no cumplido** (exceso de 3 597 bytes). V5 ocupaba 132 217 (−21,6 %). Causa: el motor de triaje (15,9 KB) es código nuevo; el catálogo se compactó de 62 133 a 17 887 bytes sin perder registros; el JS y el CSS ya no tienen comentarios ni sangrías. Alternativa negociable: servir `*.gz` desde LittleFS (ESPAsyncWebServer los entrega si existen), ≈ 34 KB; requiere acuerdo y prueba en la placa.

## Compatibilidad con el backend actual

Compilar juntos no significa que el formato sea compatible: el `POST /api/reporte` de `047e96a` lee otros campos (ver `BLOQUEANTES.md` B13). Comprobado con `triaje.construirPayload`: el payload urgente (1 334 bytes) y el normal (1 542 bytes) no contienen `barrioId`, `urgencia`, `afectados` ni `necesidadesMask`, y `categoria` es texto. Ningún dato de prueba se envió a la placa.

## Entorno y límites

- Windows 11, Node 26.13.1, Python 3.13.2, g++ 16.2.0 (WinLibs UCRT), PlatformIO 6.2.0, Playwright 1.65.1.
- MinGW falla al enlazar si su ruta contiene espacios (por ejemplo, una carpeta de usuario con espacio en el nombre). En este equipo: enlace `C:\Users\Public\mingw64` y `LIBRARY_PATH=C:/Users/Public/mingw64/x86_64-w64-mingw32/lib`.
- **NO EJECUTADAS:** carga en la placa real, portal cautivo con teléfonos, Safari/VoiceOver reales, tiempos de carga desde la ESP32, LoRa, persistencia, acuses del PMU. Compilar no prueba hardware.
- Las 9 pruebas de `tests/ai` del puente Codex fallan en Windows nativo por diseño POSIX (ver `docs/ai/VALIDACION.md`); no afectan a este entregable.
