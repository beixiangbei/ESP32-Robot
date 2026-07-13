from __future__ import annotations

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
        if self.path == "/":
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(HTML)))
            self.end_headers()
            self.wfile.write(HTML)
            return
        if self.path == "/api/v1/motor/status":
            self.send_json(200, STATE)
            return
        if self.path == "/api/v1/motor/config":
            self.send_json(200, {"version": 1, **STATE})
            return
        if self.path.startswith("/api/v1/camera/mock.png"):
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
            if axis is None:
                self.send_json(400, {"error": "invalid motor"})
                return

            state = STATE[axis]
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
    server = ThreadingHTTPServer(("127.0.0.1", 8766), Handler)
    print("MOSS control panel preview: http://127.0.0.1:8766")
    server.serve_forever()
