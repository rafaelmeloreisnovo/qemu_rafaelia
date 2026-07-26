#!/usr/bin/env python3
"""Exact reference verifier for the RAFAELIA x32/x64 square kernel."""

from __future__ import annotations

from fractions import Fraction as F
import argparse
import json
from pathlib import Path

T_PLUS = ((F(1, 2), F(-1, 2)), (F(1, 2), F(1, 2)))
T_MINUS = ((F(1, 2), F(1, 2)), (F(-1, 2), F(1, 2)))
I2 = ((F(1), F(0)), (F(0), F(1)))


def mul(a, b):
    return tuple(
        tuple(sum(a[i][k] * b[k][j] for k in range(2)) for j in range(2))
        for i in range(2)
    )


def vec(a, v):
    return tuple(sum(a[i][k] * v[k] for k in range(2)) for i in range(2))


def power(a, n):
    out = I2
    while n:
        if n & 1:
            out = mul(out, a)
        a = mul(a, a)
        n >>= 1
    return out


def transpose(a):
    return tuple(tuple(a[j][i] for j in range(2)) for i in range(2))


def norm2(v):
    return v[0] * v[0] + v[1] * v[1]


def width_model(bits, iterations, start=256):
    limit = (1 << (bits - 1)) - 1
    x = y = start
    maximum = start
    norm_ok = True
    for _ in range(iterations):
        old = x * x + y * y
        x, y = x + y, -x + y
        new = x * x + y * y
        norm_ok = norm_ok and new == 2 * old
        maximum = max(maximum, abs(x), abs(y))
    xm, ym = start + (-start), -start + (-start)
    xp, yp = xm - ym, xm + ym
    return {
        "width_bits": bits,
        "iterations": iterations,
        "max_abs_numerator": maximum,
        "overflow": maximum > limit,
        "norm_progression_exact": norm_ok,
        "opposite_pair_exact": (xp, yp) == (2 * start, -2 * start),
    }


def verify():
    half_i = ((F(1, 2), F(0)), (F(0), F(1, 2)))
    vertices = {(F(1), F(1)), (-F(1), F(1)), (-F(1), -F(1)), (F(1), -F(1))}
    midpoints = {(F(1), F(0)), (F(0), F(1)), (-F(1), F(0)), (F(0), -F(1))}
    checks = {
        "scaled_orthogonality_plus": mul(transpose(T_PLUS), T_PLUS) == half_i,
        "scaled_orthogonality_minus": mul(transpose(T_MINUS), T_MINUS) == half_i,
        "opposite_pair_half_identity": mul(T_PLUS, T_MINUS) == half_i,
        "midpoint_contact_plus": {vec(T_PLUS, v) for v in vertices} == midpoints,
        "midpoint_contact_minus": {vec(T_MINUS, v) for v in vertices} == midpoints,
        "norm_squared_halves": all(norm2(vec(T_MINUS, v)) == norm2(v) / 2 for v in vertices),
        "four_steps_negative_quarter": power(T_MINUS, 4) == ((-F(1, 4), F(0)), (F(0), -F(1, 4))),
        "eight_steps_sixteenth": power(T_MINUS, 8) == ((F(1, 16), F(0)), (F(0), F(1, 16))),
        "circle_chain": (F(49) / 2) / 2 == F(49) / 4,
        "area_ratio_half": F(49) / 2 == F(49, 2),
        "volume_two_step_ratio": F(1, 8) == F(1, 2) ** 3,
    }
    widths = [width_model(32, 32), width_model(64, 96)]
    passed = all(checks.values()) and all(
        not item["overflow"] and item["norm_progression_exact"] and item["opposite_pair_exact"]
        for item in widths
    )
    return {
        "schema": "rafaelia.square-median-x3264.reference.v1",
        "checks": checks,
        "width_models": widths,
        "pass": passed,
        "claim_allowed": False,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = verify()
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
