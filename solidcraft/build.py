#!/usr/bin/env python3
"""Build the experimental SolidCraft assembly from the current CAD sources."""

import argparse
from itertools import combinations
import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT / "cadpy"))

from build123d import Compound, export_step, export_stl
import clip_holder
import pantilt
from scene import build


def command(action, **params):
    return {"command": action, "params": params}


def run_script(cli, directory, commands, *, source=None, target=None):
    script = directory / "commands.json"
    script.write_text(json.dumps({"commands": commands}), encoding="utf-8")
    argv = [cli, "run", str(script), "--results"]
    if source:
        argv += ["--in", str(source)]
    if target:
        argv += ["--save", str(target)]
    return json.loads(subprocess.check_output(argv, text=True))


def assembly_commands(components, lower_mesh):
    by_name = {o["name"]: o["id"] for c in components for o in c["occurrences"]}
    grip_component = next(c for c in components if c["name"] == "grip")
    grip = json.loads((HERE / "grip.json").read_text(encoding="utf-8"))["commands"]
    # The first new body in an assembly need not be named Body1. Use the
    # extrude's body_name instead of the standalone script's rename.
    grip = grip[:-1]
    grip[-1]["params"]["body_name"] = "grip"
    transform = grip_component["occurrences"][0]["transform"]
    placement = [list(row) for row in zip(*transform)][:3]
    commands = [
        command("component.unlock", component=next(c["id"] for c in components if c["parent"] == 0)),
        command("component.activate", component=grip_component["id"]),
        command("solid.remove", bodies=["grip"]),
        *grip,
        command("component.activate", component="root"),
        command("solid.move", bodies=["grip"], matrix=placement, name="Place grip"),
    ]

    pan = ("carrier", "servo_pan", "plate_horn", "plate_shaft")
    tilt = ("platform", "servo_tilt", "body", "grip", "phone", "plug", "cable")
    moving = {by_name[f"{key}:1"] for key in (*pan, *tilt)}
    for c in components:
        for occurrence in c["occurrences"]:
            if occurrence["id"] not in moving and not occurrence["grounded"]:
                commands.append(command("occurrence.ground", occurrence=occurrence["id"], grounded=True))

    pan_origin = [pantilt.BASE_L / 2, pantilt.BASE_W / 2, pantilt.RISER]
    tilt_z = pantilt.RISER + pantilt.ROOF_GAP + 9.5 - 2 + pantilt.BOLT_DZ
    tilt_origin = [32.5 + pantilt.PIVOT_DX, pan_origin[1], tilt_z]
    for name, a, b, point, axis, limits in (
        ("Pan", "base", "carrier", pan_origin, [0, 0, 1], [[0, 180]]),
        ("Tilt", "carrier", "platform", tilt_origin, [0, 1, 0], [[-92, 10]]),
    ):
        origin = dict(point=point, z=axis, x=[1, 0, 0])
        commands.append(command(
            "joint.create", name=name, type="revolute", flip=True, angle=0,
            a=dict(occurrence=by_name[f"{a}:1"], **origin),
            b=dict(occurrence=by_name[f"{b}:1"], **origin), limits=limits,
        ))
    for group in (pan, tilt):
        for member in group[1:]:
            commands.append(command(
                "joint.as_built", type="rigid", name=f"Mount {member}",
                a=by_name[f"{group[0]}:1"], b=by_name[f"{member}:1"], quiet=True,
            ))
    if lower_mesh:
        commands.extend([
            command("file.insert_mesh", path=str(lower_mesh), name="box_lower (mesh)", units="mm"),
            command("occurrence.ground", occurrence="box_lower (mesh):1", grounded=True),
        ])
    return commands


