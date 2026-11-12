# Androsaur Stand

**English** | [Русский](README.ru.md)

A motorized stand for an old Android phone: two MG996R servos (pan and tilt), a hollow leg for the wires, a two-piece
phone holder and a 125 × 125 mm electronics enclosure with a speaker, an ESP32, an amplifier and a power supply.
Part of the [Androsaur](https://github.com/androsaurs) project: the stand turns the phone with the companion on its screen toward the person.

The model is parametric, written in **Python / build123d** (BRep, not a mesh) and lives in [`cadpy/`](cadpy/).
The former FreeCAD model was removed from the working tree; it stays in the git history (commit `9d254f3`).

![Assembly](docs/img/assembly.png)

## Status

A working draft for experiments, not a ready-to-print kit. The parts were printed on 2026-11-11 and the electronics assembly is next.

| Part | State |
| --- | --- |
| Pan-tilt mechanism | Assembled and checked for intersections: tilt −2…100°, pan 45–180° |
| Phone holder (back plate + clamp for a Redmi 12: 76.3 × 9.0 mm) | Parametric, the Type-C connector and cable are modeled; the slot has 0.5 mm side and 0.8 mm thickness clearances; printed, the fit is not confirmed yet |
| Enclosure 125 × 125 × 70, two halves | Built and printed; the stand hangs under the lid, the leg comes out through a hole in the lid; the power-jack hole was fixed in the model afterwards (Ø8.0) |
| FD92-1 speaker, shelf, ESP32, amplifier, DAC | Placed; board sizes are estimates except for the overall dimensions |
| Power module | HW-674 buck converter (bought), modeled as a 60 × 38 × 26 mm block |
| Stability | It stands, but with a small margin; ballast is needed ([checks](docs/verification.md)) |
| Electronics schematic | Drawn: [docs/schematic.svg](docs/schematic.svg); the checks before soldering: [docs/electronics-checks.md](docs/electronics-checks.md) |
| Firmware, wiring | BLE setup from an exterior QR label and HTTPS authorization implemented; actuator/audio drivers and wiring remain |

Details and open questions: [docs/open-issues.md](docs/open-issues.md).

## Quick start

```bash
cd cadpy
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
./viewer.sh                       # viewer at http://127.0.0.1:3939/viewer, redraws when view.py is saved
.venv/bin/python check.py -2 45 90 100   # intersection check while tilting
.venv/bin/python export.py        # STEP for FreeCAD and friends -> exports/androsaur-stand-assembly.step
.venv/bin/python export_stl.py    # STL of the mechanism and holder parts for printing -> exports/stl/
.venv/bin/python shots.py         # pictures for the documentation -> docs/img/
```

More: [docs/model-guide.md](docs/model-guide.md) (in Russian).
Running the viewer, automatic loading after a tab refresh: [docs/viewer.md](docs/viewer.md) (in Russian).

## What is where

| Path | What it is |
| --- | --- |
| [`cadpy/pantilt.py`](cadpy/pantilt.py) | The mechanism, base, leg and the `assemble()` function of the whole model |
| [`cadpy/clip_holder.py`](cadpy/clip_holder.py) | The two-piece phone holder, the Type-C plug and cable |
| [`cadpy/enclosure.py`](cadpy/enclosure.py) | The enclosure, shelf, enclosure feet, ventilation, power inlet |
| [`cadpy/speaker.py`](cadpy/speaker.py), [`esp32.py`](cadpy/esp32.py), [`amp.py`](cadpy/amp.py) | Dimensions of the speaker, the ESP32 board, amplifier, DAC and power module |
| [`cadpy/check.py`](cadpy/check.py), [`pancheck.py`](cadpy/pancheck.py) | Intersection checks while tilting and panning |
| [`cadpy/view.py`](cadpy/view.py), [`palette.py`](cadpy/palette.py) | Live view and part colors |
| [`cadpy/export.py`](cadpy/export.py), [`shots.py`](cadpy/shots.py) | STEP export, pictures |
| [`exports/`](exports/) | The assembly STEP and a FreeCAD project with the same geometry (for viewing and exchange); [`exports/stl/`](exports/stl/): STL of the parts for printing |
| [`references/`](references/README.md) | Sources of third-party material (micromet, MG996R, the holder) |
| [`docs/`](docs/) | Documentation |

## Documentation

The documentation is in Russian for now.

- [Design](docs/design.md): mechanism, holder, leg, parameters
- [Enclosure and layout](docs/enclosure.md): dimensions, coordinates, assembly order, printing
- [Fasteners](docs/fasteners.md): what and how many to buy
- [Printing](docs/printing.md): STL per part, orientation, what to print first, settings
- [Electronics](docs/electronics.md): components, power, amplifier, servos, what to look for in a shop
- [Schematic](docs/schematic.svg) and [checks before soldering](docs/electronics-checks.md)
- [Checks](docs/verification.md): clearances, intersections, stability, printability
- [Open questions](docs/open-issues.md)
- [Working with the model](docs/model-guide.md)
- [Coordinate systems](docs/interfaces.md)
- [Part names](docs/part-names.md), [bill of materials](docs/bom.csv)
- [Requirements](docs/requirements.md)

## License

Our own materials are [CC BY-NC-SA 4.0](LICENSE). The mechanism geometry was reconstructed from the micromet sources
(Thingiverse 1941574, CC BY-SA) and keeps their license. The authors' terms for the MG996R model and the phone holder
supplied by the owner are not known. Details next to the sources in `references/`.
