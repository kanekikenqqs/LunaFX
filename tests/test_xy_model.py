#!/usr/bin/env python3
"""Independent reference tests for the eight-vertex octagonal XY mapping.

Mirrors Source/XYRouting.h. Constants are parsed from the C++ header so a
header change that silently disagrees with the documented model fails here.
"""
from __future__ import annotations

import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_constants():
    text = (ROOT / "Source/XYRouting.h").read_text(encoding="utf-8")

    def number(pattern: str) -> float:
        m = re.search(pattern, text)
        if not m:
            raise SystemExit(f"cannot find {pattern!r} in XYRouting.h")
        return float(m.group(1))

    slots = int(number(r"kNumSlots\s*=\s*(\d+)"))
    per_side = int(number(r"kSlotsPerSide\s*=\s*(\d+)"))
    radius = number(r"kSlotRadius\s*=\s*([0-9.]+)f")
    radial = number(r"kRadialGate\s*=\s*([0-9.]+)f")
    sigma = number(r"kAngularSigmaRad\s*=\s*([0-9.]+)f")

    angles_text = re.search(r"kSlotAngleDeg\[kNumSlots\]\s*=\s*\{([^}]*)\}", text)
    if not angles_text:
        raise SystemExit("cannot find kSlotAngleDeg")
    angles = [float(x.rstrip("f")) for x in re.findall(r"-?[0-9.]+f", angles_text.group(1))]

    order_text = re.search(r"kFieldOrder\[kNumSlots\]\s*=\s*\{([^}]*)\}", text)
    if not order_text:
        raise SystemExit("cannot find kFieldOrder")
    order = [int(x) for x in re.findall(r"\d+", order_text.group(1))]
    return slots, per_side, radius, radial, sigma, angles, order


SLOTS, PER_SIDE, RADIUS, RADIAL_GATE, ANGULAR_SIGMA, ANGLES, FIELD_ORDER = load_constants()


def position(i: int) -> tuple[float, float]:
    a = math.radians(ANGLES[i])
    return RADIUS * math.cos(a), RADIUS * math.sin(a)


def vertex(i: int) -> tuple[float, float]:
    return position(FIELD_ORDER[i % SLOTS])


def clamp01(v: float) -> float:
    return 0.0 if v < 0.0 else 1.0 if v > 1.0 else v


def smoothstep(v: float) -> float:
    v = clamp01(v)
    return v * v * (3.0 - 2.0 * v)


def inside_field(x: float, y: float) -> bool:
    eps = -1e-6
    for i in range(SLOTS):
        ax, ay = vertex(i)
        bx, by = vertex(i + 1)
        cross = (bx - ax) * (y - ay) - (by - ay) * (x - ax)
        if cross < eps:
            return False
    return True


def clamp_to_field(x: float, y: float) -> tuple[float, float]:
    if inside_field(x, y):
        return x, y
    best = (0.0, 0.0)
    best_d = float("inf")
    for i in range(SLOTS):
        ax, ay = vertex(i)
        bx, by = vertex(i + 1)
        dx, dy = bx - ax, by - ay
        len2 = dx * dx + dy * dy
        t = 0.0 if len2 == 0 else ((x - ax) * dx + (y - ay) * dy) / len2
        t = max(0.0, min(1.0, t))
        qx, qy = ax + t * dx, ay + t * dy
        d = (x - qx) ** 2 + (y - qy) ** 2
        if d < best_d:
            best_d, best = d, (qx, qy)
    return best


def shortest_angle(a: float) -> float:
    a = math.fmod(a + math.pi, 2 * math.pi)
    if a < 0:
        a += 2 * math.pi
    return a - math.pi


def proximity(x: float, y: float, slot: int) -> float:
    x, y = clamp_to_field(x, y)
    nx, ny = position(slot)
    r = math.hypot(x, y)
    radial = smoothstep(r / RADIAL_GATE)
    node_angle = math.atan2(ny, nx)
    puck_angle = math.atan2(y, x)
    d = abs(shortest_angle(puck_angle - node_angle))
    angular = math.exp(-0.5 * (d / ANGULAR_SIGMA) ** 2)
    return clamp01(radial * angular)


def weights(x: float, y: float) -> list[float]:
    return [proximity(x, y, i) for i in range(SLOTS)]


def effective(max_mix: float, depth: float, w: float, enabled: bool = True, assigned: bool = True) -> float:
    if not enabled or not assigned:
        return 0.0
    return min(1.0, max(0.0, max_mix * depth * w))


checks: list[tuple[str, bool]] = []


def check(name: str, ok: bool) -> None:
    checks.append((name, bool(ok)))