def verify(cli, work, model, baseline):
    tracked = ("base", "carrier", "platform", "grip", "phone")
    collision_bodies = {"base": "base", "platform": "platform", "grip": "grip",
                        "holder": "body", "phone": "phone", "cable": "cable"}
    results = []
    commands = [command("joint.list"), command("inspect.measure", names_only=True)]
    poses = [(90, 0), (-2, 0), (45, 45), (100, 180)]
    for tilt, pan in poses:
        commands.extend([
            command("joint.drive", joint="Pan", value=pan, quiet=True),
            command("joint.drive", joint="Tilt", value=tilt - 90, quiet=True),
            command("inspect.measure", bodies=list(tracked)),
            command("inspect.interference", bodies=list(collision_bodies)),
        ])
    commands.extend([
        command("parameters.change", name="phone_width", expression="80 mm"),
        command("inspect.measure", bodies=["grip"]),
        command("inspect.interference", bodies=["grip", "holder"]),
    ])
    replies = run_script(cli, work, commands, source=model)
    joints, inventory = replies[:2]
    if joints["errors"] or joints["conflicts"] or joints["floating"]:
        raise RuntimeError("SolidCraft assembly has joint errors or unconnected components")
    if len(inventory["bodies"]) != len(baseline):
        raise RuntimeError(f"Expected {len(baseline)} bodies in the finished assembly")
    for index, (tilt, pan) in enumerate(poses):
        pan_reply, tilt_reply, measures, interference = replies[2 + 4 * index:6 + 4 * index]
        if pan_reply["values"][0]["value"] != pan or tilt_reply["values"][0]["value"] != tilt - 90:
            raise RuntimeError(f"Joints did not reach tilt={tilt}, pan={pan}")
        reference = baseline if index == 0 else pantilt.assemble(tilt, pan, cable=True, box=False)
        max_error = 0.0
        for body in measures["bodies"]:
            shape = reference[body["name"]]
            bounds = shape.bounding_box()
            for bound in ("min", "max"):
                max_error = max(max_error, *(abs(a - b) for a, b in zip(body["bbox"][bound], getattr(bounds, bound))))
            if not body["closed"] or abs(body["volume_mm3"] / shape.volume - 1) > 0.001:
                raise RuntimeError(f"Unexpected volume for {body['name']} at tilt={tilt}, pan={pan}")
        if max_error > 0.05:
            raise RuntimeError(f"Pose differs from build123d at tilt={tilt}, pan={pan}: {max_error:.6f} mm")
        if interference["unchecked"] or interference["truncated"]:
            raise RuntimeError(f"Incomplete SolidCraft collision check at tilt={tilt}, pan={pan}")
        solidcraft_overlaps = {
            tuple(sorted((item["a"], item["b"]))): item["volume_mm3"]
            for item in interference["interferences"] if item["volume_mm3"] > 0.01
        }
        reference_overlaps = []
        for a, b in combinations(collision_bodies, 2):
            shape_a, shape_b = (reference[collision_bodies[name]] for name in (a, b))
            common = shape_a.intersect(shape_b)
            volume = 0.0 if common is None else (
                common.volume if hasattr(common, "volume") else sum(shape.volume for shape in common)
            )
            pair = tuple(sorted((a, b)))
            if abs(volume - solidcraft_overlaps.pop(pair, 0.0)) > 0.02:
                raise RuntimeError(f"Collision volume differs for {pair} at tilt={tilt}, pan={pan}")
            if volume > 0.01:
                reference_overlaps.append({"a": a, "b": b, "volume_mm3": volume})
        if solidcraft_overlaps:
            raise RuntimeError(f"Unexpected collision pairs at tilt={tilt}, pan={pan}: {solidcraft_overlaps}")
        results.append({"tilt": tilt, "pan": pan, "max_bbox_error_mm": max_error,
                        "interference": interference, "build123d_overlaps": reference_overlaps})

    variant = replies[-2]["bodies"][0]
    wall = clip_holder.GRIP_WALL
    try:
        clip_holder.GRIP_WALL = (clip_holder.GRIP_W - 80 - 2 * clip_holder.FIT_SIDE) / 2
        expected_volume = clip_holder.grip().volume
    finally:
        clip_holder.GRIP_WALL = wall
    fit = replies[-1]
    if not variant["closed"] or abs(variant["volume_mm3"] - expected_volume) > 0.01:
        raise RuntimeError("Grip did not rebuild correctly at phone_width=80 mm")
    if fit["unchecked"] or fit["truncated"] or fit["count"]:
        raise RuntimeError("Grip and holder did not pass the interference check at phone_width=80 mm")
    return {"body_count": len(inventory["bodies"]), "poses": results,
            "grip_80_mm": {"volume_mm3": variant["volume_mm3"], "fit": fit}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", default="solidcraft-cli")
    parser.add_argument("--out", type=Path, default=HERE / "out")
    args = parser.parse_args()
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with TemporaryDirectory(prefix="androsaur-solidcraft-") as temp:
        work = Path(temp)
        step = work / "androsaur-stand.step"
        imported = work / "import.sc"
        parts = pantilt.assemble(tilt_deg=90, pan_deg=0, cable=True)
        export_step(Compound(children=build(parts, glass=False), label="androsaur-stand"), step)
        subprocess.run([args.cli, "run", "--in", step, "--save", imported, "--quiet"], check=True)
        inventory, measures = run_script(args.cli, work, [
            command("component.list", compact=True),
            command("inspect.measure", names_only=True),
        ], source=imported)
        missing = len(parts) - len(measures["bodies"])
        names = {body["name"] for body in measures["bodies"]}
        lower_mesh = None
        if missing == 1 and "box_lower" not in names:
            lower_mesh = work / "box_lower.stl"
            export_stl(parts["box_lower"], lower_mesh, tolerance=0.01, angular_tolerance=0.1)
            print("box_lower: STEP import omitted the solid; inserting its display mesh.", flush=True)
        elif missing or "box_lower" not in names:
            raise RuntimeError(f"Unexpected STEP import: {len(measures['bodies'])}/{len(parts)} bodies")
        commands = assembly_commands(inventory["components"], lower_mesh)
        model = work / "androsaur-stand.sc"
        run_script(args.cli, work, commands, source=imported, target=model)
        report = verify(args.cli, work, model, parts)
        (output / "androsaur-stand.sc").write_bytes(model.read_bytes())
        subprocess.run([args.cli, "run", HERE / "grip.json", "--save", output / "grip.sc", "--quiet"], check=True)
        (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"Saved the assembly, native grip, and verification report to {output}")


if __name__ == "__main__":
    main()
