"""Exercise the compiled client's HTTP validation against a local fixture."""
import json
import subprocess
import threading
import time
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

CLIENT = Path(__file__).resolve().parent.parent / 'fontes/build/Release/auth_online_test.exe'


class ConnectionTests(unittest.TestCase):
    def validate(self, payload, expected, delay=0, status=200):
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_POST(self):
                self.rfile.read(int(self.headers.get('Content-Length', '0')))
                time.sleep(delay)
                body = json.dumps(payload).encode()
                self.send_response(status)
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        with ThreadingHTTPServer(('127.0.0.1', 0), Handler) as server:
            worker = threading.Thread(target=server.serve_forever, daemon=True)
            worker.start()
            try:
                result = subprocess.run(
                    [str(CLIENT), f'http://127.0.0.1:{server.server_port}/validate', expected],
                    input='fixture-only\n', text=True, capture_output=True, timeout=20)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            finally:
                server.shutdown()
                worker.join()

    def test_slow_valid_response(self):
        self.validate({'valid': True}, 'OK', delay=1.5)

    def test_service_unavailable_is_network_failure(self):
        self.validate({'valid': False, 'reason': 'service_unavailable'}, 'NET_ERROR', status=503)

    def test_revoked_key_is_denied(self):
        self.validate({'valid': False, 'reason': 'revoked'}, 'BAD_KEY')

    def test_expired_key_is_denied(self):
        self.validate({'valid': False, 'reason': 'expired'}, 'EXPIRED')

    def test_malformed_response_is_network_failure(self):
        self.validate({'unexpected': True}, 'NET_ERROR')


if __name__ == '__main__':
    unittest.main()