check("exactly eight vertices, four per side", SLOTS == 8 and PER_SIDE == 4)
check("left and right are mirrored", all(
    abs(position(i)[0] + position(i + 4)[0]) < 1e-9 and abs(position(i)[1] - position(i + 4)[1]) < 1e-9
    for i in range(4)))
check("vertices lie on the configured radius", all(abs(math.hypot(*position(i)) - RADIUS) < 1e-9 for i in range(8)))
check("four negative-x and four positive-x vertices",
      sum(position(i)[0] < 0 for i in range(8)) == 4 and sum(position(i)[0] > 0 for i in range(8)) == 4)

area2 = 0.0
for i in range(8):
    ax, ay = vertex(i)
    bx, by = vertex(i + 1)
    area2 += ax * by - bx * ay
check("field order is counter-clockwise", area2 > 0)
check("centre is inside the octagon", inside_field(0.0, 0.0))
check("square top-centre is outside the octagon", not inside_field(0.0, 1.0))

check("centre is exactly dry", all(w < 1e-9 for w in weights(0.0, 0.0)))
check("each vertex peaks at 1", all(abs(proximity(*position(i), i) - 1.0) < 1e-9 for i in range(8)))

adjacent = proximity(*position(0), 4)
check("45-degree neighbour is present but weaker", 0.25 < adjacent < 0.50)

nx, ny = position(1)
half_w = proximity(nx * 0.5, ny * 0.5, 1)
check("aligned tool exceeds 50% at half radius", half_w > 0.50)
check("half-radius influence is below the peak", half_w < 1.0)

min_active = 8
for a in range(0, 360, 15):
    rad = math.radians(a)
    for r in (0.43, 0.63, 0.80):
        w = weights(math.cos(rad) * r, math.sin(rad) * r)
        min_active = min(min_active, sum(1 for v in w if v > 0.02))
check("at least three tools remain active across the field", min_active >= 3)

length = math.hypot(nx, ny)
ux, uy = nx / length, ny / length
prev = -1.0
mono = True
for step in range(101):
    t = RADIUS * step / 100.0
    w = proximity(ux * t, uy * t, 1)
    mono &= w + 1e-6 >= prev
    prev = w
check("proximity rises directly toward a vertex", mono)

max_jump = 0.0
for a in range(0, 360, 2):
    rad = math.radians(a)
    x, y = math.cos(rad) * 0.73, math.sin(rad) * 0.73
    for i in range(8):
        max_jump = max(max_jump, abs(proximity(x, y, i) - proximity(x + 1e-4, y + 1e-4, i)))
check("mapping is continuous (no angular jumps)", max_jump < 1e-3)

ax, ay = position(0)
bx, by = position(4)
mx, my = (ax + bx) / 2, (ay + by) / 2
wa, wb = proximity(mx, my, 0), proximity(mx, my, 4)
check("octagon edge midpoint blends adjacent vertices", wa > 0.5 and wb > 0.5 and abs(wa - wb) < 0.02)

p = clamp_to_field(3.0, 4.0)
check("outside points clamp onto the octagon", inside_field(*p))
top = clamp_to_field(0.0, 4.0)
check("top clamp lands on the octagon edge", abs(top[1] - 0.795) < 0.03 and abs(top[0]) < 0.03)
check("far query is finite and bounded", all(0.0 <= proximity(50.0, -50.0, i) <= 1.0 for i in range(8)))

mix_ok = True
for i in range(8):
    w = proximity(*position(i), i)
    for depth in (0.0, 0.5, 1.0):
        for mm in (0.0, 0.37, 1.0):
            mix_ok &= 0.0 <= effective(mm, depth, w) <= 1.0
check("effective wet is bounded to 0..1", mix_ok)
check("unassigned is dry even at the vertex", all(effective(1.0, 1.0, proximity(*position(i), i), assigned=False) == 0.0 for i in range(8)))
check("disabled is dry even at the vertex", all(effective(1.0, 1.0, proximity(*position(i), i), enabled=False) == 0.0 for i in range(8)))
check("full mix at the vertex reaches max mix", effective(0.8, 1.0, 1.0) == 0.8)

check("radial gate is gentler than the octagon radius", 0.7 < RADIAL_GATE < 0.95)
check("angular overlap is broad", 0.45 < ANGULAR_SIGMA < 0.70)

# Print useful diagnostics before failure.
print(f"  half-radius weight = {half_w:.3f}")
print(f"  minimum active tools = {min_active}")
print(f"  adjacent weight = {adjacent:.3f}")

failures = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
if failures:
    raise SystemExit(f"{len(failures)} XY model check(s) failed")
print(f"All {len(checks)} XY model checks passed.")
