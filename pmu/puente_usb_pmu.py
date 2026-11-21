#!/usr/bin/env python3
"""
Servidor local offline-first CONECTA (Contrato Backend v7) + Puente USB ESP32-S3.

Funciones:
  1. Sirve pmu.html y config.js en http://localhost:8000/ (evita el bloqueo 403 de OSM en file://).
  2. Implementa el Contrato Backend v7 (CONTRATO_BACKEND.md):
       - POST   /api/mensajes            (201) guarda y difunde por /api/stream
       - GET    /api/mensajes            (200) historial en orden de llegada (?sin_torres=1)
       - GET    /api/stream              SSE con 'data: {json}\\n\\n' y ': ping\\n\\n' cada 20 s
       - PUT    /api/torres/{id}/latido  (200) recibe latido real de ESP32/simulador por HTTP
       - DELETE /api/mensajes            (200) limpia datos de demo
  3. Lee en paralelo la ESP32-S3 Concentrador conectada por USB (115200 baudios)
     y traduce cada paquete ESP-NOW recibido al formato exacto del PMU v7.
  4. Permite conectar al mismo tiempo ESP32 reales (por USB/ESP-NOW o por HTTP en red local)
     y simuladores.

Uso:
  python pmu/puente_usb_pmu.py                # Autodetecta puerto COM de la ESP32-S3
  python pmu/puente_usb_pmu.py --port COM5    # Usa un puerto COM especifico
  python pmu/puente_usb_pmu.py --sin-usb      # Solo servidor HTTP/SSE (sin leer COM)
  python pmu/puente_usb_pmu.py --simular      # Prueba con latidos y reporte simulados
"""

from __future__ import annotations

import argparse
import json
import queue
import threading
import time
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlparse

BASE_DIR = Path(__file__).resolve().parent

# Mapa de CategoriaEmergencia (Protocol.h) -> claves CATS de pmu.html
CAT_MAP = {
    1: "atrapadas",
    2: "derrumbe",
    3: "terremoto",
    4: "atrapadas",
    5: "otro",
    6: "necesidades",
    7: "terremoto",
    8: "volcanica",
    9: "donaciones",
    10: "inventarios",
    11: "entregas",
}

CATS_URGENTES = {"atrapadas", "derrumbe", "terremoto", "volcanica", "otro"}


