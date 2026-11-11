"""Check browser-confirmed assembly loading; optionally save a screenshot."""
import argparse
import json
from urllib.request import urlopen


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", type=int, nargs="?", default=3939)
    parser.add_argument("--screenshot", help="Absolute path to save the current browser view")
    args = parser.parse_args()
    with urlopen(f"http://127.0.0.1:{args.port}/health", timeout=5) as response:
        health = json.load(response)
    print(json.dumps(health, indent=2))
    if not health["ready"]:
        raise SystemExit("Assembly is not yet confirmed in every connected browser tab")
    if args.screenshot:
        from ocp_vscode import save_screenshot, set_port
        set_port(args.port)
        save_screenshot(args.screenshot, timeout_s=10)


if __name__ == "__main__":
    main()
