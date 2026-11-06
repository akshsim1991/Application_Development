"""Prints a PDF's page count and its bookmarks as 'Title->page' (nested ones indented), for the checks. Needs pikepdf."""
import sys
import pikepdf

p = pikepdf.open(sys.argv[1])
pages = {pg.objgen: i + 1 for i, pg in enumerate(p.pages)}
with p.open_outline() as o:
    def walk(items, depth=0):
        out = []
        for it in items:
            dest = it.destination
            target = pages.get(dest[0].objgen, "?") if dest is not None and len(dest) else "?"
            out.append("  " * depth + f"{it.title}->{target}")
            out += walk(it.children, depth + 1)
        return out
    print(len(p.pages), "; ".join(walk(o.root)))
