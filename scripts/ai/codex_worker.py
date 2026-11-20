#!/usr/bin/env python3
"""Run one bounded Codex task. Preview by default; never commit or publish."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import signal
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = ROOT / "docs/ai/worker-report.schema.json"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def valid_path(value):
    if not isinstance(value, str) or not value or "\\" in value:
        raise ValueError("Las rutas deben ser relativas al repositorio y usar /.")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or ":" in value or not path.parts:
        raise ValueError(f"Ruta inválida: {value}")
    if path.parts[0] in {".git", ".ai-runs"}:
        raise ValueError(f"Ruta reservada: {value}")
    if not (ROOT / value).resolve().is_relative_to(ROOT.resolve()):
        raise ValueError(f"Ruta fuera del repositorio: {value}")
    return path.as_posix() + ("/" if value.endswith("/") else "")


def load_task(path):
    task = json.loads(path.read_text(encoding="utf-8"))
    required = {"id", "objective", "read", "write", "acceptance", "checks"}
    if not isinstance(task, dict) or set(task) != required:
        raise ValueError("Contrato: usar exactamente id, objective, read, write, acceptance y checks.")
    if not isinstance(task["id"], str) or not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", task["id"]):
        raise ValueError("id debe tener entre 1 y 64 letras minúsculas, números o guiones.")
    if not isinstance(task["objective"], str) or not task["objective"].strip():
        raise ValueError("Falta objective.")
    for key in ("read", "write", "acceptance"):
        if not isinstance(task[key], list) or not all(isinstance(v, str) and v.strip() for v in task[key]):
            raise ValueError(f"{key} debe ser una lista de textos no vacíos.")
    if not task["acceptance"]:
        raise ValueError("Faltan criterios de aceptación.")
    for key in ("read", "write"):
        task[key] = [valid_path(v) for v in task[key]]
    for name in task["read"]:
        if not (ROOT / name).exists():
            raise ValueError(f"No existe la referencia: {name}")
    if not isinstance(task["checks"], list) or not all(
        isinstance(cmd, list) and cmd and all(isinstance(v, str) and v for v in cmd)
        for cmd in task["checks"]
    ):
        raise ValueError("checks debe contener listas argv, sin shell.")
    return task


def validate_report(value):
    keys = {"status", "summary", "files_changed", "evidence", "risks"}
    if not isinstance(value, dict) or set(value) != keys:
        raise ValueError("Informe incompleto o con campos inesperados.")
    if value["status"] not in ("complete", "blocked") or not isinstance(value["summary"], str):
        raise ValueError("Estado o resumen inválidos.")
    for key in ("files_changed", "evidence", "risks"):
        if not isinstance(value[key], list) or not all(isinstance(v, str) for v in value[key]):
            raise ValueError(f"Campo inválido: {key}")


def changed_paths():
    found = set()
    for args in (("diff", "--name-only", "--no-renames", "-z", "HEAD"),
                 ("ls-files", "--others", "--exclude-standard", "-z")):
        result = subprocess.check_output(["git", *args], cwd=ROOT).decode("utf-8")
        found.update(p for p in result.split("\0") if p)
    return sorted(found)


def in_scope(path, allowed):
    return any(path.startswith(p) if p.endswith("/") else path == p for p in allowed)


def run_logged(command, output, errors, timeout, prompt=None):
    with output.open("w", encoding="utf-8") as out, errors.open("w", encoding="utf-8") as err:
        process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.PIPE,
                                   stdout=out, stderr=err, text=True,
                                   start_new_session=(os.name == "posix"))
        try:
            process.communicate(prompt, timeout=timeout)
            return process.returncode
        except (subprocess.TimeoutExpired, KeyboardInterrupt):
            if os.name == "posix":
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
            process.wait()
            raise


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("task", type=Path)
    parser.add_argument("--mode", choices=("inspect", "implement"), default="inspect")
    parser.add_argument("--model", default="gpt-6-astra")
    parser.add_argument("--effort", choices=("low", "medium", "high", "xhigh", "max"), default="medium")
    parser.add_argument("--timeout", type=int, default=900, help="Segundos por proceso")
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args(argv)
    task = load_task(args.task.resolve())
    if args.timeout < 1:
        raise ValueError("timeout debe ser positivo.")
    if args.mode == "inspect" and (task["write"] or task["checks"]):
        raise ValueError("inspect exige write y checks vacíos.")
    if args.mode == "implement" and not task["write"]:
        raise ValueError("implement requiere rutas de escritura.")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    run_dir = ROOT / ".ai-runs" / f"{stamp}-{task['id']}-{uuid.uuid4().hex[:8]}"
    command = ["codex", "exec", "-C", str(ROOT), "--model", args.model,
               "--sandbox", "read-only" if args.mode == "inspect" else "workspace-write",
               "-c", 'approval_policy="never"',
               "-c", "sandbox_workspace_write.network_access=false",
               "-c", f'model_reasoning_effort="{args.effort}"',
               "--ephemeral", "--json", "--output-schema", str(SCHEMA),
               "--output-last-message", str(run_dir / "report.json"), "-"]
    prompt = (
        "Eres el trabajador Codex de CONECTA. Sigue AGENTS.md. "
        "Lee solo las referencias y el código necesario. Respeta estrictamente write. "
        "No hagas commit, push, PR, merge ni instales dependencias. "
        "No cambies Git ni las reglas del proyecto. Si falta un requisito, devuelve blocked. "
        "El wrapper ejecutará los checks declarados al terminar; no afirmes que pasaron. "
        "Devuelve JSON según el esquema, evidencia breve y riesgos reales; no razonamiento interno.\n"
        + json.dumps(task, ensure_ascii=False)
    )
    if not args.execute:
        print(json.dumps({"preview": True, "command": command, "task": task,
                          "note": "No se ejecutó Codex ni se crearon archivos."}, ensure_ascii=False, indent=2))
        return 0
    if not shutil.which("codex"):
        raise ValueError("No se encuentra Codex CLI. Consultar docs/ai/GUIA.md.")
    head = git("rev-parse", "HEAD")
    branch = git("branch", "--show-current")
    if args.mode == "implement" and (not branch or branch in ("main", "master")):
        raise ValueError("Implementar exige una rama de trabajo; no main/master ni HEAD separado.")
    if git("status", "--porcelain"):
        raise ValueError("El árbol debe estar limpio. Revisar y guardar los cambios antes de delegar.")
    run_dir.parent.mkdir(exist_ok=True)
    lock = run_dir.parent / "worker.lock"
    try:
        lock.mkdir()
    except FileExistsError:
        raise ValueError("Existe worker.lock: hay un trabajador activo o una ejecución interrumpida.")
    result = {"task": task["id"], "model_requested": args.model, "effort": args.effort,
              "head_before": head, "branch": branch, "checks": [], "passed": False}
    try:
        run_dir.mkdir()
        (run_dir / "task.json").write_text(json.dumps(task, ensure_ascii=False, indent=2), encoding="utf-8")
        result["codex_exit"] = run_logged(command, run_dir / "events.jsonl", run_dir / "stderr.log", args.timeout, prompt)
        if result["codex_exit"] != 0:
            raise ValueError("Codex falló; consultar stderr.log y events.jsonl de esta ejecución.")
        report = json.loads((run_dir / "report.json").read_text(encoding="utf-8"))
        validate_report(report)
        if report["status"] != "complete":
            raise ValueError("Codex informó un bloqueo; revisar report.json.")
        for i, check in enumerate(task["checks"]):
            code = run_logged(check, run_dir / f"check-{i}.log", run_dir / f"check-{i}.stderr.log", args.timeout)
            result["checks"].append({"argv": check, "exit_code": code})
        result["passed"] = all(c["exit_code"] == 0 for c in result["checks"])
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        result["error"] = str(exc)
    finally:
        try:
            result["head_after"] = git("rev-parse", "HEAD")
            result["changed_paths"] = changed_paths()
            result["out_of_scope"] = [p for p in result["changed_paths"] if not in_scope(p, task["write"])]
            if result["head_after"] != head or git("branch", "--show-current") != branch or result["out_of_scope"]:
                result["passed"] = False
                result["error"] = "Cambió Git o hay archivos fuera de alcance; revisar sin borrar automáticamente."
            if run_dir.exists():
                (run_dir / "verification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        finally:
            lock.rmdir()
    print(json.dumps({"run": str(run_dir.relative_to(ROOT)), **result}, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(2)
