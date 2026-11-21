// Auditoría independiente (Claude) de data/ frente a V5 en Chromium real con viewport de iPhone.
// No reutiliza las pruebas de Codex. /api/reporte se simula por prueba: no existe backend real aquí.
import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';
import { writeFileSync, mkdirSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';

const V5 = '/ref/v5.html', DATA = '/data/index.html';
// Los hallazgos se acumulan en disco: Playwright reinicia el proceso tras un fallo.
const DIR_SALIDA = fileURLToPath(new URL('../../test-results/navegador/', import.meta.url));
const SALIDA = DIR_SALIDA + 'hallazgos.json';
const anotar = (k, v) => {
  mkdirSync(DIR_SALIDA, { recursive: true });
  let h = {}; try { h = JSON.parse(readFileSync(SALIDA, 'utf8')); } catch {}
  h[k] = v; writeFileSync(SALIDA, JSON.stringify(h, null, 1));
};

const app = (p) => p.locator('#app');
const accion = (p, a) => app(p).locator(`[data-action="${a}"]`).first().click();
const elegir = (p, campo, valor) => app(p).locator(`[data-choice="${campo}"][data-value="${valor}"]`).click();
async function texto(p) {
  const t = await app(p).innerText();
  return t.replace(/DEMO-[A-Z0-9-]+/g, 'DEMO-X').replace(/\d/g, '#').split('\n').map((l) => l.trim()).filter(Boolean);
}
const casillas = (p) => p.$$eval('#app input[type=checkbox]', (els) => els.map((e) => `${e.id}:${e.checked}`));

async function ubicacionEnea(p) {
  await p.fill('#barrio', 'enea');
  await app(p).locator('.suggestion', { hasText: 'La Enea' }).first().click();
}
async function rutaNormalHastaRevision(p) {
  await elegir(p, 'danger', 'No'); await accion(p, 'next');
  await ubicacionEnea(p); await p.fill('#persons', '3'); await accion(p, 'next');
  await elegir(p, 'needs', 'Agua potable'); await accion(p, 'next');
  await elegir(p, 'waiting', 'Más de 24 horas'); await accion(p, 'next');
}
async function rutaUrgente(p, peligro = 'Personas atrapadas') {
  await elegir(p, 'danger', 'Sí'); await accion(p, 'next');
  await elegir(p, 'hazard', peligro);
  await accion(p, 'toggleUnknownNeighborhood');
  await p.fill('#reference', 'frente a la cancha del colegio');
}
async function leerCodigo(p) {
  const codes = await app(p).locator('.code').allInnerTexts();
  return { codigo: codes[0].trim(), clave: codes[1].trim() };
}

// Recorre los flujos legales y devuelve una instantánea por pantalla (texto visible + casillas).
async function recorridoLegal(p, url) {
  const s = [];
  const foto = async (nombre, filtro = () => true) => s.push({ nombre, texto: (await texto(p)).filter(filtro), casillas: await casillas(p) });
  await p.goto(url);
  await p.click('#legalHead'); await foto('privacidad'); await accion(p, 'returnLegal');
  await rutaNormalHastaRevision(p);
  const sinTriaje = (l) => !l.startsWith('Revisión sugerida');
  await foto('revision', sinTriaje);
  for (const _ of [1, 2, 3]) await p.click('#reviewConsent'); // marcar, desmarcar, marcar: historial
  await foto('revision-consentida', sinTriaje);
  await accion(p, 'sendNormal');
  const { codigo, clave } = await leerCodigo(p);
  const sinEstadoEnvio = (l) => !/^(Modo demo|Envío sin confirmar|Esperando confirmación|Recibido y guardado)/.test(l);
  await foto('comprobante', sinEstadoEnvio);
  await accion(p, 'contactFromReceipt'); await foto('permisoContacto');
  await p.click('#contactConsent'); await foto('permisoContacto-marcado');
  await accion(p, 'continueContact'); await foto('contacto');
  await p.fill('#contactAlias', 'Alias Ficticio'); await p.fill('#contactPhone', '3000000000');
  await accion(p, 'saveContact');
  await accion(p, 'showAuth'); await foto('autorizaciones-cerrado');
  await p.fill('#authCode', 'DEMO-ZZZZZ'); await p.fill('#authSecret', '000000'); await accion(p, 'verifyAuth');
  await foto('autorizaciones-error');
  await p.fill('#authCode', codigo); await p.fill('#authSecret', clave); await accion(p, 'verifyAuth');
  await foto('autorizaciones-abierto');
  await accion(p, 'toggleHistory'); await foto('historial');
  await accion(p, 'revokeContact'); await foto('revocado-contacto');
  await accion(p, 'reqCorreccion'); await p.fill('#reqText', 'Corregir la referencia del lugar'); await accion(p, 'sendRequest');
  await foto('solicitud-correccion');
  await accion(p, 'reqSupresion'); await accion(p, 'sendRequest'); await foto('solicitud-supresion');
  await accion(p, 'receipt'); await accion(p, 'showAdditional');
  await accion(p, 'openHealth'); await foto('salud');
  await p.click('#healthConsent'); await foto('salud-marcado');
  await elegir(p, 'healthCats', 'Movilidad reducida'); await foto('salud-elegido');
  await accion(p, 'saveHealth'); await accion(p, 'saveAdditional');
  await accion(p, 'showAuth'); await p.fill('#authCode', codigo); await p.fill('#authSecret', clave); await accion(p, 'verifyAuth');
  await accion(p, 'toggleHistory'); await foto('historial-final');
  await accion(p, 'revokeHealth'); await foto('revocado-salud');
  // Ruta urgente: autorización por art. 10c.
  await p.goto(url); await rutaUrgente(p); await accion(p, 'sendRapid');
  const u = await leerCodigo(p); await foto('comprobante-urgente', sinEstadoEnvio);
  await accion(p, 'showAuth'); await p.fill('#authCode', u.codigo); await p.fill('#authSecret', u.clave); await accion(p, 'verifyAuth');
  await foto('autorizaciones-urgente');
  return s;
}

test('1. flujos legales: data/ se comporta igual que V5 pantalla a pantalla', async ({ page }) => {
  const v5 = await recorridoLegal(page, V5);
  const nuevo = await recorridoLegal(page, DATA);
  const diferencias = [];
  v5.forEach((a, i) => {
    const b = nuevo[i];
    const faltan = a.texto.filter((l) => !b.texto.includes(l)), sobran = b.texto.filter((l) => !a.texto.includes(l));
    if (faltan.length || sobran.length || a.casillas.join() !== b.casillas.join())
      diferencias.push({ pantalla: a.nombre, faltanEnData: faltan, nuevasEnData: sobran, casillasV5: a.casillas, casillasData: b.casillas });
  });
  anotar('flujosLegales', { pantallas: v5.length, diferencias });
  expect(diferencias).toEqual([]);
});

test('2a. cambiar de urgente a normal elimina el peligro residual', async ({ page }) => {
  let cuerpo = null;
  await page.route('**/api/reporte', async (r) => { cuerpo = JSON.parse(r.request().postData()); await r.fulfill({ json: { status: 'en_espera' } }); });
  await page.goto(DATA + '?modo=estacion');
  await elegir(page, 'danger', 'Sí'); await accion(page, 'next'); await elegir(page, 'hazard', 'Personas atrapadas');
  await accion(page, 'back'); await elegir(page, 'danger', 'No'); await accion(page, 'next');
  await ubicacionEnea(page); await page.fill('#persons', '3'); await accion(page, 'next');
  await elegir(page, 'needs', 'Agua potable'); await accion(page, 'next');
  await elegir(page, 'waiting', 'Más de 24 horas'); await accion(page, 'next');
  await page.click('#reviewConsent'); await accion(page, 'sendNormal');
  await expect.poll(() => cuerpo).not.toBeNull();
  const t = cuerpo.triajeSugerido;
  anotar('residualUrgenteANormal', { nivel: t.nivel, reglas: t.reglasActivadas, categoria: cuerpo.categoria, hazardOriginal: t.respuestasOriginales.hazard });
  expect(cuerpo.categoria).toBe('necesidades');
  expect(t.nivel).not.toBe('critico');
  expect(t.respuestasOriginales.hazard, 'el peligro elegido antes debe eliminarse al cambiar de ruta').toBeNull();
});

test('2b. cambiar de normal a urgente elimina respuestas residuales de la ruta normal', async ({ page }) => {
  let cuerpo = null;
  await page.route('**/api/reporte', async (r) => { cuerpo = JSON.parse(r.request().postData()); await r.fulfill({ json: { status: 'en_espera' } }); });
  await page.goto(DATA + '?modo=estacion');
  await elegir(page, 'danger', 'No'); await accion(page, 'next');
  await ubicacionEnea(page); await page.fill('#persons', '4'); await accion(page, 'next');
  await elegir(page, 'needs', 'Alimentos'); await accion(page, 'next');
  await elegir(page, 'waiting', 'No podemos esperar');
  for (const _ of [1, 2, 3]) await accion(page, 'back');
  await elegir(page, 'danger', 'Sí'); await accion(page, 'next'); await elegir(page, 'hazard', 'Derrumbe o deslizamiento');
  await accion(page, 'toggleUnknownNeighborhood'); await page.fill('#reference', 'junto al puente peatonal');
  await accion(page, 'sendRapid');
  await expect.poll(() => cuerpo).not.toBeNull();
  const o = cuerpo.triajeSugerido.respuestasOriginales;
  anotar('residualNormalAUrgente', { nivel: cuerpo.triajeSugerido.nivel, reglas: cuerpo.triajeSugerido.reglasActivadas, originales: o });
  expect(cuerpo.triajeSugerido.nivel).toBe('critico');
  expect(o.needs).toEqual([]); expect(o.waiting).toBeNull(); expect(o.persons).toBe('');
});

async function enviarUrgente(page, responder, { dobleClic = false } = {}) {
  const posts = [];
  await page.route('**/api/reporte', async (r) => { const c = JSON.parse(r.request().postData()); posts.push(c); await responder(r, c, posts.length); });
  await page.goto(DATA + '?modo=estacion');
  await rutaUrgente(page);
  const b = app(page).locator('[data-action="sendRapid"]');
  if (dobleClic) await b.dblclick(); else await b.click();
  return posts;
}

test('3a. 200 {status:en_espera} no se muestra como guardado', async ({ page }) => {
  const posts = await enviarUrgente(page, (r) => r.fulfill({ json: { status: 'en_espera', mensaje: 'Endpoint listo' } }));
  await expect(app(page)).toContainText('Envío sin confirmar');
  await expect(app(page)).not.toContainText('Recibido y guardado');
  expect(posts).toHaveLength(1);
  expect(JSON.stringify(posts[0])).not.toMatch(/hashClave|privateKey/);
});

test('3b. guardado solo con acuse válido y mismo idReporte', async ({ page }) => {
  await enviarUrgente(page, (r, c) => r.fulfill({ status: 201, json: { estado: 'guardado', idReporte: c.idReporte, recibidoEnEstacion: true } }));
  await expect(app(page)).toContainText('Recibido y guardado en la estación');
  const textoReceipt = await app(page).innerText();
  anotar('contradiccionComprobanteGuardado', textoReceipt.includes('No se ha confirmado recepción por un servidor real.'));
});

test('3c. acuse con otro idReporte queda sin confirmar', async ({ page }) => {
  await enviarUrgente(page, (r) => r.fulfill({ json: { estado: 'guardado', idReporte: 'OTRO', recibidoEnEstacion: true } }));
  await expect(app(page)).toContainText('Envío sin confirmar');
});

test('3d. doble clic produce un solo POST', async ({ page }) => {
  const posts = await enviarUrgente(page, async (r) => { await new Promise((x) => setTimeout(x, 700)); await r.fulfill({ json: { status: 'en_espera' } }); }, { dobleClic: true });
  await expect(app(page)).toContainText('Envío sin confirmar');
  await page.waitForTimeout(500);
  expect(posts).toHaveLength(1);
});

test('3e. corte de red tras POST y reintento con el mismo ID y cuerpo', async ({ page }) => {
  const posts = await enviarUrgente(page, (r, c, n) => n === 1 ? r.abort('connectionreset') : r.fulfill({ json: { estado: 'guardado', idReporte: c.idReporte, recibidoEnEstacion: true } }));
  await expect(app(page)).toContainText('Envío sin confirmar');
  await accion(page, 'retry');
  await expect(app(page)).toContainText('Recibido y guardado en la estación');
  expect(posts).toHaveLength(2);
  expect(posts[1]).toEqual(posts[0]);
});

test('3f. respuesta no JSON queda sin confirmar', async ({ page }) => {
  await enviarUrgente(page, (r) => r.fulfill({ status: 200, contentType: 'text/html', body: '<h1>Portal</h1>' }));
  await expect(app(page)).toContainText('Envío sin confirmar');
});

test('3g. sin crypto.subtle el aviso se crea y no envía hash', async ({ page }) => {
  await page.addInitScript(() => { try { Object.defineProperty(globalThis.crypto, 'subtle', { get: () => undefined }); } catch {} });
  const posts = await enviarUrgente(page, (r) => r.fulfill({ json: { status: 'en_espera' } }));
  await expect(app(page)).toContainText('Envío sin confirmar');
  expect(posts).toHaveLength(1);
  expect(posts[0].hashClave).toBeUndefined();
});

test('4. offline: ninguna petición sale del origen local', async ({ page }) => {
  const externas = [];
  await page.context().route('**/*', (r) => { const u = new URL(r.request().url()); if (u.host !== '127.0.0.1:8765') { externas.push(u.href); return r.abort(); } return r.continue(); });
  await page.route('**/api/reporte', (r) => r.fulfill({ json: { status: 'en_espera' } }));
  await page.goto(DATA + '?modo=estacion');
  await rutaNormalHastaRevision(page); await page.click('#reviewConsent'); await accion(page, 'sendNormal');
  await page.click('#legalHead');
  anotar('peticionesExternas', externas);
  expect(externas).toEqual([]);
  expect(await page.evaluate(() => window.Lugares?.length ?? null)).toBe(514);
});

test('5. accesibilidad axe (WCAG 2.2 AA) en V5 y data/', async ({ page }) => {
  const tags = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];
  const revisar = async () => (await new AxeBuilder({ page }).include('#app').withTags(tags).analyze()).violations.map((v) => `${v.id}(${v.impact})x${v.nodes.length}`);
  const recorrer = async (url) => {
    const r = {};
    await page.goto(url); r.inicio = await revisar();
    await elegir(page, 'danger', 'No'); await accion(page, 'next'); r.ubicacion = await revisar();
    await ubicacionEnea(page); await page.fill('#persons', '2'); await accion(page, 'next'); r.necesidades = await revisar();
    await elegir(page, 'needs', 'Alimentos'); await accion(page, 'next'); r.espera = await revisar();
    await elegir(page, 'waiting', 'No sé'); await accion(page, 'next'); r.revision = await revisar();
    await page.click('#reviewConsent'); await accion(page, 'sendNormal'); r.comprobante = await revisar();
    const { codigo, clave } = await leerCodigo(page);
    await accion(page, 'contactFromReceipt'); r.permisoContacto = await revisar();
    await page.click('#contactConsent'); await accion(page, 'continueContact'); r.contacto = await revisar();
    await accion(page, 'saveContact'); await accion(page, 'showAuth'); r.autorizaciones = await revisar();
    await page.fill('#authCode', codigo); await page.fill('#authSecret', clave); await accion(page, 'verifyAuth'); r.autorizacionesAbierto = await revisar();
    await page.click('#legalHead'); r.privacidad = await revisar();
    await page.goto(url); await elegir(page, 'danger', 'Sí'); await accion(page, 'next'); r.urgencia = await revisar();
    return r;
  };
  const v5 = await recorrer(V5), nuevo = await recorrer(DATA);
  anotar('axe', { v5, data: nuevo });
  const nuevas = Object.keys(nuevo).flatMap((k) => nuevo[k].filter((x) => !v5[k].map((y) => y.split('(')[0]).includes(x.split('(')[0])).map((x) => `${k}:${x}`));
  anotar('axeRegresiones', nuevas);
  expect(nuevas, 'data/ no debe añadir violaciones que V5 no tenía').toEqual([]);
});

