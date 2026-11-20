# Validación del puente de desarrollo

Fecha: 20 de noviembre de 2026.

## Comprobaciones realizadas

- `python3 -m unittest discover -s tests/ai -v`: **13 pruebas aprobadas** con repositorios temporales y un ejecutable Codex simulado. Cubren invocación, informe, checks reales de procesos de prueba, alcance, ramas, árbol sucio, lock y validación de rutas.
- Previsualización de `repository-audit.json`: correcta, sin ejecutar inferencia ni crear archivos.
- `git diff --check`: sin problemas de espacios.
- Se verificó `codex exec --help` con CLI `0.172.0-alpha.2`: reconoce las opciones que utiliza el wrapper. No existe el antiguo subcomando `mcp-server`.

## Prueba real intentada

La ejecución de solo lectura de `repository-audit.json` terminó con código 1 antes de iniciar una tarea de modelo. El CLI no pudo inicializar su app-server interno porque su directorio de estado en este entorno está en un sistema de archivos de solo lectura.

El wrapper detectó el error, devolvió `passed: false`, dejó el repositorio sin modificaciones y liberó su lock. No se intentó cambiar credenciales ni desactivar el aislamiento. **Esta prueba no demuestra una llamada exitosa a GPT-6 Astra** ni un fallo de ese modelo; la falla fue de inicialización local.

## Límites

Claude Code no está instalado en este entorno: no se ha ejecutado el recorrido completo iniciado por Claude Opus 5.5. Las herramientas externas recomendadas se documentaron, pero no se instalaron aquí. La skill de proyecto se entrega en el repositorio para su carga por Claude Code, no como una instalación personal global.

No se modificó ni compiló el firmware. No existe interfaz nueva en este cambio, por lo que no hay pruebas visuales ni de accesibilidad del producto. Tampoco se verificaron radio, hardware, backend o acuses del PMU. Antes de confiar en el puente, repetir la auditoría de solo lectura desde un equipo con ambos CLIs configurados y acceso a los modelos solicitados.
