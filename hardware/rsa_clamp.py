"""Parametric NATO clamp with pogo pins for the DJI RS RSA port.

Generates three STL files in hardware/stl/: the clamp body (with the fixed jaw), the movable jaw and the
lid for the wiring cavity. Run with:  hardware/.venv/bin/python hardware/rsa_clamp.py

Axes: X across the rail, Y along the rail, Z up. The clamp is modelled with the rail channel facing up;
print the body and jaw as exported (flat bottom on the bed).

Parameters marked MEASURE are estimates from a photo: check them with a caliper before trusting a print.
"""

from pathlib import Path

import numpy as np
from manifold3d import CrossSection, Manifold

# --- Overall size (measured on the DJI accessory clamp) ---------------------------------------------------
TOTAL_W = 34.7  # across the rail, both jaws included, when clamped
LENGTH = 26.2  # along the rail
HEIGHT = 12.0  # MEASURE: clamp height

# --- Rail / dovetail ----------------------------------------------------------------------------------------
RAIL_W = 21.2  # MEASURE: widest point of the rail (NATO/Picatinny nominal 21.2)
RAIL_CLEARANCE = 0.3
DOVETAIL_DEPTH = 2.6  # MEASURE: depth of the dovetail undercut
DOVETAIL_ANGLE = 45.0  # degrees from horizontal

# --- Movable jaw ---------------------------------------------------------------------------------------------
JAW_W = 6.0
JAW_GAP = 0.5  # jaw-to-body gap when clamped; the jaw must open by more than the undercut to release
SCREW_Z = 3.5  # height of the M4 clamp screw and guide pins
SCREW_CLEARANCE_D = 4.4
INSERT_D = 5.6  # M4 heat-set insert hole
INSERT_DEPTH = 5.0
GUIDE_PIN_SPACING = 16.0  # two Ø3 mm steel dowels either side of the screw
GUIDE_PIN_SLIDE_D = 3.3  # in the jaw: sliding fit
GUIDE_PIN_PRESS_D = 2.9  # in the body: press fit
GUIDE_PIN_DEPTH = 5.5  # keeps the holes out of the wiring cavity

# --- Pogo pin connector (2 x 4) -----------------------------------------------------------------------------
PIN_PITCH = 2.0  # MEASURE: 2.0-2.3 mm estimated from the photo
CONNECTOR_X = 10.0  # MEASURE: pocket for a ready-made 2x4 pogo connector, set from its datasheet
CONNECTOR_Y = 5.0
CONNECTOR_CENTER_Y = LENGTH / 2  # MEASURE: the photo suggests ~11.5 mm from one end

# --- Wiring cavity (open at the bottom, closed by the lid) --------------------------------------------------
WALL = 1.2
CAVITY_X0 = 12.5  # stays clear of the heat-set insert
CABLE_HOLE_D = 5.0
LID_CLEARANCE = 0.2

SEGMENTS = 48
OUT_DIR = Path(__file__).resolve().parent / "stl"


def box(x0, y0, z0, x1, y1, z1):
    return Manifold.cube((x1 - x0, y1 - y0, z1 - z0)).translate((x0, y0, z0))


def x_cylinder(d, x0, length, y, z):
    """Cylinder along +X starting at x0."""
    return Manifold.cylinder(length, d / 2, circular_segments=SEGMENTS).rotate((0, 90, 0)).translate((x0, y, z))


def y_cylinder(d, y0, length, x, z):
    """Cylinder along +Y starting at y0."""
    return Manifold.cylinder(length, d / 2, circular_segments=SEGMENTS).rotate((-90, 0, 0)).translate((x, y0, z))


def xz_prism(points):
    """Extrudes an XZ polygon along Y over the full clamp length (with a little overshoot for clean cuts)."""
    section = CrossSection([[(x, z) for x, z in points]])
    # Extrusion runs along +Z in the XY plane; rotate so the section's Y becomes world Z and extrusion runs along +Y.
    prism = section.extrude(LENGTH + 2).rotate((90, 0, 0)).translate((0, LENGTH + 1, 0))
    return prism


def dovetail():
    floor_z = HEIGHT - DOVETAIL_DEPTH
    floor_w = RAIL_W + RAIL_CLEARANCE
    opening_w = floor_w - 2 * DOVETAIL_DEPTH / np.tan(np.radians(DOVETAIL_ANGLE))
    return floor_z, floor_w, opening_w


