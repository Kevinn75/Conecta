# Claude planifica; Codex implementa

## Alcance y requisitos

Esta integración es para **Claude Code con terminal**, en el mismo checkout que Codex CLI. Adjuntar documentos al chat web de Claude no le da acceso a tu terminal ni instala Codex. En Windows, WSL2 ofrece un entorno consistente para ambos CLIs; usar Python 3.10+, Git, Node.js 22.20+ para las herramientas recomendadas y GitHub CLI autenticado para publicar.

No se ha instalado Claude Code ni se han conectado cuentas desde este repositorio. Las cuentas y permisos se configuran en tu equipo. El acceso y consumo de Claude y Codex son independientes; no asumir que una suscripción cubre la otra o el uso de ambas APIs.

## Herramientas mínimas y repositorios

| Prioridad | Herramienta / repositorio | Uso y responsable |
| --- | --- | --- |
| Fundamental | [Codex CLI](https://github.com/openai/codex) | Trabajador invocado por Claude mediante `codex exec` |
| Fundamental | [Claude Code](https://github.com/anthropics/claude-code) | Orquestación, planificación y revisión |
| Fundamental | [frontend-design](https://github.com/anthropics/claude-plugins-official/tree/main/plugins/frontend-design) | Claude define dirección visual y la convierte en un brief para Codex |
| Fundamental | [web-design-guidelines](https://github.com/vercel-labs/agent-skills/tree/main/skills/web-design-guidelines) | Claude audita accesibilidad y UX sobre los archivos afectados |
| Fundamental al existir UI | [Playwright CLI](https://github.com/microsoft/playwright-cli) | Navegación, capturas y verificación de estados; cargarlo cuando se pruebe interfaz |
| Fundamental para publicar | [GitHub CLI](https://github.com/cli/cli) | Rama, diff, PR, checks y merge; lo opera el orquestador |
| Opcional | [Context7](https://github.com/upstash/context7) | Consultar documentación de una dependencia cuando exista una duda concreta |
| Opcional | [GitHub MCP](https://github.com/github/github-mcp-server) | Alternativa a `gh`; no hace falta activar ambos para la misma operación |
| Opcional | [Playwright MCP](https://github.com/microsoft/playwright-mcp) | Útil para exploración prolongada con estado del navegador; preferir CLI para tareas acotadas |

El repositorio [anthropics/skills](https://github.com/anthropics/skills) también contiene frontend-design; es una alternativa de distribución, no otra skill que debas duplicar. Las skills de React/Next.js de Vercel se justifican solo si el proyecto adopta esas tecnologías. No agregar Figma sin un archivo de diseño que usar, ni un servidor de “razonamiento” para tareas que el modelo ya puede resolver.

Una skill proporciona instrucciones; un plugin empaqueta capacidades; MCP expone herramientas. Ninguno garantiza calidad ni ahorro por sí mismo. Aquí el ahorro se busca con alcance pequeño y evidencia breve, no con un porcentaje prometido.

## Instalación en tu equipo

Instalar Claude Code desde su [guía oficial](https://code.claude.com/docs/en/overview), Codex desde su README y GitHub CLI desde su repositorio. En la raíz de CONECTA:

```bash
codex --version
codex login
gh auth login
gh auth status
claude --model claude-opus-5-5
```

Dentro de Claude Code:

```text
/plugin install frontend-design@claude-plugins-official
```

Si el marketplace oficial no aparece, usar `/plugin > Discover` según la documentación. No instalar todo `example-skills` solo para obtener frontend-design.

Desde terminal, instalar selectivamente la auditoría y el navegador:

```bash
npx skills add vercel-labs/agent-skills --skill web-design-guidelines -a claude-code
npm install -g @playwright/cli@latest
playwright-cli install --skills
playwright-cli --help
```

Estos comandos son instrucciones de instalación, no operaciones ya ejecutadas. Inspeccionar los archivos y licencias que incorpora cada paquete antes de confirmarlos en Git. Registrar versiones resueltas y los archivos de bloqueo generados; `latest` no es reproducible. No actualizar herramientas a mitad de una tarea. Las guías de Vercel consultan reglas remotas: registrar la revisión usada en una auditoría importante.

Para consultar documentación de OpenAI desde Claude, añadir solo cuando haga falta:

```bash
claude mcp add --transport http openaiDeveloperDocs https://developers.openai.com/mcp
```

## Modelos y conexión

- Claude: `claude-opus-5-5`, fijado en `.claude/settings.json`; comprobar el modelo efectivo con `/model`. Puede requerir actualización del CLI y acceso de cuenta.
- Codex: `gpt-6-astra`, fijado como valor por defecto del wrapper. Se puede seleccionar otro modelo explícitamente con `--model`, nunca sustituirlo en silencio.
- Esfuerzo del trabajador: `medium` para tareas acotadas, `high` para fallos o decisiones complejas; usar `xhigh/max` solo cuando se haya comprobado una mejora que compense su coste.

`codex mcp-server` fue retirado. No usar tutoriales antiguos que lo registran como servidor MCP de Claude. Esta integración llama al modo no interactivo `codex exec`; `codex app-server` tiene otro protocolo y no es un reemplazo MCP directo.

## Ejecutar una tarea

1. Claude escribe un objetivo y criterios observables, identifica archivos relevantes y guarda el contrato en `docs/ai/tasks/`.
2. Crear una rama y confirmar el contrato después de revisarlo. No ocultar ni descartar cambios ajenos para obtener un árbol limpio.
3. Inspeccionar el comando sin llamar a un modelo:

```bash
python3 scripts/ai/codex_worker.py docs/ai/tasks/t1-contrato-reglas.json --mode implement
```

4. Ejecutar en la rama limpia cuando esa tarea esté autorizada:

```bash
python3 scripts/ai/codex_worker.py docs/ai/tasks/t1-contrato-reglas.json --mode implement --execute
```

Para diagnóstico sin modificaciones, ejecutar `repository-audit.json` en el modo predeterminado `inspect`, añadiendo `--execute` después de previsualizarlo. `--timeout` limita cada proceso y `--effort high` permite más esfuerzo cuando la tarea lo requiera.

El wrapper envía el contrato por stdin, selecciona sandbox, guarda eventos y el resultado JSON, comprueba cambios fuera de alcance y ejecuta los checks declarados como argumentos, sin shell. No crea commits, PR ni merges. Un informe `blocked`, error de proceso, check fallido o modificación fuera de alcance produce salida no exitosa.

`approval_policy=never` hace que las acciones que necesiten aprobación fallen; no elimina el sandbox. La red del sandbox de escritura está desactivada. Preparar dependencias antes de delegar, sin ampliar permisos automáticamente si algo falla.

`write` es un contrato con comprobación posterior de diferencias, **no un aislamiento de seguridad por carpeta**. El sandbox de escritura abarca el workspace y la comparación no detecta cambios en archivos ignorados, acciones externas ni escrituras revertidas. Usar un checkout o worktree dedicado y sin datos privados. Las configuraciones locales y MCP del CLI deben revisarse; este script no las desactiva ni las sustituye.

No ejecutar trabajadores concurrentes sobre el mismo checkout. `worker.lock` evita dos invocaciones simultáneas del wrapper; tras una interrupción abrupta, confirmar que no queda un proceso activo antes de retirar un lock obsoleto. En Windows nativo, un timeout termina el proceso principal; WSL/Linux también termina su grupo de procesos.

## Revisión y ahorro de contexto

Claude lee `report.json`, `verification.json` y el diff real. Los eventos completos quedan en `.ai-runs/` y no se cargan normalmente ni se publican. `model_requested` documenta la selección solicitada, no demuestra por sí solo el modelo servido. Los eventos del proveedor pueden aportar metadatos de modelo y uso; no inventar métricas ausentes.

Una tarea debe cubrir un resultado comprobable, normalmente un componente o flujo. Evitar un trabajo completo por cada pequeño cambio de texto y evitar también “construye toda la app”. Reutilizar contexto estable en archivos, no reenviar conversaciones. Pedir decisiones y pruebas, no cadenas internas de razonamiento.

Los checks del wrapper proporcionan códigos de salida independientes del relato del modelo; no prueban que el diseño sea correcto. Claude debe inspeccionar el comportamiento visual y casos de error. Un `passed: true` en una auditoría sin checks solo significa que el proceso y sus restricciones terminaron correctamente. Revisar siempre qué se verificó.

## Publicar mediante PR

Después de revisar y confirmar cambios, con acceso al repositorio:

```bash
git push -u origin NOMBRE_DE_RAMA
gh pr create --repo Kevinn75/Conecta --base main --head NOMBRE_DE_RAMA --title "Título concreto" --body-file /ruta/al/cuerpo-del-pr.md
gh pr view NUMERO --repo Kevinn75/Conecta --json url,headRefOid,mergeable,mergeStateStatus,reviewDecision,statusCheckRollup
gh pr checks NUMERO --repo Kevinn75/Conecta
```

Comprobar que el HEAD revisado no cambió y que las revisiones y checks requeridos pasan. Si no existen checks remotos, documentarlo y usar la evidencia local, sin presentarlo como CI verde. Con merge autorizado y ningún bloqueo:

```bash
gh pr merge NUMERO --repo Kevinn75/Conecta --squash --match-head-commit SHA_REVISADO
gh pr view NUMERO --repo Kevinn75/Conecta --json url,state,mergedAt,mergeCommit
```

No usar `--admin`, force push ni eliminar controles de protección. Un error de autenticación es un bloqueo de acceso; no una petición de permiso repetida para un merge que el usuario ya autorizó.

## Prompt para iniciar Claude

> Trabaja en Kevinn75/Conecta. Lee CLAUDE.md, AGENTS.md y docs/ai/CONTEXTO.md. Actúa como orquestador con Opus 5.5: define el brief, divide la petición en tareas comprobables y delega la programación a Codex GPT-6 Astra con scripts/ai/codex_worker.py. Usa frontend-design para el brief, web-design-guidelines para revisar y Playwright cuando exista UI. No inventes backend ni confirmaciones del PMU. Revisa el diff y las pruebas; corrige hallazgos bloqueantes. Prepara PR y merge cuando estén autorizados y se cumplan los controles del repositorio. Empieza mostrando el estado comprobado y la primera tarea concreta.

## Fuentes verificadas el 20 de noviembre de 2026

- [Modo no interactivo de Codex](https://learn.chatgpt.com/docs/non-interactive-mode)
- [Retirada del servidor MCP de Codex](https://learn.chatgpt.com/docs/mcp-server)
- [GPT-6 Astra](https://developers.openai.com/api/docs/models/gpt-6-astra)
- [Configuración de modelos Claude](https://code.claude.com/docs/en/model-config)
- [Skills de Claude Code](https://code.claude.com/docs/en/skills)
- [OpenAI Docs MCP](https://developers.openai.com/learn/docs-mcp)
- READMEs oficiales de los repositorios enlazados en la tabla.

Las versiones y la disponibilidad cambian. Comprobar `--help`, `/model`, permisos de cuenta y documentación antes de actualizar la integración.
