"""Contract tests with a fake Codex process; no model calls or credentials."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[2] / "scripts/ai/codex_worker.py"
spec = importlib.util.spec_from_file_location("codex_worker", SOURCE)
worker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(worker)

FAKE = r'''
import json, os, pathlib, sys
args = sys.argv[1:]
behavior = os.environ.get("CONECTA_TEST_BEHAVIOR", "success")
task = json.loads(sys.stdin.read().splitlines()[-1])
if behavior == "exit":
    sys.exit(7)
if behavior == "write":
    pathlib.Path("web").mkdir(exist_ok=True)
    pathlib.Path("web/app.js").write_text("const demo = true;\n")
if behavior == "outside":
    pathlib.Path("firmware.cpp").write_text("unexpected\n")
if behavior == "git":
    import subprocess
    subprocess.run(["git", "checkout", "-b", "unexpected"], check=True)
report = {"status": "blocked" if behavior == "blocked" else "complete",
          "summary": "Fixture", "files_changed": [], "evidence": [], "risks": []}
dest = pathlib.Path(args[args.index("--output-last-message") + 1])
dest.write_text("{}" if behavior == "malformed" else json.dumps(report))
print(json.dumps({"type": "fixture", "argv": args}))
'''


class WorkerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "repo"
        self.root.mkdir()
        self.bin = self.base / "bin"
        self.bin.mkdir()
        executable = self.bin / "codex"
        executable.write_text(f"#!{sys.executable}\n" + FAKE, encoding="utf-8")
        executable.chmod(0o755)
        self.task = {"id": "fixture", "objective": "Inspect fixture", "read": ["context.md"],
                     "write": [], "acceptance": ["Use evidence"], "checks": []}
        (self.root / ".gitignore").write_text(".ai-runs/\n")
        (self.root / "context.md").write_text("Fixture repository\n")
        self.taskfile = self.base / "task.json"
        for cmd in (["init", "-b", "work"], ["config", "user.email", "test@example.invalid"],
                    ["config", "user.name", "Test"], ["add", "."], ["commit", "-m", "fixture"]):
            self.git(*cmd)
        self.root_patch = patch.object(worker, "ROOT", self.root)
        self.root_patch.start()
        self.addCleanup(self.root_patch.stop)
        self.env_patch = patch.dict(os.environ, {"PATH": str(self.bin) + os.pathsep + os.environ["PATH"],
                                                "CONECTA_TEST_BEHAVIOR": "success"})
        self.env_patch.start()
        self.addCleanup(self.env_patch.stop)

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, stderr=subprocess.DEVNULL, text=True)

    def run_task(self, *args, behavior="success"):
        self.taskfile.write_text(json.dumps(self.task), encoding="utf-8")
        capture = io.StringIO()
        with patch.dict(os.environ, {"CONECTA_TEST_BEHAVIOR": behavior}), contextlib.redirect_stdout(capture):
            code = worker.main([str(self.taskfile), *args])
        return code, json.loads(capture.getvalue())

    def implementation(self):
        self.task["write"] = ["web/"]

    def test_preview_does_not_execute_or_write(self):
        code, data = self.run_task()
        self.assertEqual(code, 0)
        self.assertTrue(data["preview"])
        self.assertFalse((self.root / ".ai-runs").exists())

    def test_inspect_uses_read_only_and_explicit_model(self):
        code, data = self.run_task("--execute")
        self.assertEqual(code, 0)
        self.assertEqual(data["changed_paths"], [])
        event = json.loads((self.root / data["run"] / "events.jsonl").read_text())
        self.assertIn("read-only", event["argv"])
        self.assertIn("gpt-6-astra", event["argv"])
        self.assertIn('approval_policy="never"', event["argv"])

    def test_implementation_checks_are_actually_run(self):
        self.implementation()
        self.task["checks"] = [[sys.executable, "-c", "from pathlib import Path; assert Path('web/app.js').is_file()"]]
        code, data = self.run_task("--mode", "implement", "--execute", behavior="write")
        self.assertEqual(code, 0)
        self.assertEqual(data["changed_paths"], ["web/app.js"])
        self.assertEqual(data["checks"][0]["exit_code"], 0)

    def test_failed_check_prevents_success(self):
        self.implementation()
        self.task["checks"] = [[sys.executable, "-c", "raise SystemExit(3)"]]
        code, data = self.run_task("--mode", "implement", "--execute")
        self.assertEqual(code, 1)
        self.assertFalse(data["passed"])
        self.assertEqual(data["checks"][0]["exit_code"], 3)

    def test_worker_failure_blocked_and_malformed_reports(self):
        for behavior in ("exit", "blocked", "malformed"):
            with self.subTest(behavior=behavior):
                code, data = self.run_task("--execute", behavior=behavior)
                self.assertEqual(code, 1)
                self.assertFalse(data["passed"])
                self.assertIn("error", data)
                self.assertFalse((self.root / ".ai-runs/worker.lock").exists())

    def test_out_of_scope_changes_preserved_and_flagged(self):
        self.implementation()
        code, data = self.run_task("--mode", "implement", "--execute", behavior="outside")
        self.assertEqual(code, 1)
        self.assertEqual(data["out_of_scope"], ["firmware.cpp"])
        self.assertTrue((self.root / "firmware.cpp").exists())

    def test_branch_changes_are_flagged(self):
        code, data = self.run_task("--execute", behavior="git")
        self.assertEqual(code, 1)
        self.assertIn("error", data)

    def test_protected_branch_and_dirty_tree_refused(self):
        self.implementation()
        self.git("branch", "-m", "main")
        with self.assertRaisesRegex(ValueError, "rama de trabajo"):
            self.run_task("--mode", "implement", "--execute")
        self.git("branch", "-m", "work")
        (self.root / "context.md").write_text("User work\n")
        with self.assertRaisesRegex(ValueError, "limpio"):
            self.run_task("--mode", "implement", "--execute")

    def test_lock_prevents_concurrent_worker(self):
        (self.root / ".ai-runs/worker.lock").mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, "worker.lock"):
            self.run_task("--execute")

    def test_inspect_rejects_write_or_checks(self):
        self.implementation()
        with self.assertRaisesRegex(ValueError, "inspect"):
            self.run_task()
        self.task["write"] = []
        self.task["checks"] = [["echo", "not inspection"]]
        with self.assertRaisesRegex(ValueError, "inspect"):
            self.run_task()

    def test_traversal_absolute_reserved_and_symlink_escape_refused(self):
        for name in ("../elsewhere", "/tmp/external", ".git/config", ".", "C:/external", "web\\x"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                worker.valid_path(name)
        (self.root / "escape").symlink_to(self.base, target_is_directory=True)
        with self.assertRaises(ValueError):
            worker.valid_path("escape/file")

    def test_scope_distinguishes_file_directory_and_prefix(self):
        self.assertTrue(worker.in_scope("web/app.js", ["web/"]))
        self.assertFalse(worker.in_scope("web-old/app.js", ["web/"]))
        self.assertFalse(worker.in_scope("web/app.js", ["web"]))

    def test_contract_and_missing_references_refused(self):
        self.task["checks"] = ["echo unsafe shell"]
        with self.assertRaisesRegex(ValueError, "argv"):
            self.run_task()
        self.task["checks"] = []
        self.task["read"] = ["missing.md"]
        with self.assertRaisesRegex(ValueError, "No existe"):
            self.run_task()


if __name__ == "__main__":
    unittest.main()
