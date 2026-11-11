"""OCP viewer with model replay for late, refreshed and additional browser tabs."""
import argparse
import threading
from http import HTTPStatus

import orjson
from ocp_viewer.server.pages import _response, missing_assets, respond
from ocp_viewer.server.sockets import handle
from ocp_viewer.server.viewer import Viewer
from ocp_viewer_core.logo import logo
from ocp_viewer_core.state import add_port, del_port
from websockets.exceptions import ConnectionClosed
from websockets.sync.server import serve


GROUPS = ("stand", "holder", "enclosure", "electronics", "fasteners")


class ModelReplay:
    def __init__(self):
        self.lock = threading.RLock()
        self.payload = None
        self.generation = 0
        self.clients = set()

    def publish(self, viewer, payload):
        with self.lock:
            self.payload = payload
            self.generation += 1
            viewer.splash = False
            for client in tuple(self.clients):
                client.deliver()
        print("Model cached; waiting for browser confirmation", flush=True)

    def health(self):
        with self.lock:
            clients = [c.confirmed == self.generation for c in self.clients]
            return {
                "model_cached": self.payload is not None,
                "generation": self.generation,
                "browser_count": len(clients),
                "confirmed_browsers": sum(clients) if self.payload else 0,
                "ready": bool(self.payload and clients and all(clients)),
            }


class BrowserConnection:
    """Keep upstream protocol handling; replay only after the page reports its tree.

    Registration alone is too early: the page still draws its splash on load.
    A state notification proves that rendering has been initialized.
    """
    def __init__(self, ws, viewer, replay):
        self.ws, self.viewer, self.replay = ws, viewer, replay
        self.initialized = False
        self.sent = self.confirmed = 0

    def __getattr__(self, name):
        return getattr(self.ws, name)

    def deliver(self):
        replay = self.replay
        if self.initialized and replay.payload and self.sent != replay.generation:
            try:
                self.ws.send(replay.payload)
                self.sent = replay.generation
            except ConnectionClosed:
                replay.clients.discard(self)

    def __iter__(self):
        for raw in self.ws:
            message = raw.decode("utf-8") if isinstance(raw, bytes) else raw
            kind, payload = message[0], message[2:]
            if kind == "D":
                self.replay.publish(self.viewer, payload)
                continue
            yield raw
            if kind == "U":
                update = orjson.loads(payload)
                states = update.get("text", {}).get("states", {}) if update.get("command") == "status" else {}
                if states:
                    with self.replay.lock:
                        self.initialized = True
                        self.replay.clients.add(self)
                        loaded = all(any(p.startswith(f"/Group/{g}/") for p in states) for g in GROUPS)
                        if loaded and self.sent == self.replay.generation and self.sent:
                            if self.confirmed != self.sent:
                                print(f"Browser confirmed assembly generation {self.sent}", flush=True)
                            self.confirmed = self.sent
                        self.deliver()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=3939)
    args = parser.parse_args()
    missing = missing_assets()
    if missing:
        parser.error(f"OCP viewer assets missing: {missing}")
    viewer = Viewer({"host": "127.0.0.1", "port": args.port})
    viewer.backend.load_model(logo)
    replay = ModelReplay()

    def connection(ws):
        client = BrowserConnection(ws, viewer, replay)
        try:
            handle(viewer, client)
        finally:
            with replay.lock:
                replay.clients.discard(client)

    def page(_connection, request):
        if request.path == "/health":
            return _response(HTTPStatus.OK, orjson.dumps(replay.health()), "application/json")
        return respond(viewer, request)

    with serve(connection, "127.0.0.1", args.port, process_request=page,
               compression=None, max_size=None) as server:
        add_port(args.port)
        print(f"Viewer: http://127.0.0.1:{args.port}/viewer", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            del_port(args.port)


if __name__ == "__main__":
    main()
