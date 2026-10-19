"""Local-only browser bridge to the persistent HIMERA C++ simulator.

Requires only Python 3.9+ standard library, CMake-built himera_sim binary.
Run: python visual/server.py
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from threading import Lock
from urllib.parse import urlsplit
import webbrowser

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "visual" / "web"
ALLOWED = re.compile(
    r"(?:RESET|STEP|GUNSHOT|SPOT|LOST|DOOR|HAZARD|SAFE [01]|PLAYER \d{1,2} \d{1,2}|"
    r"SET (?:health|exposure|stress|ammo|securityHealth|securityAmmo) (?:\d{1,3}(?:\.\d{1,4})?|\.\d{1,4}))\Z"
)


def resolve_binary() -> Path:
    """Find the engine built by either MSYS2 Ninja or Visual Studio.

    The launcher sets HIMERA_SIM_BINARY explicitly, so Python never has to
    guess which CMake build directory contains the executable.
    """
    explicit = os.environ.get("HIMERA_SIM_BINARY", "").strip()
    if explicit:
        requested = Path(explicit).expanduser().resolve()
        if requested.is_file():
            return requested
        raise FileNotFoundError(
            f"HIMERA_SIM_BINARY points to a missing executable: {requested}"
        )

    candidates = (
        ROOT / "build-vscode" / "himera_sim.exe",
        ROOT / "build-vscode" / "Release" / "himera_sim.exe",
        ROOT / "build" / "Release" / "himera_sim.exe",
        ROOT / "build" / "himera_sim.exe",
        ROOT / "build" / "himera_sim",
        ROOT / "build" / "Debug" / "himera_sim.exe",
    )
    for filename in candidates:
        if filename.is_file():
            return filename
    searched = "\n  ".join(str(path) for path in candidates)
    raise FileNotFoundError(
        "C++ simulator executable not found.\n"
        f"Resolved project directory: {ROOT}\n"
        f"Searched:\n  {searched}\n"
        "Verify the build with: Get-ChildItem .\\build-vscode -Recurse "
        "-Filter himera_sim.exe"
    )


class Core:
    def __init__(self) -> None:
        binary = resolve_binary()
        self.proc = subprocess.Popen(
            [str(binary)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, bufsize=1, cwd=ROOT,
        )
        self.lock = Lock()
        self.state = self._read()
        print(f"C++ engine: {binary}", flush=True)

    def _read(self) -> dict:
        assert self.proc.stdout is not None
        line = self.proc.stdout.readline()
        if not line:
            raise RuntimeError(f"C++ simulator stopped unexpectedly (exit={self.proc.poll()}).")
        return json.loads(line)

    def command(self, instruction: str) -> dict:
        if not ALLOWED.fullmatch(instruction):
            return {"ok": False, "error": "Unsupported or invalid simulator command."}
        with self.lock:
            if self.proc.poll() is not None:
                return {"ok": False, "error": "C++ simulator has stopped. Restart the server."}
            assert self.proc.stdin is not None
            self.proc.stdin.write(instruction + "\n")
            self.proc.stdin.flush()
            result = self._read()
            if result.get("ok"):
                self.state = result
            return result

    def current(self) -> dict:
        with self.lock:
            return self.state

    def close(self) -> None:
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.proc.kill()


class Handler(SimpleHTTPRequestHandler):
    core: Core

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB), **kwargs)

    def _json(self, data: dict, status: int = 200) -> None:
        payload = json.dumps(data, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self) -> None:
        if urlsplit(self.path).path == "/api/state":
            self._json(self.core.current())
        else:
            super().do_GET()

    def do_POST(self) -> None:
        if urlsplit(self.path).path != "/api/action":
            self._json({"ok": False, "error": "Unknown endpoint"}, 404)
            return
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not (0 < size <= 1024):
                raise ValueError("Request size must be 1–1024 bytes")
            payload = json.loads(self.rfile.read(size))
            if not isinstance(payload, dict) or not isinstance(payload.get("command"), str):
                raise ValueError("Expected JSON object with string command")
            self._json(self.core.command(payload["command"]))
        except (ValueError, json.JSONDecodeError) as exc:
            self._json({"ok": False, "error": str(exc)}, 400)
        except Exception as exc:
            self._json({"ok": False, "error": str(exc)}, 500)

    def log_message(self, format, *args):
        if not self.path.startswith("/api/"):
            super().log_message(format, *args)


def main() -> int:
    parser = argparse.ArgumentParser(description="HIMERA C++ AI Visual Simulator")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("--port must be between 1024 and 65535")
    try:
        core = Core()
    except (OSError, RuntimeError) as exc:
        print(f"Cannot start visual simulator: {exc}", file=sys.stderr)
        return 1
    Handler.core = core
    try:
        server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
        server.daemon_threads = True
    except OSError as exc:
        core.close()
        print(f"Cannot open local port {args.port}: {exc}", file=sys.stderr)
        return 1
    url = f"http://127.0.0.1:{args.port}"
    print(f"\nHIMERA VISUAL SIMULATOR — {url}\nPress Ctrl+C to stop.\n", flush=True)
    if not args.no_browser:
        webbrowser.open(url)
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        print("\nStopping HIMERA simulator...")
    finally:
        server.server_close()
        core.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
