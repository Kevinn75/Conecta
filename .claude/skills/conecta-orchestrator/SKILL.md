---
name: conecta-orchestrator
description: Planificar, delegar a Codex y revisar tareas de frontend de CONECTA. Usar al implementar o revisar una interfaz, dividir trabajo o preparar un PR de este proyecto con Claude como orquestador.
---

Leer `CLAUDE.md` y `docs/ai/CONTEXTO.md` desde la raíz del repositorio.
Para decisiones visuales, leer `docs/ai/FRONTEND.md`; para comandos, `docs/ai/GUIA.md`.

Crear un contrato JSON pequeño con objetivo, archivos de lectura, rutas de escritura, aceptación y checks. Usar `docs/ai/tasks/frontend-foundation.json` como ejemplo; no ejecutarlo sin adaptar el objetivo a la petición actual.

Preparar una rama limpia y ejecutar:

```bash
python3 scripts/ai/codex_worker.py RUTA_TAREA.json --mode implement
python3 scripts/ai/codex_worker.py RUTA_TAREA.json --mode implement --execute
```

Revisar los cambios y `.ai-runs/<ejecucion>/verification.json`; abrir logs solo si fallan. Evaluar formularios, accesibilidad y estados de red con evidencia cuando haya frontend. Formular correcciones acotadas. No confundir `status: complete` con aprobación del revisor.

Antes de publicar, leer el alcance de autorización vigente y comprobar el HEAD revisado, los checks y las revisiones exigidas. Si están cumplidos, crear el PR y hacer merge. Informar el enlace y el estado real; si falta acceso, conservar el trabajo y describir el bloqueo.
