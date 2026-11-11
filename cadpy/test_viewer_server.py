"""Regression checks for model replay without constructing CAD geometry."""
import json
import unittest
from types import SimpleNamespace

from viewer_server import BrowserConnection, GROUPS, ModelReplay, hide_splash


class Socket:
    def __init__(self):
        self.incoming = []
        self.sent = []

    def __iter__(self):
        return iter(self.incoming)

    def send(self, message):
        self.sent.append(message)


class ReplayTests(unittest.TestCase):
    def setUp(self):
        self.replay = ModelReplay()
        self.viewer = SimpleNamespace(splash=True)

    def client(self):
        return BrowserConnection(Socket(), self.viewer, self.replay)

    def states(self, client, assembly=False):
        states = {f"/Group/{g}/part": [1, 1] for g in GROUPS} if assembly else {"/Group/OCP": [1, 1]}
        client.ws.incoming = ["U:" + json.dumps({"command": "status", "text": {"states": states}})]
        list(client)  # Upstream handles the yielded message before iteration resumes.

    def test_late_browser_waits_for_splash_then_receives_cached_model(self):
        self.replay.publish(self.viewer, "assembly")
        client = self.client()
        client.deliver()
        self.assertEqual(client.ws.sent, [])
        self.states(client)
        self.assertEqual(client.ws.sent, ["assembly"])
        self.assertFalse(self.replay.health()["ready"])
        self.states(client, assembly=True)
        self.assertTrue(self.replay.health()["ready"])

    def test_refreshed_and_additional_tabs_receive_model(self):
        self.replay.publish(self.viewer, "assembly")
        for _ in range(3):
            client = self.client()
            self.states(client)
            self.assertEqual(client.ws.sent, ["assembly"])
            self.states(client, assembly=True)
        self.assertEqual(self.replay.health()["confirmed_browsers"], 3)

    def test_new_model_requires_new_browser_confirmation(self):
        client = self.client()
        self.states(client)
        self.replay.publish(self.viewer, "first")
        self.states(client, assembly=True)
        self.replay.publish(self.viewer, "second")
        self.assertEqual(client.ws.sent, ["first", "second"])
        self.assertFalse(self.replay.health()["ready"])
        self.states(client, assembly=True)
        self.assertTrue(self.replay.health()["ready"])

    def test_model_is_sent_immediately_when_page_registers(self):
        self.replay.publish(self.viewer, "assembly")
        client = self.client()
        client.ws.incoming = ["L:{}"]
        list(client)
        self.assertIn(client, self.replay.clients)
        self.assertEqual(client.ws.sent, ["assembly"])

    def test_state_after_register_does_not_resend(self):
        self.replay.publish(self.viewer, "assembly")
        client = self.client()
        client.ws.incoming = ["L:{}"]
        list(client)
        self.states(client)
        self.assertEqual(client.ws.sent, ["assembly"])

    def test_cache_without_browser_is_not_ready(self):
        self.replay.publish(self.viewer, "assembly")
        self.assertFalse(self.replay.health()["ready"])


class SplashTests(unittest.TestCase):
    def test_splash_is_hidden_until_model_message(self):
        page = hide_splash(b"<html><head><title>x</title></head><body></body></html>").decode()
        self.assertIn("#cad_viewer{visibility:hidden}", page)
        self.assertLess(page.index("visibility:hidden"), page.index("</head>"))
        self.assertIn("setTimeout(reveal, 15000)", page)
        self.assertEqual(page.count("</head>"), 1)


if __name__ == "__main__":
    unittest.main()
