from __future__ import annotations

import argparse
import json
import struct
import zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
HTML = (ROOT / "web" / "control-panel.html").read_bytes()
STATE: dict[str, dict[str, Any]] = {
    "pan": {"pos": 0, "busy": False, "min": -1024, "center": 0, "max": 1024, "reversed": False},
    "tilt": {"pos": 0, "busy": False, "min": -256, "center": 0, "max": 256, "reversed": False},
}
NETWORK: dict[str, Any] = {
    "connected": False,
    "configured": False,
    "mode": "ap_sta",
    "ssid": "",
    "hostname": "moss-preview",
    "ip": "192.168.4.1",
    "rssi": 0,
    "ap_active": True,
    "ap_ssid": "MOSS-Setup-DEMO",
}
CAMERA: dict[str, Any] = {"enabled": False}


class Handler(BaseHTTPRequestHandler):
    def send_json(self, status: int, payload: object) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def read_json(self) -> dict[str, Any]:
        length = int(self.headers.get("Content-Length", "0"))
        return json.loads(self.rfile.read(length) or b"{}")

    def do_GET(self) -> None:
        if self.path in ("/", "/device"):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(HTML)))
            self.end_headers()
            self.wfile.write(HTML)
            return
        if self.path == "/api/v1/capabilities":
            self.send_json(200, {
                "motor": True, "network": True, "camera": True,
                "oled": True, "led": True, "audio": True, "system": True,
                "power_config": False, "agent": False,
                "automation": False, "ota": False,
            })
            return
        if self.path == "/api/v1/motor/status":
            self.send_json(200, STATE)
            return
        if self.path == "/api/v1/motor/config":
            self.send_json(200, {"version": 1, **STATE})
            return
        if self.path == "/api/v1/status":
            self.send_json(200, {
                "version": "preview", "uptime": 67320,
                "memory": {"free": 246784, "psram": 0},
                "wifi": {"rssi": NETWORK["rssi"]},
                "motor": {
                    "pan": {"pos": STATE["pan"]["pos"], "busy": False},
                    "tilt": {"pos": STATE["tilt"]["pos"], "busy": False},
                },
                "camera": CAMERA,
                "led": {"0": {"effect": "static", "color": "0071E3"}, "1": {"effect": "static", "color": "0071E3"}},
                "oled": {"rotation": 0},
                "battery": {"voltage": 3.91, "percent": 78, "charging": False},
            })
            return
        if self.path == "/api/v1/network/status":
            self.send_json(200, NETWORK)
            return
        if self.path == "/api/v1/network/scan":
            self.send_json(200, {"networks": [
                {"ssid": "Apartment-WiFi", "rssi": -43, "secure": True},
                {"ssid": "MOSS-Lab", "rssi": -61, "secure": True},
                {"ssid": "Guest", "rssi": -74, "secure": False},
            ]})
            return
        if self.path == "/api/v1/audio/mic/test":
            self.send_json(200, {"ok": True, "mic": True, "level": 37})
            return
        if self.path.startswith("/api/v1/camera/capture"):
            width, height = 640, 360
            rows = bytearray()
            for y in range(height):
                rows.append(0)
                for x in range(width):
                    block = ((x // 80) + (y // 60)) % 2
                    cross = abs(x - width // 2) < 2 or abs(y - height // 2) < 2
                    color = (52, 105, 73) if cross else ((28, 38, 32) if block else (12, 17, 14))
                    rows.extend(color)
            def chunk(kind: bytes, data: bytes) -> bytes:
                return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
            png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(bytes(rows), 6)) + chunk(b"IEND", b"")
            self.send_response(200)
            self.send_header("Content-Type", "image/png")
            self.send_header("Content-Length", str(len(png)))
            self.end_headers()
            self.wfile.write(png)
            return
        self.send_json(404, {"error": "not found"})

    def do_POST(self) -> None:
        try:
            payload = self.read_json()
            axis_index = payload.get("motor", 0)
            axis = "pan" if axis_index in (0, "pan") else "tilt" if axis_index in (1, "tilt") else None

            if self.path == "/api/v1/motor/stop":
                self.send_json(200, {"stopped": True})
                return
            if self.path == "/api/v1/network/config":
                ssid = str(payload.get("ssid", ""))
                hostname = str(payload.get("hostname", ""))
                password = str(payload.get("password", ""))
                if not ssid or not hostname or (password and len(password) < 8):
                    self.send_json(400, {"error": "invalid SSID, password, or hostname"})
                    return
                NETWORK.update({"configured": True, "ssid": ssid, "hostname": hostname})
                self.send_json(200, {"saved": True, "restarting": True})
                return
            if self.path == "/api/v1/network/forget":
                NETWORK.update({"connected": False, "configured": False, "ssid": ""})
                self.send_json(200, {"forgotten": True, "restarting": True})
                return
            if self.path == "/api/v1/camera/config":
                CAMERA["enabled"] = bool(payload.get("enabled", False))
                self.send_json(200, CAMERA)
                return
            if self.path == "/api/v1/oled/text" or self.path == "/api/v1/oled/clear" or self.path == "/api/v1/oled/rotation":
                self.send_json(200, {"ok": True})
                return
            if self.path == "/api/v1/led/effect":
                self.send_json(200, payload)
                return
            if self.path == "/api/v1/audio/speaker/test":
                self.send_json(200, {"ok": True, "speaker": True})
                return
            if self.path == "/api/v1/control/selfcheck":
                self.send_json(200, {"motor": "ok", "camera": "ok", "led": "ok", "oled": "ok", "memory": 246784})
                return
            if self.path == "/api/v1/control/reboot":
                self.send_json(200, {"status": "rebooting"})
                return
            if axis is None:
                self.send_json(400, {"error": "invalid motor"})
                return

            state = STATE[axis]
            if self.path == "/api/v1/motor/absolute":
                target = int(payload.get("target", state["pos"]))
                if target < state["min"] or target > state["max"]:
                    self.send_json(400, {"error": "target outside soft limits"})
                    return
                state["pos"] = target
                self.send_json(200, {"motor": axis_index, "busy": True})
                return
            if self.path == "/api/v1/motor/relative":
                target = state["pos"] + int(payload.get("steps", 0))
                if target < state["min"] or target > state["max"]:
                    self.send_json(400, {"error": "target outside soft limits"})
                    return
                state["pos"] = target
                self.send_json(200, {"motor": axis_index, "busy": True})
                return
            if self.path == "/api/v1/motor/limit/capture":
                limit = payload.get("limit")
                if limit == "center":
                    state["pos"] = state["center"]
                elif limit in ("min", "max"):
                    proposed = dict(state)
                    proposed[limit] = state["pos"]
                    if not proposed["min"] <= proposed["center"] <= proposed["max"]:
                        self.send_json(400, {"error": "captured position makes limits invalid"})
                        return
                    state[limit] = state["pos"]
                else:
                    self.send_json(400, {"error": "invalid limit"})
                    return
                self.send_json(200, {"version": 1, **STATE})
                return
            if self.path == "/api/v1/motor/config":
                proposed = {
                    "min": int(payload.get("min", state["min"])),
                    "center": int(payload.get("center", state["center"])),
                    "max": int(payload.get("max", state["max"])),
                }
                if not proposed["min"] <= proposed["center"] <= proposed["max"]:
                    self.send_json(400, {"error": "invalid limits"})
                    return
                state.update(proposed)
                state["reversed"] = bool(payload.get("reversed", state["reversed"]))
                self.send_json(200, {"version": 1, **STATE})
                return
            self.send_json(404, {"error": "not found"})
        except (ValueError, TypeError, json.JSONDecodeError) as error:
            self.send_json(400, {"error": str(error)})

    def log_message(self, format: str, *args: object) -> None:
        return


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Preview the MOSS control panel")
    parser.add_argument("--port", type=int, default=8766)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"MOSS control panel preview: http://127.0.0.1:{args.port}")
    server.serve_forever()
