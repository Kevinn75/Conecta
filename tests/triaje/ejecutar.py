"""Checks de PC sin dependencias: compila, ejecuta asserts y parsea JSON real."""
import json
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    raiz = Path(__file__).resolve().parents[2]
    compilador = shutil.which("g++")
    if compilador is None:
        print("blocked: se requiere g++ disponible en PATH; no se instala", file=sys.stderr)
        return 2
    destino = raiz / ".pio" / "triaje-host"
    destino.mkdir(parents=True, exist_ok=True)
    binario = destino / ("test_triaje.exe" if sys.platform == "win32" else "test_triaje")
    subprocess.run([
        compilador, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1",
        "-fno-exceptions", "-fno-rtti", "-I", str(raiz / "lib" / "Triaje"),
        str(raiz / "lib" / "Triaje" / "Triaje.cpp"),
        str(raiz / "tests" / "triaje" / "test_triaje.cpp"),
        "-o", str(binario),
    ], cwd=raiz, check=True)
    subprocess.run([str(binario)], cwd=raiz, check=True)
    salida = subprocess.run([str(binario), "--json"], cwd=raiz, check=True,
                            capture_output=True, encoding="utf-8").stdout
    datos = json.loads(salida)
    assert datos["zona"] == 'Zona "A"\\B\n\t\r\b\f\x01'
    assert datos["razones"] == ['Revisión: "sí"']
    assert datos["reportes"] == ["JSON-1"]
    assert datos["prioridadSugerida"] == "critico"
    assert datos["necesidades"]["No sé todavía"] == 0
    assert datos["cantidadReportes"] == 1
    assert datos["cantidadPersonasDeclaradas"] == 3
    print("JSON: escape, Unicode y estructura comprobados con json.loads")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