def body():
    cx = TOTAL_W / 2
    floor_z, floor_w, opening_w = dovetail()
    x0 = JAW_W + JAW_GAP
    solid = box(x0, 0, 0, TOTAL_W, LENGTH, HEIGHT)

    # Right half of the dovetail is the fixed jaw; everything left of the centre above the floor belongs to the
    # movable jaw.
    channel = xz_prism([
        (x0 - 1, floor_z),
        (cx + floor_w / 2, floor_z),
        (cx + opening_w / 2, HEIGHT),
        (cx + opening_w / 2, HEIGHT + 1),
        (x0 - 1, HEIGHT + 1),
    ])
    solid -= channel

    # Clamp screw insert and guide pin holes in the left face.
    y_mid = LENGTH / 2
    solid -= x_cylinder(INSERT_D, x0 - 1, INSERT_DEPTH + 1, y_mid, SCREW_Z)
    for dy in (-GUIDE_PIN_SPACING / 2, GUIDE_PIN_SPACING / 2):
        solid -= x_cylinder(GUIDE_PIN_PRESS_D, x0 - 1, GUIDE_PIN_DEPTH + 1, y_mid + dy, SCREW_Z)

    # Wiring cavity, open at the bottom.
    cavity = cavity_box()
    solid -= cavity

    # Pogo connector pocket through the channel floor into the cavity (only above it, to spare the insert wall).
    cavity_top = floor_z - WALL
    solid -= box(cx - CONNECTOR_X / 2, CONNECTOR_CENTER_Y - CONNECTOR_Y / 2, cavity_top - 0.01,
                 cx + CONNECTOR_X / 2, CONNECTOR_CENTER_Y + CONNECTOR_Y / 2, HEIGHT + 1)

    # Cable exit through the back face.
    solid -= y_cylinder(CABLE_HOLE_D, LENGTH - WALL - 1, WALL + 2, (CAVITY_X0 + TOTAL_W - WALL) / 2, cavity_top / 2)
    return solid


def cavity_box():
    floor_z, _, _ = dovetail()
    return box(CAVITY_X0, WALL, -1, TOTAL_W - WALL, LENGTH - WALL, floor_z - WALL)


def jaw():
    cx = TOTAL_W / 2
    floor_z, floor_w, opening_w = dovetail()
    # The hook starts 0.2 mm above the channel floor so it never rubs on the body.
    lift = 0.2
    hook_x = cx - floor_w / 2 + lift / np.tan(np.radians(DOVETAIL_ANGLE))
    solid = box(0, 0, 0, JAW_W, LENGTH, HEIGHT)
    solid += xz_prism([
        (JAW_W - 0.01, floor_z + lift),
        (hook_x, floor_z + lift),
        (cx - opening_w / 2, HEIGHT),
        (JAW_W - 0.01, HEIGHT),
    ]) ^ box(0, 0, 0, TOTAL_W, LENGTH, HEIGHT)

    y_mid = LENGTH / 2
    solid -= x_cylinder(SCREW_CLEARANCE_D, -1, JAW_W + 2, y_mid, SCREW_Z)
    for dy in (-GUIDE_PIN_SPACING / 2, GUIDE_PIN_SPACING / 2):
        solid -= x_cylinder(GUIDE_PIN_SLIDE_D, -1, JAW_W + 2, y_mid + dy, SCREW_Z)
    return solid


def lid():
    floor_z, _, _ = dovetail()
    w = TOTAL_W - WALL - CAVITY_X0 - 2 * LID_CLEARANCE
    l = LENGTH - 2 * WALL - 2 * LID_CLEARANCE
    return box(0, 0, 0, w, l, WALL)


def write_stl(manifold, path):
    mesh = manifold.to_mesh()
    verts = np.asarray(mesh.vert_properties)[:, :3].astype(np.float32)
    tris = np.asarray(mesh.tri_verts)
    v0, v1, v2 = verts[tris[:, 0]], verts[tris[:, 1]], verts[tris[:, 2]]
    normals = np.cross(v1 - v0, v2 - v0)
    lengths = np.linalg.norm(normals, axis=1, keepdims=True)
    normals = np.divide(normals, lengths, out=np.zeros_like(normals), where=lengths > 0)

    record = np.dtype([("normal", "<f4", 3), ("v", "<f4", (3, 3)), ("attr", "<u2")])
    data = np.zeros(len(tris), dtype=record)
    data["normal"] = normals
    data["v"] = np.stack([v0, v1, v2], axis=1)
    with open(path, "wb") as f:
        f.write(b"rsa_clamp".ljust(80, b" "))
        f.write(np.uint32(len(tris)).tobytes())
        f.write(data.tobytes())


def main():
    OUT_DIR.mkdir(exist_ok=True)
    floor_z, floor_w, opening_w = dovetail()
    print(f"dovetail: floor {floor_w:.2f} mm wide at z={floor_z:.2f}, opening {opening_w:.2f} mm")
    print(f"jaw must open by at least {(floor_w - opening_w) / 2:.2f} mm to release")
    for name, part in (("body", body()), ("jaw", jaw()), ("lid", lid())):
        if part.status().name != "NoError" or part.is_empty():
            raise SystemExit(f"{name}: invalid geometry ({part.status()})")
        path = OUT_DIR / f"rsa_clamp_{name}.stl"
        write_stl(part, path)
        (x0, y0, z0, x1, y1, z1) = part.bounding_box()
        print(f"{path.name}: {x1 - x0:.1f} x {y1 - y0:.1f} x {z1 - z0:.1f} mm, {part.volume() / 1000:.2f} cm3")


if __name__ == "__main__":
    main()
