// Comprueba que los textos legales y avisos de V5 siguen literalmente en los archivos indicados.
// Uso: node tests/web/verificar_legal.mjs [archivo-o-carpeta ...]   (por defecto: data/)
// Sale con código 1 si falta algún fragmento o cambia la versión del texto legal.
import { readFileSync, readdirSync, statSync } from "node:fs";
import { join, dirname, extname } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const base = JSON.parse(readFileSync(join(ROOT, "tests/web/base_legal.json"), "utf8"));
const objetivos = process.argv.slice(2).length ? process.argv.slice(2) : ["data"];

function archivos(ruta) {
  const abs = join(ROOT, ruta);
  if (statSync(abs).isFile()) return [abs];
  return readdirSync(abs).map((n) => join(ruta, n)).flatMap((r) =>
    statSync(join(ROOT, r)).isDirectory() ? archivos(r)
      : [".html", ".js"].includes(extname(r).toLowerCase()) ? [join(ROOT, r)] : []);
}

const texto = objetivos.flatMap(archivos).map((f) => readFileSync(f, "utf8")).join("\n");
const faltan = [];
let total = 0;
for (const [bloque, { fragmentos }] of Object.entries(base.bloques)) {
  for (const f of fragmentos) {
    total++;
    if (!texto.includes(f)) faltan.push(`${bloque}: «${f}»`);
  }
}
if (!texto.includes("v4-2026-11-20")) faltan.push("CONSENT_VERSION: «v4-2026-11-20»");

console.log(`Fragmentos legales verificados: ${total - faltan.length}/${total} en ${objetivos.join(", ")}`);
if (faltan.length) {
  console.error("FALTAN O CAMBIARON:\n" + faltan.map((x) => "  - " + x).join("\n"));
  process.exit(1);
}
