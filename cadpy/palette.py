"""Colours, opacity and grouping of the parts: ONE place used by the viewer (view.py), the STEP export (export.py) and the pictures (shots.py).

Groups (tree nodes in the viewer, assemblies in the STEP):
    stand        mechanism: base, carrier, side plates, servos, platform
    holder       phone holder: body, grip, phone, plug, cable
    enclosure    two box halves, shelf, box feet
    electronics  speaker, ESP32, amplifier, DAC, power module and its spacers
    fasteners    all screws, in sub-groups per joint
"""

COLORS = {
    "base": "#2f6fdd", "carrier": "#8e44ad", "servo_pan": "#555555", "servo_tilt": "#555555",
    "plate_horn": "#17a2b8", "plate_shaft": "#7fd3df", "platform": "#e67e22",
    "body": "#f1c40f", "grip": "#d35400", "phone": "#2c3e50", "plug": "#000000", "cable": "#c0392b",
    "box_lower": "#6c8ea4", "box_upper": "#b8a1d6", "shelf": "#2ecc71",
    "foot0": "#333333", "foot1": "#333333", "foot2": "#333333", "foot3": "#333333",
    "speaker": "#e74c3c", "esp32": "#00a86b", "amp": "#f39c12", "dac": "#9b59b6", "psu": "#e8c547",
}
DEFAULT_COLOR = "#999999"
FASTENER_COLOR = "#e02020"       # red

# opacity: only the shells that hide the inside are see-through, everything else is solid
OPACITY = {"phone": 0.3, "box_lower": 0.3, "box_upper": 0.3, "shelf": 0.6}

GROUP = {
    **{n: "stand" for n in ("base", "carrier", "servo_pan", "servo_tilt", "plate_horn", "plate_shaft", "platform")},
    **{n: "holder" for n in ("body", "grip", "phone", "plug", "cable")},
    **{n: "enclosure" for n in ("box_lower", "box_upper", "shelf", "foot0", "foot1", "foot2", "foot3")},
    **{n: "electronics" for n in ("speaker", "esp32", "amp", "dac", "psu")},
}

# fastener sub-groups by name prefix (hw_<kind><index>)
FASTENER_JOINTS = {
    "hw_stand": "stand-to-lid", "hw_join": "box-halves", "hw_foot": "box-feet", "hw_shelfleg": "shelf-legs",
    "hw_amp": "amplifier", "hw_speaker": "speaker", "hw_psu": "power-module",
    "hw_panservo": "servo-pan", "hw_tiltservo": "servo-tilt", "hw_plate": "side-plates", "hw_holder": "holder",
}


def style(name):
    """(hex colour, opacity) of a part."""
    if name.startswith("hw_"):
        return FASTENER_COLOR, 1.0
    return COLORS.get(name, DEFAULT_COLOR), OPACITY.get(name, 1.0)


def group_path(name):
    """('group',) or ('fasteners', 'joint') for a part name."""
    if name.startswith("hw_"):
        for prefix, joint in FASTENER_JOINTS.items():
            if name.startswith(prefix):
                return ("fasteners", joint)
        return ("fasteners", "other")
    if name.startswith("psu_spacer"):
        return ("electronics",)
    return (GROUP.get(name, "other"),)
