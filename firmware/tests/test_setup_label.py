import importlib.util
import json
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("setup_label", Path(__file__).parents[1] / "tools/setup_label.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class LabelTest(unittest.TestCase):
    def label(self):
        return {"kind": "androsaur-stand", "v": 1, "id": "stand-112233445566",
                "name": "ANDRO_445566", "pop": "a" * 32, "fingerprint": "b" * 64}

    def test_label_keeps_only_required_physical_bootstrap_data(self):
        source = self.label() | {"ownerKey": "must never be exported"}
        parsed = module.valid_label(json.dumps(source))
        self.assertNotIn("ownerKey", parsed)
        self.assertEqual(parsed, self.label())

    def test_rejects_lookalike_device_and_short_or_missing_secret(self):
        for change in ({"name": "ANDRO_000000"}, {"id": "other"}, {"pop": "1234"},
                       {"fingerprint": "short"}, {"kind": "other"}, {"v": 2}):
            with self.subTest(change=change):
                with self.assertRaises(ValueError):
                    module.valid_label(json.dumps(self.label() | change))


if __name__ == "__main__":
    unittest.main()
