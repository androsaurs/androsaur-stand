"""Builds the named, coloured group tree of the assembly for the viewer and the STEP export."""
from build123d import Color, Compound

from palette import group_path, style

ORDER = ["stand", "holder", "enclosure", "electronics", "fasteners", "other"]


def build(shapes, glass=True):
    """shapes: dict name -> Shape (from assemble). glass=False makes every part opaque. Returns a list of top-level Compounds."""
    tree = {}
    for name, shape in shapes.items():
        hexcol, alpha = style(name)
        r, g, b = (int(hexcol[i:i + 2], 16) / 255 for i in (1, 3, 5))
        shape.label = name
        shape.color = Color(r, g, b, alpha if glass else 1.0)
        path = group_path(name)
        tree.setdefault(path[0], {}).setdefault(path[1] if len(path) > 1 else None, []).append(shape)
    tops = []
    for top in sorted(tree, key=lambda t: ORDER.index(t) if t in ORDER else len(ORDER)):
        subs = tree[top]
        children = list(subs.get(None, []))
        children += [Compound(children=shapes_, label=sub) for sub, shapes_ in subs.items() if sub is not None]
        tops.append(Compound(children=children, label=top))
    return tops
