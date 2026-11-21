#!/usr/bin/env python3
"""
Puente local offline-first entre la ESP32-S3 (ROL_CONCENTRADOR por USB)
y el tablero web del PMU (pmu.html) sin modificar el frontend.

Funciones:
  1. Sirve pmu.html y config.js en http://localhost:8000 (evita el bloqueo 403 de OSM en file://).
  2. Emula localmente los streams SSE /conecta/bus.json y /conecta/torres.json que espera pmu.html.
  3. Lee las lineas JSON que imprime la ESP32-S3 Concentrador por USB (115200 baud)
     y las traduce en tiempo real a eventos 'tower.heartbeat' y 'report.new' en el mapa del PMU.

Uso:
  python pmu/puente_usb_pmu.py                # Autodetecta el puerto COM de la ESP32-S3
  python pmu/puente_usb_pmu.py --port COM5    # Usa un puerto COM especifico
  python pmu/puente_usb_pmu.py --simular      # Prueba sin placa conectada
"""

from __future__ import annotations

import argparse
import json
import os
import queue
import sys
import threading
import time
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

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


class EstadoPuente:
    """Almacenamiento en memoria y difusion SSE hacia todas las pestanas del PMU."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._bus: dict[str, dict] = {}
        self._torres: dict[str, dict] = {}
        self._bus_subs: list[queue.Queue[str]] = []
        self._torres_subs: list[queue.Queue[str]] = []
        self._seq = 0

    def _next_key(self, prefix: str = "m") -> str:
        self._seq += 1
        return f"{prefix}_{self._seq:06d}"

    def snapshot_bus(self) -> dict[str, dict]:
        with self._lock:
            return dict(self._bus)

    def snapshot_torres(self) -> dict[str, dict]:
        with self._lock:
            return dict(self._torres)

    def suscribir(self, canal: str) -> queue.Queue[str]:
        q: queue.Queue[str] = queue.Queue(maxsize=128)
        with self._lock:
            if canal == "bus":
                self._bus_subs.append(q)
            else:
                self._torres_subs.append(q)
        return q

    def desuscribir(self, canal: str, q: queue.Queue[str]) -> None:
        with self._lock:
            lista = self._bus_subs if canal == "bus" else self._torres_subs
            if q in lista:
                lista.remove(q)

    def _broadcast(self, lista: list[queue.Queue[str]], payload_sse: str) -> None:
        for q in list(lista):
            try:
                q.put_nowait(payload_sse)
            except queue.Full:
                pass

    def publicar_bus(self, mensaje: dict) -> str:
        with self._lock:
            key = self._next_key("msg")
            self._bus[key] = mensaje
            sse = f"event: put\ndata: {json.dumps({'path': '/' + key, 'data': mensaje}, ensure_ascii=False)}\n\n"
            self._broadcast(self._bus_subs, sse)
            return key

    def publicar_torre(self, tower_id: str, datos: dict) -> None:
        with self._lock:
            self._torres[tower_id] = datos
            sse = f"event: put\ndata: {json.dumps({'path': '/' + tower_id, 'data': datos}, ensure_ascii=False)}\n\n"
            self._broadcast(self._torres_subs, sse)

    def limpiar(self) -> None:
        with self._lock:
            self._bus.clear()
            self._torres.clear()
            sse = 'event: put\ndata: {"path":"/","data":{}}\n\n'
            self._broadcast(self._bus_subs, sse)
            self._broadcast(self._torres_subs, sse)


ESTADO = EstadoPuente()


def decodificar_necesidades(mask: int) -> list[str]:
    etiquetas = [
        (1 << 0, "Agua potable"),
        (1 << 1, "Alimentos"),
        (1 << 2, "Refugio o alojamiento"),
        (1 << 3, "Apoyo en salud"),
    ]
    return [texto for bit, texto in etiquetas if mask & bit]


def decodificar_ubicacion(barrio_id: int, referencia: str, torre: str) -> dict:
    if 1 <= barrio_id <= 346:
        return {"id": f"mz-{barrio_id:04d}"}
    if 1001 <= barrio_id <= 1168:
        return {"id": f"poi-mz-{barrio_id - 1000:04d}"}
    if referencia:
        return {"texto": referencia}
    return {"texto": f"Zona de la torre {torre}"}


def procesar_paquete_esp(raw: dict) -> None:
    """Traduce una linea JSON de ROL_CONCENTRADOR al contrato del PMU."""
    ahora_ms = int(time.time() * 1000)
    torre = str(raw.get("tower") or "E01").strip() or "E01"
    tipo = raw.get("tipo")

    # Siempre refresca el latido real de la torre cuando llega trafico valido desde esa estacion
    latido = {
        "tower": torre,
        "source": "esp",
        "at": ahora_ms,
        "bateriaPct": raw.get("bateriaPct", 100),
    }
    ESTADO.publicar_torre(torre, latido)

    if tipo in (4, 2, "latido", "telemetria"):
        print(f"[PMU <- ESP] Latido torre={torre} bat={latido['bateriaPct']}%", flush=True)
        return

    if tipo in (1, "reporte"):
        cat_raw = raw.get("categoria", 6)
        categoria = cat_raw if isinstance(cat_raw, str) else CAT_MAP.get(int(cat_raw), "necesidades")

        barrio_id = int(raw.get("barrioId") or 0)
        urgencia = int(raw.get("urgencia") or 2)
        afectados = int(raw.get("afectados") or 1)
        mask = int(raw.get("necesidadesMask") or 0)
        codigo = int(raw.get("codigo") or 0)
        referencia = str(raw.get("referencia") or "").strip()

        es_urgente = (urgencia >= 3) or (categoria in CATS_URGENTES)
        espera = (
            "No podemos esperar"
            if urgencia >= 3
            else ("Hoy" if urgencia == 2 else "Dentro de las próximas 24 horas")
        )

        id_reporte = f"MZL-{codigo:03d}" if codigo > 0 else f"{torre}-{ahora_ms % 100000}"
        payload = {
            "idReporte": id_reporte,
            "categoria": categoria,
            "ruta": "urgente" if es_urgente else "normal",
            "torre": torre,
            "ubicacion": decodificar_ubicacion(barrio_id, referencia, torre),
            "personas": afectados,
            "necesidades": decodificar_necesidades(mask),
            "prioridad": {"espera": espera},
            "referencia": referencia,
            "fecha": datetime.now(timezone.utc).isoformat(),
        }
        mensaje = {
            "mid": f"esp-{id_reporte}-{ahora_ms}",
            "type": "report.new",
            "at": ahora_ms,
            "payload": payload,
        }
        ESTADO.publicar_bus(mensaje)
        print(
            f"[PMU <- ESP] REPORTE {id_reporte} | torre={torre} | cat={categoria} | barrioId={barrio_id} | pers={afectados}",
            flush=True,
        )


class PMUHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, format: str, *args) -> None:
        # Silencia logs ruidosos de cada archivo/peticion HTTP
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

    def _serve_sse(self, canal: str) -> None:
        self.send_response(HTTPStatus.OK)
        self._cors_headers()
        self.send_header("Content-Type", "text/event-stream; charset=utf-8")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        q = ESTADO.suscribir(canal)
        try:
            inicial = ESTADO.snapshot_bus() if canal == "bus" else ESTADO.snapshot_torres()
            primer_evento = f"event: put\ndata: {json.dumps({'path': '/', 'data': inicial}, ensure_ascii=False)}\n\n"
            self.wfile.write(primer_evento.encode("utf-8"))
            self.wfile.flush()

            while True:
                try:
                    evento = q.get(timeout=15.0)
                    self.wfile.write(evento.encode("utf-8"))
                except queue.Empty:
                    self.wfile.write(b": keepalive\n\n")
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, OSError):
            pass
        finally:
            ESTADO.desuscribir(canal, q)

    def do_OPTIONS(self) -> None:
        self.send_response(HTTPStatus.NO_CONTENT)
        self._cors_headers()
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self) -> None:
        path = urlparse(self.path).path

        if path == "/conecta/bus.json":
            self._serve_sse("bus")
            return
        if path == "/conecta/torres.json":
            self._serve_sse("torres")
            return

        # Servir archivos estaticos del PMU
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
        length = int(self.headers.get("Content-Length", "0") or "0")
        body = self.rfile.read(length) if length > 0 else b"{}"

        if path == "/conecta/bus.json":
            try:
                data = json.loads(body.decode("utf-8"))
            except json.JSONDecodeError:
                self._send_bytes(400, "application/json", b'{"error":"json_invalido"}')
                return
            key = ESTADO.publicar_bus(data)
            resp = json.dumps({"name": key}).encode("utf-8")
            self._send_bytes(200, "application/json", resp)
            return

        self._send_bytes(404, "application/json", b'{"error":"ruta_no_encontrada"}')

    def do_PUT(self) -> None:
        path = urlparse(self.path).path
        length = int(self.headers.get("Content-Length", "0") or "0")
        body = self.rfile.read(length) if length > 0 else b"{}"

        if path.startswith("/conecta/torres/") and path.endswith(".json"):
            tower_id = path[len("/conecta/torres/") : -len(".json")]
            try:
                data = json.loads(body.decode("utf-8"))
            except json.JSONDecodeError:
                self._send_bytes(400, "application/json", b'{"error":"json_invalido"}')
                return
            ESTADO.publicar_torre(tower_id, data)
            self._send_bytes(200, "application/json", json.dumps(data).encode("utf-8"))
            return

        self._send_bytes(404, "application/json", b'{"error":"ruta_no_encontrada"}')

    def do_DELETE(self) -> None:
        path = urlparse(self.path).path
        if path == "/conecta.json":
            ESTADO.limpiar()
            print("[PMU] Datos locales de la demo limpiados.", flush=True)
            self._send_bytes(200, "application/json", b"null")
            return
        self._send_bytes(404, "application/json", b'{"error":"ruta_no_encontrada"}')


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
            "        (o prueba sin placa usando: python pmu/puente_usb_pmu.py --simular)",
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
    """Inyecta un latido de E01 cada 5 s y un reporte de prueba a los 4 s para validar sin placa."""
    print("[SIM] Modo simulacion activo (sin USB). Enviando trafico de prueba al PMU...", flush=True)
    time.sleep(2)
    procesar_paquete_esp({"source": "esp", "tower": "E01", "tipo": 4, "bateriaPct": 98})
    time.sleep(2)
    procesar_paquete_esp(
        {
            "source": "esp",
            "tower": "E01",
            "tipo": 1,
            "barrioId": 6,  # mz-0006 = Chipre
            "categoria": 2,  # derrumbe
            "urgencia": 3,
            "afectados": 4,
            "necesidadesMask": 3,  # Agua + Alimentos
            "bateriaPct": 97,
            "codigo": 42,
            "referencia": "Prueba desde puente USB local",
        }
    )
    while True:
        time.sleep(5)
        procesar_paquete_esp({"source": "esp", "tower": "E01", "tipo": 4, "bateriaPct": 96})


def main() -> None:
    parser = argparse.ArgumentParser(description="Puente local USB (ESP32-S3 Concentrador) <-> PMU Web")
    parser.add_argument("--port", default=None, help="Puerto serie del Concentrador (ej. COM5)")
    parser.add_argument("--baud", type=int, default=115200, help="Velocidad serial (default: 115200)")
    parser.add_argument("--http-port", type=int, default=8000, help="Puerto HTTP local (default: 8000)")
    parser.add_argument("--simular", action="store_true", help="Simula latidos y un reporte sin ESP32")
    args = parser.parse_args()

    if args.simular:
        t = threading.Thread(target=hilo_simulador, daemon=True)
    else:
        t = threading.Thread(target=hilo_lector_usb, args=(args.port, args.baud), daemon=True)
    t.start()

    server = ThreadingHTTPServer(("127.0.0.1", args.http_port), PMUHandler)
    print(f"[OK] Tablero PMU disponible en: http://localhost:{args.http_port}/", flush=True)
    print("     Presiona Ctrl+C para detener.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[OK] Puente detenido.", flush=True)
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
