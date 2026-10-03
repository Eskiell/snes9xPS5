#!/usr/bin/env python3
"""check_ppm.py file.ppm x y r g b [tolerance]  -- asserts the pixel colour of a host flip dump."""
import sys

path, x, y, r, g, b = sys.argv[1], *map(int, sys.argv[2:7])
tol = int(sys.argv[7]) if len(sys.argv) > 7 else 8
with open(path, "rb") as f:
    data = f.read()
parts = data.split(b"\n", 3)
w, h = map(int, parts[1].split())
pix = parts[3]
o = (y * w + x) * 3
pr, pg, pb = pix[o], pix[o + 1], pix[o + 2]
ok = abs(pr - r) <= tol and abs(pg - g) <= tol and abs(pb - b) <= tol
print(f"{path} ({x},{y}) = {pr},{pg},{pb} expected {r},{g},{b}: {'ok' if ok else 'FAIL'}")
sys.exit(0 if ok else 1)
