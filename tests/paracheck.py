"""Checks for the paragraph editing smoke test. Usage: paracheck.py same|others|xpos|overlap|moved|fonts before.pdf after.pdf"""
import re, subprocess, sys


def words(pdf):
    html = subprocess.run(["pdftotext", "-bbox", pdf, "-"], capture_output=True, text=True).stdout
    pat = r'<word xMin="([\d.]+)" yMin="([\d.]+)" xMax="([\d.]+)" yMax="([\d.]+)">([^<]*)</word>'
    return [(float(a), float(b), float(c), float(d), t) for a, b, c, d, t in re.findall(pat, html)]


def main():
    mode, before, after = sys.argv[1:4]
    a, b = words(before), words(after)
    if mode == "same":   # every word in the same place, to 0.5 pt (PDFium measures letters with the font program's own
        # advances, which differ from the PDF's rounded /Widths by a few thousandths of a point per letter)
        ka = sorted(a, key=lambda w: (round(w[1], 1), w[0]))
        kb = sorted(b, key=lambda w: (round(w[1], 1), w[0]))
        if len(ka) != len(kb):
            print("word count", len(ka), len(kb)); return 1
        for x, y in zip(ka, kb):
            if x[4] != y[4] or abs(x[0] - y[0]) > 0.5 or abs(x[1] - y[1]) > 0.5:
                print("first difference:", x, y); return 1
    elif mode == "others":   # words below the top line exactly where they were (to 0.05 pt)
        ka = sorted((w for w in a if w[1] > 100), key=lambda w: (round(w[1], 1), w[0]))
        kb = sorted((w for w in b if w[1] > 100), key=lambda w: (round(w[1], 1), w[0]))
        if len(ka) != len(kb) or any(x[4] != y[4] or abs(x[0] - y[0]) > 0.05 or abs(x[1] - y[1]) > 0.05 for x, y in zip(ka, kb)):
            for x, y in zip(ka, kb):
                if x[4] != y[4] or abs(x[0] - y[0]) > 0.05 or abs(x[1] - y[1]) > 0.05: print("moved:", x, y); break
            return 1
    elif mode == "xpos":   # same words, each at the same x on the same line, to 0.25 pt (a new font may have other heights and
        # widths rounded differently)
        def lines(ws):
            out, cur, last = [], [], None
            for w in sorted(ws, key=lambda w: (w[1] + w[3]) / 2):
                y = (w[1] + w[3]) / 2
                if last is not None and abs(y - last) > 3: out.append(sorted(cur)); cur = []
                cur.append((round(w[0], 2), w[4])); last = y
            return out + [sorted(cur)] if cur else out
        la, lb = lines(a), lines(b)
        for x, y in zip(la, lb):
            if [t for _, t in x] != [t for _, t in y] or any(abs(p[0] - q[0]) > 0.25 for p, q in zip(x, y)):
                print("differ:", x, y); return 1
        if len(la) != len(lb): print("lines", len(la), len(lb)); return 1
    elif mode == "fonts":   # no font that the original does not have
        def names(pdf):
            out = subprocess.run(["pdffonts", pdf], capture_output=True, text=True).stdout.splitlines()[2:]
            return {line.split()[0] for line in out if line.strip()}
        extra = names(after) - names(before)
        if extra:
            print("new fonts:", extra); return 1
    elif mode == "overlap":   # no two words of the edited page overlap
        for i, p in enumerate(b):
            for q in b[i + 1:]:
                if p[0] < q[2] - 0.5 and q[0] < p[2] - 0.5 and p[1] < q[3] - 0.5 and q[1] < p[3] - 0.5:
                    print("overlap:", p, q); return 1
    elif mode == "moved":   # the second paragraph starts lower than before
        ya = [w[1] for w in a if w[4] == "quarterly"]
        yb = [w[1] for w in b if w[4] == "quarterly"]
        if len(ya) < 2 or len(yb) < 2 or not yb[1] > ya[1] + 5:
            print("second paragraph:", ya[1:2], yb[1:2]); return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
