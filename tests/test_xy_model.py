#!/usr/bin/env python3
"""Independent reference tests for the eight-slot distance-based XY mapping.

The reference implementation mirrors the documented formula in
Source/XYRouting.h. The C++ constants are parsed from the header and compared
with the reference, so a change in the header that is not reflected in the
documented model fails this test.
"""
from __future__ import annotations

import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_constants():
    text = (ROOT / "Source/XYRouting.h").read_text(encoding="utf-8")
    def one(pattern: str) -> float:
        m = re.search(pattern, text)
        if not m:
            raise SystemExit(f"cannot find {pattern!r} in XYRouting.h")
        return float(m.group(1))
    slots = int(one(r"kNumSlots\s*=\s*(\d+)"))
    per_side = int(one(r"kSlotsPerSide\s*=\s*(\d+)"))
    radius = one(r"kSlotRadius\s*=\s*([0-9.]+)f")
    falloff = one(r"kProximityFalloff\s*=\s*([0-9.]+)f")
    angles_text = re.search(r"kSlotAngleDeg\[kNumSlots\]\s*=\s*\{([^}]*)\}", text)
    if not angles_text:
        raise SystemExit("cannot find kSlotAngleDeg")
    angles = [float(x.rstrip("f")) for x in re.findall(r"-?[0-9.]+f", angles_text.group(1))]
    return slots, per_side, radius, falloff, angles


SLOTS, PER_SIDE, RADIUS, FALLOFF, ANGLES = load_constants()


def position(i: int) -> tuple[float, float]:
    a = math.radians(ANGLES[i])
    return RADIUS * math.cos(a), RADIUS * math.sin(a)


def clamp01(v: float) -> float:
    return 0.0 if v < 0.0 else 1.0 if v > 1.0 else v


def clamp_disk(x: float, y: float) -> tuple[float, float]:
    length = math.hypot(x, y)
    if length > 1.0 and length > 0.0:
        return x / length, y / length
    return x, y


def proximity(x: float, y: float, slot: int) -> float:
    x, y = clamp_disk(x, y)
    nx, ny = position(slot)
    d = math.hypot(x - nx, y - ny)
    s = clamp01(1.0 - d / max(1e-4, FALLOFF))
    return s * s * (3.0 - 2.0 * s)


def effective(max_mix: float, depth: float, w: float, enabled: bool = True, assigned: bool = True) -> float:
    if not enabled or not assigned:
        return 0.0
    return min(1.0, max(0.0, max_mix * depth * w))


checks: list[tuple[str, bool]] = []


def check(name: str, ok: bool) -> None:
    checks.append((name, bool(ok)))


check("exactly eight slots, four per side", SLOTS == 8 and PER_SIDE == 4)
check("four nodes have negative x and four positive", sum(1 for i in range(8) if position(i)[0] < 0) == 4
      and sum(1 for i in range(8) if position(i)[0] > 0) == 4)
check("nodes sit on the configured rim radius", all(abs(math.hypot(*position(i)) - RADIUS) < 1e-9 for i in range(8)))
check("left and right are mirrored", all(
    abs(position(i)[0] + position(i + 4)[0]) < 1e-9 and abs(position(i)[1] - position(i + 4)[1]) < 1e-9
    for i in range(4)))

# centre dry
check("centre is dry", all(proximity(0.0, 0.0, i) < 1e-4 for i in range(8)))

# exact nodes peak
peak_ok = True
neighbour_ok = True
for i in range(8):
    x, y = position(i)
    peak_ok &= abs(proximity(x, y, i) - 1.0) < 1e-9
    for j in range(8):
        if i != j:
            neighbour_ok &= proximity(x, y, j) <= 0.2
check("each node peaks at 1 at its own position", peak_ok)
check("adjacent bleed stays below 20%", neighbour_ok)

# monotonic approach
mono = True
prev = -1.0
nx, ny = position(1)
length = math.hypot(nx, ny)
for step in range(101):
    t = RADIUS * step / 100.0
    w = proximity(nx / length * t, ny / length * t, 1)
    mono &= w + 1e-12 >= prev
    prev = w
check("proximity rises monotonically while approaching", mono)

# continuity
max_jump = 0.0
for a in range(0, 360, 2):
    rad = math.radians(a)
    x, y = math.cos(rad) * 0.73, math.sin(rad) * 0.73
    for i in range(8):
        max_jump = max(max_jump, abs(proximity(x, y, i) - proximity(x + 1e-4, y + 1e-4, i)))
check("mapping is continuous (no angular jumps)", max_jump < 1e-3)

# adjacent blend
ax, ay = position(0)
bx, by = position(1)
mx, my = (ax + bx) / 2, (ay + by) / 2
wa, wb = proximity(mx, my, 0), proximity(mx, my, 1)
check("adjacent nodes blend at the midpoint", wa > 0.5 and wb > 0.5 and abs(wa - wb) < 0.01)

# far and clamped input
check("far influence reaches zero", all(proximity(-position(i)[0] * 0.9, -position(i)[1] * 0.9, i) < 1e-6 for i in range(8)))
cx, cy = clamp_disk(500.0, -500.0)
check("puck outside the disk is clamped", abs(math.hypot(cx, cy) - 1.0) < 1e-9 and all(0.0 <= proximity(500, -500, i) <= 1.0 for i in range(8)))

# mix model
mix_ok = True
for i in range(8):
    x, y = position(i)
    w = proximity(x, y, i)
    for depth in (0.0, 0.5, 1.0):
        for mm in (0.0, 0.37, 1.0):
            mix_ok &= 0.0 <= effective(mm, depth, w) <= 1.0
check("effective wet is bounded to 0..1", mix_ok)
check("unassigned is dry even at the node", all(effective(1.0, 1.0, proximity(*position(i), i), assigned=False) == 0.0 for i in range(8)))
check("disabled is dry even at the node", all(effective(1.0, 1.0, proximity(*position(i), i), enabled=False) == 0.0 for i in range(8)))
check("enabled + zero proximity is dry", all(effective(1.0, 1.0, 0.0) == 0.0 for _ in range(8)))
check("full mix at the node reaches max mix", effective(0.8, 1.0, 1.0) == 0.8)

# close to the document values
check("gentle falloff far below the rim", 0.30 < FALLOFF < 0.95)
check("nodes are inside the disk", 0.7 < RADIUS < 1.0)

failures = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
if failures:
    raise SystemExit(f"{len(failures)} XY model check(s) failed")
print(f"All {len(checks)} XY model checks passed.")
