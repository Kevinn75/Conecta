# V5: manifiesto de requisitos inalterables y registro de problemas (Fase 0)

Fuente: `docs/referencia/CONECTA_redisenado_experiencia_v5.html` (SHA-256 `9d151532a5b6b2f472e81cb867894664aeaa9a8f7fd5cc93e7947e6c5fb808a0`, 132 217 bytes, 233 líneas).
**V5 usa un puente de demostración (`BroadcastChannel` + `Promise.resolve`) y emite comprobantes locales sin confirmación del servidor.**

## 1. Inventario

- **Pantallas (`SCREEN_META`, 20):** inicio, urgencia, ubicacion, apoyo, grupo, momento, necesidades, primero, motivo, espera, revision, comprobante, detalles, sinwifi, adicionales, privacidad, permisoContacto, contacto, salud, autorizaciones.
- **Ruta normal (`route()`):** inicio → ubicacion → necesidades → espera → revision → comprobante. **Ruta urgente:** inicio → urgencia → comprobante (sin consentimiento previo: Ley 1581, art. 10c).
- **Pantallas legales (`LEGAL_SCREENS`):** privacidad, permisoContacto, contacto, salud, autorizaciones (CSS previo vía `.legalFreeze`).
- **Estado:** `initialState()` (46 campos); `reportId` y `privateKey` se generan en `issueReceipt()`.
- **`API` (líneas 131-137):** `enviarReporte`, `consultarEstado`, `verificarClave`, `registrarConsentimiento`, `enviarSolicitudTitular`; todas simuladas.
- **Payloads:** `buildPayload()` (incluye `hashClave`, `consents`, contacto/salud solo con autorización), `buildLoraPayload()` (sin contacto, salud ni texto libre).
- **Catálogo:** `PLACES` (346) + `EXTRA_PLACES` (168) = **514** lugares, IDs únicos `mz-NNNN` / `poi-mz-NNNN`, 114 zonas, 13 tipos, 26 con `aliases`; 513 en Manizales y 1 en Villamaría.
- **Utilidades:** `esc()`, `norm()` (sin tildes), `searchScore()`, `filterPlaces()`, combobox accesible con flechas/Enter/Escape, `sha256()`, `randomCode()`.

## 2. Inalterable (criterios de aceptación a–l del prompt maestro)

1. Textos legales literales: 130 fragmentos en `tests/web/base_legal.json`, `CONSENT_VERSION = 'v4-2026-11-20'`, historial, revocación, plazos (10/15 días hábiles), derechos y datos de la SIC. Verificar con `node tests/web/verificar_legal.mjs data/`.
2. Avisos de demostración («MODO DEMO · SIN ENVÍO REAL», «Comprobante de prueba», «No confirma recepción…») se conservan: mientras `[Entidad responsable]` siga sin completar, **el modo de uso real queda bloqueado**.
3. Los 514 lugares con `id`, `nombre`, `zona`, `tipo`, `municipio` y `aliases`; búsqueda sin tildes; botón «No sé en qué barrio estoy» con referencia obligatoria (≥ 6 caracteres).
4. Ruta urgente de tres pasos; ruta normal corta; respuestas sin valores precargados; «No sé» válido.
5. Diseño reconocible de V5 (colores, tipografía, componentes `choice`, `btn`, `unknownLocationChoice`).

## 3. Registro de problemas de V5

| ID | Sev. | Problema | Tratamiento previsto |
| --- | --- | --- | --- |
| V5-01 | Crítico | `API.enviarReporte` devuelve `{ok:true}` sin servidor; el estado PMU llega por `BroadcastChannel`. | Adaptador HTTP real a `POST /api/reporte` con estados «sin confirmar» / «guardado en estación» / «confirmado por PMU»; el modo demo queda explícito. |
| V5-02 | Crítico | `hashClave` = SHA-256 de código + PIN de 6 dígitos: reversible por fuerza bruta (10⁶). En `http://192.168.4.1` Safari no ofrece `crypto.subtle` → `null`. | No enviar `hashClave` como protección; documentar bloqueo de contexto seguro y acordar verificación en servidor. |
| V5-03 | Alto | ID creado al final en `issueReceipt()`; sin clave de idempotencia, timeout ni bloqueo de doble clic. | ID por intento creado antes del primer `POST`, conservado en memoria para reintentos. |
| V5-04 | Alto | `triageSuggestion()` usa «Por verificar» como nivel. | Motor `crítico/alto/medio` + `requiereVerificacion` transversal. |
| V5-05 | Alto | `verificarClave` compara en el navegador. | Solo demo; ruta de verificación pendiente del backend. |
| V5-06 | Medio | Pantallas legales: texto 11–12 px, gris `#81968b` (~3:1), inputs de 15 px (zoom en iOS), `label` sin `for` (`authCode`, `authSecret`, `select` de ayuda). | Autorizado: solo atributos invisibles (`for`/`id`, `role`). Contraste y tamaños: hallazgo pendiente de autorización. |
| V5-07 | Medio | `render()` reemplaza el DOM y pierde el foco; `inform()` no siempre usa `role="alert"`. | Gestión de foco al cambiar de pantalla y errores anunciados. |
| V5-08 | Bajo | Texto «514 referencias de Manizales y Villamaría», pero solo 1 lugar es de Villamaría. | Solo registrar; no cambiar texto. |
| V5-09 | Bajo | Pantallas apoyo/grupo/momento/primero/motivo fuera de las rutas principales (accesibles desde «adicionales»). | Conservar. |
| V5-10 | Info | Tamaño 132 217 bytes (gzip 29 216); catálogo 62 133. Formato compacto de los mismos 514 registros: 23 932. | Objetivo < 100 KB medido; reportar si no se cumple. |
