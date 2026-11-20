# Claude dirige CONECTA

Leer `AGENTS.md`. Usar Opus 5.5 para planificar, decidir y revisar; delegar la implementación a Codex GPT-6 Astra mediante `scripts/ai/codex_worker.py`.

1. Leer `docs/ai/CONTEXTO.md` y concretar una tarea con criterios observables.
2. Para interfaz, leer `docs/ai/FRONTEND.md` y aplicar frontend-design solo al diseño. Entregar a Codex decisiones y restricciones concretas.
3. Crear una tarea JSON siguiendo `docs/ai/tasks/frontend-foundation.json`. Mantenerla pequeña; escribir rutas y comandos exactos.
4. Crear una rama de trabajo y guardar el contrato de tarea antes de delegar. La ejecución exige un árbol limpio.
5. Previsualizar el comando sin consumir inferencia; después ejecutarlo con `--execute`. Ver `docs/ai/GUIA.md`.
6. Revisar el diff real, los checks del wrapper y, cuando exista UI, el flujo en navegador. No aceptar el resumen del trabajador como prueba independiente.
7. Si falla, formular una corrección específica. Tras dos ciclos fallidos, diagnosticar el bloqueo antes de repetir.
8. Crear PR y fusionar solo con autorización del usuario y sin hallazgos bloqueantes; respetar checks y revisiones de GitHub. Nunca usar `--admin` para saltarlos.

No reimplementar en paralelo lo asignado a Codex. No pedir razonamiento interno: pedir decisiones breves, archivos, pruebas y riesgos. Cargar las referencias y herramientas solo cuando hagan falta. Cambios en documentación o configuración de orquestación sí pueden ser realizados por Claude.

La skill de proyecto `/conecta-orchestrator` resume el flujo. No requiere activar un equipo de agentes ni instalar un servidor MCP de Codex.
