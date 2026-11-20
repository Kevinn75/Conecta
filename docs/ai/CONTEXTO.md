# Contexto mínimo de CONECTA

Inspección del 20 de noviembre de 2026, base `6324ab813056a3720b7253edc00d63bf753bba4b`, rama `main` de https://github.com/Kevinn75/Conecta.
Releer los archivos si el repositorio evoluciona; este inventario es una instantánea.

## Comprobado en el repositorio

| Archivo | Estado en la base inspeccionada |
| --- | --- |
| `platformio.ini` | Arduino, plataforma espressif32, placa esp32-s3-devkitc-1, flash configurada de 16 MB, PSRAM y monitor 115200 |
| `env:estacion` | Define `ROL_ESTACION`; hereda dependencia EByte LoRa E220 de `[env]` |
| `env:concentrador` | Define `ROL_CONCENTRADOR`; declara ArduinoJson, ESPAsyncWebServer y AsyncTCP |
| `src/main.cpp` | Plantilla con `myFunction(2, 3)`, sin aplicación funcional |
| `lib/Pins/Config.h`, `lib/Lora/Lora.h`, `lib/Protocol/Protocol.h` | Vacíos |
| `lib/Servidor/principal.c`, `Data/Data.sql` | Vacíos |
| `test/README` | Instrucciones genéricas de PlatformIO; no pruebas del sistema |

No hay frontend, contrato HTTP, esquema SQL efectivo, protocolo LoRa implementado ni conexión comprobada al PMU en esa base. Las versiones de las dependencias de firmware no están fijadas en el archivo. No se ha comprobado aquí la placa física ni compilado el firmware.

Revisar en una tarea de firmware si el concentrador también necesita E220: su `lib_deps` no incluye explícitamente `${env.lib_deps}`. Es una observación de configuración, no un fallo de ejecución demostrado. Tampoco se ha decidido cómo servir recursos web ni qué sistema de archivos utilizará el dispositivo.

## Intención del producto, procedente del contexto del proyecto

CONECTA busca recoger necesidades de la comunidad durante emergencias y facilitar su revisión por el Puesto de Mando Unificado, incluso con pérdida de internet. Esa intención no acredita una implementación. Recolección y triaje son distintos de inventarios y entregas.

## Decisiones propuestas para el siguiente incremento

- Crear el frontend en `web/` como HTML/CSS/JavaScript estático, con recursos locales.
- Conservar el firmware actual mientras se acuerdan los contratos con sus responsables.
- Empezar con datos ficticios y un adaptador de demostración identificado como tal.
- Documentar por separado cada estado: borrador, pendiente, recibido por estación, confirmado por PMU.
- Definir con backend el acuse, los reintentos y la idempotencia antes de mostrar una recepción real.

Pendientes: contrato HTTP real, persistencia, manejo de duplicados, autenticación de reportes, formato LoRa, destinatario de cada estado y método de despliegue. No resolverlos con supuestos invisibles en el frontend.
