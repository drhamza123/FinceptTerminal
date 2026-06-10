"""macOS bridge — launches alongside FinceptTerminal.app.
Proxies HTTP requests from the Qt app to the VPS.
Qt QNetworkAccessManager has macOS 26 compatibility issues,
so we bypass it by routing through Python's urllib.
"""
import json
import os
import subprocess
import sys
import urllib.request
import urllib.error
from http.server import HTTPServer, BaseHTTPRequestHandler

VPS_HOST = "64.235.61.6"
VPS_PORT = 8155
BRIDGE_PORT = 8158


def vps_request(method, path, body=None):
    url = f"http://{VPS_HOST}:{VPS_PORT}{path}"
    try:
        req = urllib.request.Request(url, method=method,
                                     headers={"Content-Type": "application/json"})
        if body:
            req.data = json.dumps(body).encode()
        r = urllib.request.urlopen(req, timeout=30)
        return json.loads(r.read())
    except urllib.error.HTTPError as e:
        err_body = e.read()
        return json.loads(err_body) if err_body else {"success": False, "error": str(e), "code": e.code}
    except Exception as e:
        return {"success": False, "error": str(e)}


class BridgeHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        result = vps_request("GET", self.path)
        self.send_json(result)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = json.loads(self.rfile.read(length)) if length else None
        result = vps_request("POST", self.path, body)
        self.send_json(result)

    def send_json(self, data):
        resp = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(resp)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(resp)

    def log_message(self, fmt, *args):
        pass


if __name__ == "__main__":
    app_path = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                            "fincept-qt", "build", "macos-release",
                            "FinceptTerminal.app", "Contents", "MacOS", "FinceptTerminal")
    if os.path.exists(app_path):
        print(f"Starting bridge on port {BRIDGE_PORT}...")
        print(f"Starting FinceptTerminal...")
        subprocess.Popen([app_path], env={**os.environ, "FINCEPT_API_URL": f"http://127.0.0.1:{BRIDGE_PORT}"})
        HTTPServer(("127.0.0.1", BRIDGE_PORT), BridgeHandler).serve_forever()
    else:
        print(f"FinceptTerminal not found at {app_path}")
        print("Starting bridge only on port", BRIDGE_PORT)
        HTTPServer(("127.0.0.1", BRIDGE_PORT), BridgeHandler).serve_forever()
