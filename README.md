# CONECTA

Base del sistema embebido con dos roles, estación y concentrador, configurados en PlatformIO para ESP32-S3.

El código inicial contiene una plantilla de `src/main.cpp` y módulos vacíos. La existencia de carpetas o dependencias no implica que el servidor, LoRa o la base de datos estén implementados. Consultar el [estado comprobado](docs/ai/CONTEXTO.md).

## Desarrollo con Claude y Codex

Claude Code **Opus 5.5** planifica y revisa. Codex CLI **GPT-6 Astra** implementa tareas acotadas. Ambos trabajan sobre este repositorio; no hay dependencia de IA en el firmware ni en la futura web.

- [Guía, herramientas e instalación](docs/ai/GUIA.md)
- [Criterios de diseño de frontend](docs/ai/FRONTEND.md)
- [Primera tarea propuesta](docs/ai/tasks/t1-contrato-reglas.json)
- [Instrucciones para Claude](CLAUDE.md) y [reglas compartidas](AGENTS.md)

Previsualización sin llamar a un modelo:

```bash
python3 scripts/ai/codex_worker.py docs/ai/tasks/repository-audit.json
```

Pruebas locales del puente, sin claves ni inferencia:

```bash
python3 -m unittest discover -s tests/ai -v
```

Este cambio prepara el flujo de desarrollo. No incorpora todavía una interfaz de emergencia ni certifica una integración Claude→Codex ejecutada desde una instalación de Claude Code.