test('6. teclado y foco: el foco va al encabezado tras cambiar de pantalla', async ({ page }) => {
  await page.goto(DATA);
  await page.keyboard.press('Tab');
  let primeros = [];
  for (let i = 0; i < 6; i++) { primeros.push(await page.evaluate(() => document.activeElement?.textContent?.trim().slice(0, 30))); await page.keyboard.press('Tab'); }
  await elegir(page, 'danger', 'No'); await accion(page, 'next');
  const foco = await page.evaluate(() => ({ tag: document.activeElement?.tagName, texto: document.activeElement?.textContent?.slice(0, 40) }));
  anotar('foco', { ordenTab: primeros, trasContinuar: foco });
  expect(foco.tag).toBe('H2');
});

for (const ancho of [360, 390]) {
  test(`7. sin scroll horizontal a ${ancho}px`, async ({ page }) => {
    await page.setViewportSize({ width: ancho, height: 780 });
    const medir = async (n) => ({ n, ok: await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth + 1) });
    const r = [];
    await page.goto(DATA); r.push(await medir('inicio'));
    await elegir(page, 'danger', 'No'); await accion(page, 'next'); await page.fill('#barrio', 'la'); r.push(await medir('ubicacion+menu'));
    await page.click('#legalHead'); r.push(await medir('privacidad'));
    await page.goto(DATA); await rutaUrgente(page); r.push(await medir('urgencia'));
    await accion(page, 'sendRapid'); r.push(await medir('comprobante'));
    anotar(`scroll${ancho}`, r);
    expect(r.filter((x) => !x.ok)).toEqual([]);
  });
}

