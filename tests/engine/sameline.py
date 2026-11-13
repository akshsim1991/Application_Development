#!/usr/bin/env python3
"""True (exit 0) when two words are on the same line of page 1: their letters share a baseline (within 1 pt).
Prints where each was found otherwise. Usage: sameline.py file.pdf word1 word2"""
import sys
from pdfminer.high_level import extract_pages
from pdfminer.layout import LTTextContainer, LTTextLine, LTChar

path, a, b = sys.argv[1:4]
lines = []
for page in extract_pages(path, page_numbers=[0]):
    def walk(o):
        if isinstance(o, LTTextLine):
            chars = [c for c in o if isinstance(c, LTChar)]
            if chars: lines.append((o.get_text().strip(), min(c.y0 for c in chars), chars))
        elif hasattr(o, '__iter__'):
            for x in o: walk(x)
    walk(page)
def baseline(word):
    for text, y, chars in lines:
        if word in text: return y, text
    return None, None
ya, ta = baseline(a); yb, tb = baseline(b)
if ya is not None and yb is not None and abs(ya - yb) < 1: sys.exit(0)
print(f"  {a!r} at {ya} in {ta!r}; {b!r} at {yb} in {tb!r}")
for text, y, _ in lines: print(f"    {y:.1f} {text!r}")
sys.exit(1)
