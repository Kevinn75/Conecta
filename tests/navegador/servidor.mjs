// Servidor estático de pruebas: /data/* desde data/ y /ref/v5.html desde la referencia congelada.
// No implementa /api/reporte: cada prueba lo simula con page.route para no inventar backend.
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { join, normalize, extname, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
const REPO = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const tipos = { '.html': 'text/html; charset=utf-8', '.css': 'text/css', '.js': 'application/javascript' };
createServer(async (req, res) => {
  const url = new URL(req.url, 'http://x');
  if (url.pathname === '/salud') return res.end('ok');
  let archivo = null;
  if (url.pathname === '/ref/v5.html') archivo = join(REPO, 'docs/referencia/CONECTA_redisenado_experiencia_v5.html');
  else if (url.pathname.startsWith('/data/')) {
    const rel = normalize(url.pathname.slice(6) || 'index.html');
    if (!rel.startsWith('..')) archivo = join(REPO, 'data', rel);
  }
  try {
    const cuerpo = await readFile(archivo);
    res.writeHead(200, { 'Content-Type': tipos[extname(archivo)] || 'application/octet-stream' });
    res.end(cuerpo);
  } catch { res.writeHead(404); res.end('no'); }
}).listen(8765, '127.0.0.1');