test('8. buscador: tildes ignoradas y «No sé en qué barrio estoy»', async ({ page }) => {
  await page.goto(DATA); await elegir(page, 'danger', 'No'); await accion(page, 'next');
  const buscar = async (q) => { await page.fill('#barrio', q); return app(page).locator('.suggestion b').allInnerTexts(); };
  const r = { villamaria: await buscar('villamaria'), exito: await buscar('exito fundadores'), chipre: await buscar('CHIPRE') };
  anotar('buscador', r);
  expect(r.chipre).toContain('Chipre');
  expect(r.exito.length).toBeGreaterThan(0);
  await expect(app(page).locator('[data-action="toggleUnknownNeighborhood"]')).toContainText('No sé en qué barrio estoy');
  // Con el menú abierto, el botón queda tapado por las sugerencias (igual en V5): se cierra primero.
  const tapado = await page.evaluate(() => { const b = document.querySelector('[data-action="toggleUnknownNeighborhood"]').getBoundingClientRect(); const e = document.elementFromPoint(b.x + b.width / 2, b.y + 10); return !!e?.closest('#ubicaciones'); });
  anotar('menuTapaBotonNoSe', tapado);
  await page.keyboard.press('Escape');
  await accion(page, 'toggleUnknownNeighborhood'); await page.fill('#reference', 'aqui'); await page.fill('#persons', '1'); await accion(page, 'next');
  await expect(page.locator('#inlineMessage')).toContainText('referencia reconocible');
});

