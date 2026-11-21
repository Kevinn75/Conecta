"""Genera la línea base de textos legales de V5 (Fase 0). No modifica V5.

Extrae los segmentos de texto estático de las pantallas legales, de las
autorizaciones versionadas y de los avisos legales de las pantallas no legales.
El resultado sirve para comprobar que `data/` conserva esos textos literalmente.
Uso: python -I tests/web/generar_base_legal.py
"""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
V5 = ROOT / "docs/referencia/CONECTA_redisenado_experiencia_v5.html"
OUT = ROOT / "tests/web/base_legal.json"

# Líneas 1-indexadas de V5 que contienen material legal.
RANGOS = {
    "autorizaciones_versionadas": (121, 128),
    "aviso_urgente_ley_1581": (167, 167),
    "inicio_avisos": (169, 169),
    "revision_consentimiento": (179, 179),
    "comprobante_avisos": (180, 180),
    "pantallas_legales": (184, 207),
}


def literales(codigo):
    """Devuelve el contenido estático de las cadenas JS ('', "", ``) del código.

    En plantillas, cada ${...} corta el segmento y su expresión se analiza a su vez.
    Los comentarios /* */ y // se ignoran.
    """
    salida, i, n = [], 0, len(codigo)
    while i < n:
        if codigo.startswith("/*", i):
            fin = codigo.find("*/", i + 2)
            i = n if fin < 0 else fin + 2
        elif codigo.startswith("//", i) and (i == 0 or codigo[i - 1] not in ":\\"):
            fin = codigo.find("\n", i)
            i = n if fin < 0 else fin
        elif codigo[i] in "'\"":
            q, j, buf = codigo[i], i + 1, []
            while j < n and codigo[j] != q:
                if codigo[j] == "\\":
                    j += 1
                buf.append(codigo[j])
                j += 1
            salida.append("".join(buf))
            i = j + 1
        elif codigo[i] == "`":
            j, buf = i + 1, []
            while j < n and codigo[j] != "`":
                if codigo.startswith("${", j):
                    salida.append("".join(buf))
                    buf, nivel, k = [], 1, j + 2
                    while k < n and nivel:
                        if codigo[k] in "'\"`":  # saltar cadenas internas al contar llaves
                            q = codigo[k]
                            k += 1
                            while k < n and codigo[k] != q:
                                k += 2 if codigo[k] == "\\" else 1
                        nivel += {"{": 1, "}": -1}.get(codigo[k], 0) if k < n else 0
                        k += 1
                    salida.extend(literales(codigo[j + 2:k - 1]))
                    j = k
                    continue
                if codigo[j] == "\\":
                    j += 1
                buf.append(codigo[j])
                j += 1
            salida.append("".join(buf))
            i = j + 1
        else:
            i += 1
    return salida


def fragmentos(codigo):
    vistos = []
    for lit in literales(codigo):
        for parte in re.split(r"<[^>]*>", lit):
            parte = parte.lstrip(">").strip()
            if "<" in parte or "=\"" in parte:  # restos de etiquetas cortadas por ${...}
                continue
            # Texto visible en español, no nombres de clases, ids ni acciones.
            if len(parte) >= 12 and re.search(r"[a-záéíóúñ]{2,}[,.:]? [(¿¡a-záéíóúñ]", parte, re.I) and parte not in vistos:
                vistos.append(parte)
    return vistos


def main():
    data = V5.read_bytes()
    lineas = data.decode("utf-8").split("\n")
    base = {"fuente": "docs/referencia/CONECTA_redisenado_experiencia_v5.html",
            "sha256": hashlib.sha256(data).hexdigest(), "bloques": {}}
    for nombre, (ini, fin) in RANGOS.items():
        frags = fragmentos("\n".join(lineas[ini - 1:fin]))
        base["bloques"][nombre] = {"lineas": [ini, fin], "fragmentos": frags}
    OUT.write_text(json.dumps(base, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    total = sum(len(b["fragmentos"]) for b in base["bloques"].values())
    print(f"{total} fragmentos legales en {len(RANGOS)} bloques -> {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
