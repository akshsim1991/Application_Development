"""Are two result files the same apart from numbers a hair apart? PDFium measures some drawings a few hundredths of a
point differently on different systems (a 1 pt line's edge at 645.33 on Windows, 645.35 on the Mac). Words, counts and
everything else must be the same. Usage: close.py expected got [tolerance in points, default 0.15]; exit 0 when close."""
import re
import sys

NUM = re.compile(r"-?\d+\.\d+|-?\d+")


def parts(line):
    out, pos = [], 0
    for m in NUM.finditer(line):
        out.append(line[pos:m.start()])
        out.append(float(m.group()))
        pos = m.end()
    out.append(line[pos:])
    return out


def main():
    a = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
    b = open(sys.argv[2], encoding="utf-8", errors="replace").read().splitlines()
    tol = float(sys.argv[3]) if len(sys.argv) > 3 else 0.15
    if len(a) != len(b):
        return 1
    for x, y in zip(a, b):
        px, py = parts(x), parts(y)
        if len(px) != len(py):
            return 1
        for u, v in zip(px, py):
            if isinstance(u, float) != isinstance(v, float):
                return 1
            if isinstance(u, float) and abs(u - v) > tol:
                return 1
            if not isinstance(u, float) and u != v:
                return 1
    return 0


sys.exit(main())