test('9. diseño: data/ es idéntico a V5 píxel a píxel en pantallas sin cambios funcionales', async ({ page }) => {
  const capturas = async (url) => {
    const c = {};
    const foto = async (n) => { await page.mouse.move(0, 0); c[n] = await page.locator('.page').screenshot({ animations: 'disabled', caret: 'hide' }); };
    await page.goto(url); await foto('inicio');
    await elegir(page, 'danger', 'No'); await foto('inicio-elegido');
    await accion(page, 'next'); await foto('ubicacion');
    await ubicacionEnea(page); await page.fill('#persons', '2'); await accion(page, 'next'); await foto('necesidades');
    await elegir(page, 'needs', 'Alimentos'); await accion(page, 'next'); await elegir(page, 'waiting', 'Más de 24 horas'); await foto('espera');
    await page.click('#legalHead'); await foto('privacidad');
    await page.goto(url); await elegir(page, 'danger', 'Sí'); await accion(page, 'next'); await elegir(page, 'hazard', 'Personas atrapadas'); await foto('urgencia');
    await accion(page, 'toggleUnknownNeighborhood'); await foto('urgencia-no-se');
    return c;
  };
  const a = await capturas(V5), b = await capturas(DATA);
  const distintas = Object.keys(a).filter((k) => !a[k].equals(b[k]));
  for (const k of distintas) { writeFileSync(`${DIR_SALIDA}diseno-${k}-v5.png`, a[k]); writeFileSync(`${DIR_SALIDA}diseno-${k}-data.png`, b[k]); }
  anotar('disenoDistinto', distintas);
  expect(distintas).toEqual([]);
});