class EstadoBackendV7:
    """Almacenamiento en memoria y difusion SSE conforme al Contrato Backend v7."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._mensajes: list[dict] = []
        self._ultimos_latidos: dict[str, dict] = {}
        self._seen_mids: set[str] = set()
        self._reportes_activos: set[str] = set()
        self._subs: list[queue.Queue[str]] = []
        self._seq = 0
        self._metricas = {
            "paquetesRecibidos": 0,
            "incidentesUnicos": 0,
            "actualizacionesConsenso": 0,
            "duplicadosDescartados": 0,
        }

    def existe_reporte(self, id_reporte: str) -> bool:
        with self._lock:
            return id_reporte in self._reportes_activos

    def registrar_metricas_esp(self, duplicado: bool = False, consenso: bool = False, nuevo: bool = False) -> None:
        with self._lock:
            self._metricas["paquetesRecibidos"] += 1
            if duplicado:
                self._metricas["duplicadosDescartados"] += 1
            if consenso:
                self._metricas["actualizacionesConsenso"] += 1
            if nuevo:
                self._metricas["incidentesUnicos"] += 1

    def obtener_metricas(self) -> dict:
        with self._lock:
            m = dict(self._metricas)
            redundantes = m["actualizacionesConsenso"] + m["duplicadosDescartados"]
            m["tasaConsolidacionPct"] = round(
                100.0 * redundantes / redundantes, 1
            ) if redundantes > 0 else 100.0
            return m

    def _next_mid(self, prefix: str = "srv") -> str:
        self._seq += 1
        return f"{prefix}-{self._seq}-{int(time.time() * 1000)}"

    def suscribir(self) -> queue.Queue[str]:
        q: queue.Queue[str] = queue.Queue(maxsize=256)
        with self._lock:
            self._subs.append(q)
        return q

    def desuscribir(self, q: queue.Queue[str]) -> None:
        with self._lock:
            if q in self._subs:
                self._subs.remove(q)

    def _broadcast_locked(self, mensaje: dict) -> None:
        # Contrato v7: "Cada mensaje nuevo se envia como data: {json}\n\n, sin campo event."
        trama = f"data: {json.dumps(mensaje, ensure_ascii=False)}\n\n"
        for q in list(self._subs):
            try:
                q.put_nowait(trama)
            except queue.Full:
                pass

    def _aplicar_habeas_data_locked(self, mensaje: dict) -> None:
        """Reglas Ley 1581 del Contrato v7 (§5)."""
        # 1. Nunca conservar clave privada en claro
        payload = mensaje.get("payload")
        if isinstance(payload, dict):
            payload.pop("clavePrivada", None)

        # 2. Si se retira autorizacion de contacto o salud, borrar ese dato del historial
        if mensaje.get("type") == "report.consent":
            rid = mensaje.get("id")
            consent = mensaje.get("consent") or {}
            tipo_c = consent.get("tipo")
            if rid and consent.get("otorgada") is False and tipo_c in ("contacto", "salud"):
                for prev in self._mensajes:
                    p = prev.get("payload")
                    if isinstance(p, dict) and p.get("idReporte") == rid:
                        p.pop(tipo_c, None)
                        aut = p.get("autorizaciones")
                        if isinstance(aut, dict):
                            aut[tipo_c] = False

    def registrar_mensaje(self, mensaje: dict) -> dict:
        with self._lock:
            msg = dict(mensaje)
            if not msg.get("at"):
                msg["at"] = int(time.time() * 1000)
            if not msg.get("mid"):
                msg["mid"] = self._next_mid("msg")

            mid = str(msg["mid"])
            if mid in self._seen_mids:
                return msg
            self._seen_mids.add(mid)

            self._aplicar_habeas_data_locked(msg)

            if msg.get("type") == "tower.heartbeat":
                tower = str(msg.get("tower") or "").upper()
                if tower:
                    self._ultimos_latidos[tower] = msg
            else:
                self._mensajes.append(msg)
                p = msg.get("payload")
                if isinstance(p, dict) and p.get("idReporte"):
                    self._reportes_activos.add(str(p["idReporte"]))

            self._broadcast_locked(msg)
            return msg

    def es_mid_duplicado(self, mid: str) -> bool:
        with self._lock:
            return mid in self._seen_mids

    def registrar_latido(self, tower_id: str, source: str = "esp", extra: dict | None = None) -> dict:
        ahora_ms = int(time.time() * 1000)
        tower = str(tower_id).strip().upper() or "E01"
        msg: dict = {
            "type": "tower.heartbeat",
            "tower": tower,
            "source": source,
            "at": ahora_ms,
        }
        if extra:
            for k in ("bateriaPct", "mac"):
                if k in extra:
                    msg[k] = extra[k]
        with self._lock:
            self._ultimos_latidos[tower] = msg
            self._broadcast_locked(msg)
        return msg

    def obtener_historial(self, sin_torres: bool = False) -> list[dict]:
        with self._lock:
            if sin_torres:
                return list(self._mensajes)
            combinado = list(self._mensajes) + list(self._ultimos_latidos.values())
            combinado.sort(key=lambda m: m.get("at", 0))
            return combinado

    def limpiar(self) -> None:
        with self._lock:
            self._mensajes.clear()
            self._ultimos_latidos.clear()
            self._seen_mids.clear()
            self._reportes_activos.clear()
            for k in self._metricas:
                self._metricas[k] = 0


ESTADO = EstadoBackendV7()


def decodificar_necesidades(mask: int) -> list[str]:
    etiquetas = [
        (1 << 0, "Agua potable"),
        (1 << 1, "Alimentos"),
        (1 << 2, "Refugio o alojamiento"),
        (1 << 3, "Apoyo en salud"),
    ]
    return [texto for bit, texto in etiquetas if mask & bit]


def decodificar_ubicacion(barrio_id: int, ubicacion_txt: str, referencia: str, torre: str) -> dict:
    if 1 <= barrio_id <= 346:
        return {"id": f"mz-{barrio_id:04d}"}
    if 1001 <= barrio_id <= 1168:
        return {"id": f"poi-mz-{barrio_id - 1000:04d}"}
    if ubicacion_txt:
        return {"id": None, "nombre": ubicacion_txt, "texto": ubicacion_txt}
    if referencia:
        return {"id": None, "nombre": referencia, "texto": referencia}
    return {"id": None, "texto": f"Zona de la torre {torre}"}


def decodificar_tiempo(hace_tiempo: str) -> str | None:
    mapa = {
        "10": "Hace aprox. 10 minutos",
        "15": "Hace aprox. 15 minutos",
        "30": "Hace aprox. 30 minutos",
        "mas_60": "Hace más de 1 hora",
    }
    return mapa.get(hace_tiempo)


def procesar_paquete_esp(raw: dict) -> None:
    """Traduce una linea JSON de ROL_CONCENTRADOR (USB) al Contrato v7."""
    ahora_ms = int(time.time() * 1000)
    torre = str(raw.get("tower") or "E01").strip().upper() or "E01"
    tipo = raw.get("tipo")

    # Todo trafico valido desde una ESP32 actualiza su latido real (source: "esp")
    ESTADO.registrar_latido(
        torre,
        source="esp",
        extra={"bateriaPct": raw.get("bateriaPct", 100), "mac": raw.get("mac", "")},
    )

    if tipo in (4, 2, "latido", "telemetria"):
        print(f"[PMU <- ESP] Latido torre={torre} bat={raw.get('bateriaPct', 100)}%", flush=True)
        return

    if tipo in (1, "reporte"):
        cat_raw = raw.get("categoria", 6)
        categoria = cat_raw if isinstance(cat_raw, str) else CAT_MAP.get(int(cat_raw), "necesidades")

        barrio_id = int(raw.get("barrioId") or 0)
        urgencia = int(raw.get("urgencia") or 2)
        afectados_raw = raw.get("afectados")
        afectados_int = int(afectados_raw) if afectados_raw is not None else 0
        # 0 significa "No se cuantas personas son" -> None ("personas s/d" en pmu.html)
        personas = afectados_int if afectados_int > 0 else None

        mask = int(raw.get("necesidadesMask") or 0)
        codigo = int(raw.get("codigo") or 0)
        confirmaciones = max(1, int(raw.get("confirmaciones") or 1))
        ubicacion_txt = str(raw.get("ubicacion") or "").strip()
        referencia = str(raw.get("referencia") or "").strip()
        hace_tiempo = str(raw.get("haceTiempo") or "").strip()

        es_urgente = (urgencia >= 3) or (categoria in CATS_URGENTES)
        espera = (
            "No podemos esperar"
            if urgencia >= 3
            else ("Dentro de las próximas 24 horas" if urgencia == 2 else "Puede esperar más de 24 horas")
        )

        id_reporte = f"MZL-{codigo:03d}" if codigo > 0 else f"{torre}-{ahora_ms % 100000}"
        # mid incluye confirmaciones: reenvio identico se descarta; nueva corroboracion actualiza el incidente
        mid = f"esp-{torre}-{id_reporte}-c{confirmaciones}" if codigo > 0 else f"esp-{id_reporte}-{ahora_ms}"
        if ESTADO.es_mid_duplicado(mid):
            ESTADO.registrar_metricas_esp(duplicado=True)
            print(f"[PMU-DEDUP] Paquete repetido descartado: {id_reporte} (conf={confirmaciones})", flush=True)
            return

        ya_existe = ESTADO.existe_reporte(id_reporte)
        tipo_evento = "report.update" if (ya_existe or confirmaciones > 1) else "report.new"

        payload = {
            "idReporte": id_reporte,
            "torre": torre,
            "ruta": "urgente" if es_urgente else "normal",
            "categoria": categoria,
            "ubicacion": decodificar_ubicacion(barrio_id, ubicacion_txt, referencia, torre),
            "referencia": referencia or None,
            "observado": decodificar_tiempo(hace_tiempo),
            "personas": personas,
            "confirmaciones": confirmaciones,
            "necesidades": decodificar_necesidades(mask),
            "prioridad": {"top": None, "motivo": None, "espera": espera},
            "apoyos": [],
            "autorizaciones": {"solicitud": True, "contacto": False, "salud": False},
            "fecha": datetime.now(timezone.utc).isoformat(),
        }
        mensaje = {
            "mid": mid,
            "type": tipo_evento,
            "at": ahora_ms,
            "payload": payload,
        }
        ESTADO.registrar_mensaje(mensaje)
        ESTADO.registrar_metricas_esp(consenso=ya_existe, nuevo=(not ya_existe))
        etiqueta = "CONSENSO" if ya_existe else "REPORTE"
        print(
            f"[PMU <- ESP] {etiqueta} {id_reporte} | torre={torre} | cat={categoria} | conf={confirmaciones} | urg={urgencia} | pers={personas if personas is not None else 's/d'}",
            flush=True,
        )


class PMUHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format: str, *args) -> None:
        pass

    def _cors_headers(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def _send_bytes(self, code: int, content_type: str, body: bytes, no_cache: bool = False) -> None:
        self.send_response(code)
        self._cors_headers()
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        if no_cache:
            self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _send_json(self, code: int, obj: object) -> None:
        raw = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self._send_bytes(code, "application/json; charset=utf-8", raw, no_cache=True)

    def _read_json_body(self) -> dict | None:
        length = int(self.headers.get("Content-Length", "0") or "0")
        body = self.rfile.read(length) if length > 0 else b"{}"
        try:
            data = json.loads(body.decode("utf-8"))
            return data if isinstance(data, dict) else {}
        except json.JSONDecodeError:
            return None

    def _serve_sse_stream(self) -> None:
        self.send_response(HTTPStatus.OK)
        self._cors_headers()
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        q = ESTADO.suscribir()
        try:
            self.wfile.write(b": connected\n\n")
            self.wfile.flush()

            while True:
                try:
                    evento = q.get(timeout=20.0)
                    self.wfile.write(evento.encode("utf-8"))
                except queue.Empty:
                    # Contrato v7: ": ping" cada 20 s
                    self.wfile.write(b": ping\n\n")
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, OSError):
            pass
        finally:
            ESTADO.desuscribir(q)

    def do_OPTIONS(self) -> None:
        self.send_response(HTTPStatus.NO_CONTENT)
        self._cors_headers()
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        path = parsed.path

        if path == "/api/stream":
            self._serve_sse_stream()
            return

        if path == "/api/mensajes":
            qs = parse_qs(parsed.query)
            sin_torres = qs.get("sin_torres", ["0"])[0] in ("1", "true", "yes")
            self._send_json(HTTPStatus.OK, ESTADO.obtener_historial(sin_torres=sin_torres))
            return

        if path == "/api/metricas":
            self._send_json(HTTPStatus.OK, ESTADO.obtener_metricas())
            return

        # Servir archivos estaticos del PMU (pmu.html, config.js)
        rel = "pmu.html" if path in ("/", "/index.html") else path.lstrip("/")
        target = (BASE_DIR / rel).resolve()
        if not str(target).startswith(str(BASE_DIR)) or not target.is_file():
            self._send_bytes(404, "text/plain; charset=utf-8", b"Archivo no encontrado")
            return

        ext = target.suffix.lower()
        tipos = {
            ".html": "text/html; charset=utf-8",
            ".js": "application/javascript; charset=utf-8",
            ".css": "text/css; charset=utf-8",
            ".json": "application/json; charset=utf-8",
            ".png": "image/png",
            ".svg": "image/svg+xml",
        }
        ctype = tipos.get(ext, "application/octet-stream")
        self._send_bytes(200, ctype, target.read_bytes(), no_cache=(ext in (".html", ".js")))

    def do_POST(self) -> None:
        path = urlparse(self.path).path
        if path == "/api/mensajes":
            data = self._read_json_body()
            if data is None:
                self._send_json(HTTPStatus.BAD_REQUEST, {"error": "json_invalido"})
                return
            guardado = ESTADO.registrar_mensaje(data)
            self._send_json(HTTPStatus.CREATED, {"ok": True, "mid": guardado.get("mid")})
            return

        self._send_json(HTTPStatus.NOT_FOUND, {"error": "ruta_no_encontrada"})

    def do_PUT(self) -> None:
        path = urlparse(self.path).path
        # PUT /api/torres/{id}/latido
        if path.startswith("/api/torres/") and path.endswith("/latido"):
            tower_id = unquote(path[len("/api/torres/") : -len("/latido")]).strip("/")
            data = self._read_json_body()
            if data is None:
                self._send_json(HTTPStatus.BAD_REQUEST, {"error": "json_invalido"})
                return
            source = str(data.get("source") or "esp")
            latido = ESTADO.registrar_latido(tower_id, source=source, extra=data)
            print(f"[PMU <- HTTP] Latido torre={latido['tower']} source={source}", flush=True)
            self._send_json(HTTPStatus.OK, latido)
            return

        self._send_json(HTTPStatus.NOT_FOUND, {"error": "ruta_no_encontrada"})

    def do_DELETE(self) -> None:
        path = urlparse(self.path).path
        if path == "/api/mensajes":
            ESTADO.limpiar()
            print("[PMU] Historial local limpiado (DELETE /api/mensajes).", flush=True)
            self._send_json(HTTPStatus.OK, {"ok": True})
            return

        self._send_json(HTTPStatus.NOT_FOUND, {"error": "ruta_no_encontrada"})


def detectar_puerto_esp32(puerto_manual: str | None) -> str | None:
    import serial.tools.list_ports

    if puerto_manual:
        return puerto_manual

    puertos = list(serial.tools.list_ports.comports())
    if not puertos:
        return None

    # Prioriza VIDs tipicos de ESP32-S3 (Espressif 0x303A, CP210x 0x10C4, CH340 0x1A86)
    vids_esp = {0x303A, 0x10C4, 0x1A86}
    for p in puertos:
        if p.vid in vids_esp:
            return p.device

    if len(puertos) == 1:
        return puertos[0].device

    print("[USB] Se detectaron varios puertos COM; indica cual usar con --port COMx:", flush=True)
    for p in puertos:
        print(f"      - {p.device}: {p.description}", flush=True)
    return None


def hilo_lector_usb(puerto_manual: str | None, baud: int) -> None:
    try:
        import serial  # type: ignore
    except ImportError:
        print(
            "[ERROR] Falta la libreria 'pyserial'. Instalala con:\n"
            "        pip install pyserial\n"
            "        (o usa --sin-usb / --simular si aun no vas a leer puerto COM)",
            flush=True,
        )
        return

    while True:
        puerto = detectar_puerto_esp32(puerto_manual)
        if not puerto:
            print("[USB] Esperando ESP32-S3 Concentrador por USB...", flush=True)
            time.sleep(3)
            continue

        try:
            print(f"[USB] Conectando a {puerto} @ {baud} baudios...", flush=True)
            with serial.Serial(puerto, baudrate=baud, timeout=1.0) as ser:
                print(f"[USB] Conectado a {puerto}. Escuchando paquetes ESP-NOW...", flush=True)
                while True:
                    linea = ser.readline().decode("utf-8", errors="ignore").strip()
                    if not linea:
                        continue
                    if linea.startswith("{") and linea.endswith("}"):
                        try:
                            data = json.loads(linea)
                            procesar_paquete_esp(data)
                        except json.JSONDecodeError:
                            print(f"[ESP RAW] {linea}", flush=True)
                    else:
                        print(f"[ESP LOG] {linea}", flush=True)
        except Exception as exc:
            print(f"[USB] Conexion interrumpida en {puerto}: {exc}. Reintentando en 2 s...", flush=True)
            time.sleep(2)


def hilo_simulador() -> None:
    """Inyecta latidos de E01 y un reporte de prueba para validar el PMU v7 sin placa."""
    print("[SIM] Modo simulacion activo (sin USB). Enviando trafico de prueba al PMU...", flush=True)
    time.sleep(2)
    procesar_paquete_esp({"source": "esp", "tower": "E01", "tipo": 4, "bateriaPct": 98})
    time.sleep(2)
    procesar_paquete_esp(
        {
            "source": "esp",
            "tower": "E01",
            "tipo": 1,
            "barrioId": 6,   # mz-0006 = Chipre
            "categoria": 2,  # derrumbe
            "urgencia": 3,
            "afectados": 4,
            "necesidadesMask": 3,
            "bateriaPct": 97,
            "codigo": 42,
            "referencia": "Prueba desde servidor local v7",
        }
    )
    while True:
        time.sleep(5)
        procesar_paquete_esp({"source": "esp", "tower": "E01", "tipo": 4, "bateriaPct": 96})


def main() -> None:
    parser = argparse.ArgumentParser(description="Servidor local CONECTA v7 + Puente USB ESP32-S3")
    parser.add_argument("--port", default=None, help="Puerto serie del Concentrador (ej. COM5)")
    parser.add_argument("--baud", type=int, default=115200, help="Velocidad serial (default: 115200)")
    parser.add_argument("--host", default="0.0.0.0", help="Interfaz HTTP (default: 0.0.0.0 para aceptar red local)")
    parser.add_argument("--http-port", type=int, default=8000, help="Puerto HTTP local (default: 8000)")
    parser.add_argument("--sin-usb", action="store_true", help="No abre puerto serial; solo recibe por HTTP")
    parser.add_argument("--simular", action="store_true", help="Simula latidos y un reporte sin ESP32")
    args = parser.parse_args()

    if args.simular:
        threading.Thread(target=hilo_simulador, daemon=True).start()
    elif not args.sin_usb:
        threading.Thread(target=hilo_lector_usb, args=(args.port, args.baud), daemon=True).start()

    server = ThreadingHTTPServer((args.host, args.http_port), PMUHandler)
    print(f"[OK] Tablero PMU v7 disponible en: http://localhost:{args.http_port}/", flush=True)
    print(f"[OK] API Contrato v7 escuchando en: http://{args.host}:{args.http_port}/api/", flush=True)
    print("     Presiona Ctrl+C para detener.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[OK] Servidor detenido.", flush=True)
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