test('10. aviso urgente crítico no se bloquea por datos opcionales inválidos', async ({ page }) => {
  let cuerpo = null;
  await page.route('**/api/reporte', async (r) => { cuerpo = JSON.parse(r.request().postData()); await r.fulfill({ json: { status: 'en_espera' } }); });
  await page.goto(DATA + '?modo=estacion');
  await rutaUrgente(page); await page.fill('#rapidPersons', '0');
  await accion(page, 'sendRapid');
  await expect.poll(() => cuerpo, { timeout: 5000 }).not.toBeNull();
  anotar('urgenteConPersonasInvalidas', { nivel: cuerpo.triajeSugerido.nivel, personas: cuerpo.personas, reglas: cuerpo.triajeSugerido.reglasActivadas });
  expect(cuerpo.triajeSugerido.nivel).toBe('critico');
  expect(cuerpo.personas).toBeNull();
  expect(cuerpo.triajeSugerido.reglasActivadas).toContain('V3');
});

test('11. con el menú de sugerencias abierto, «No sé en qué barrio estoy» sigue accesible al tocarlo', async ({ page }) => {
  await page.goto(DATA); await elegir(page, 'danger', 'No'); await accion(page, 'next');
  await page.fill('#barrio', 'la');
  // Se prueban borde superior, centro y borde inferior: un toque en cualquiera debe llegar al botón.
  const tapados = await page.evaluate(() => {
    const b = document.querySelector('[data-action="toggleUnknownNeighborhood"]').getBoundingClientRect();
    return [8, b.height / 2, b.height - 8].map((dy) => !!document.elementFromPoint(b.x + b.width / 2, b.y + dy)?.closest('#ubicaciones'));
  });
  anotar('menuTapaBotonNoSe', tapados);
  expect(tapados).toEqual([false, false, false]);
});
