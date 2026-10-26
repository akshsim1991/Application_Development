"""Every command a menu offers must be handled somewhere: a `case X:`, a comparison with X, or a range test that
includes it (`cmd >= A && cmd <= B`, `cmd >= A && cmd < A + n`). Usage: dispatchcheck.py src/main.cpp"""
import re, sys

src = open(sys.argv[1], encoding="utf-8").read()
code = re.sub(r"//[^\n]*", "", src)

# enum values, in declaration order (explicit values restart the count)
values = {}
for body in re.findall(r"enum\s*(?:class\s+)?\w*\s*(?::\s*\w+\s*)?\{([^}]*)\}", code):
    n = -1
    for part in body.split(","):
        part = part.strip()
        if not part:
            continue
        m = re.match(r"(\w+)\s*=\s*([^,]+)$", part)
        if m:
            name, expr = m.group(1), m.group(2).strip()
            try:
                n = int(eval(expr, {}, values))
            except Exception:
                n = values.get(expr, n + 1)
        else:
            name = re.match(r"(\w+)", part).group(1)
            n += 1
        values.setdefault(name, n)

# commands put into menus
used = set()
for pat in (r"\badd\(\s*([A-Z][A-Z0-9_]+)\s*[,)]", r"\bpadd\(\s*([A-Z][A-Z0-9_]+)\s*,",
            r"AppendMenuW\([^,]+,[^,]+,\s*([A-Z][A-Z0-9_]+)\s*(?:\+[^,]+)?,"):
    used |= set(re.findall(pat, code))
used = {u for u in used if u in values and not u.startswith(("MF_", "TPM_"))}

handled = set(re.findall(r"\bcase\s+([A-Z][A-Z0-9_]+)\s*(?:\+\s*\d+\s*)?:", code))
handled |= set(re.findall(r"==\s*([A-Z][A-Z0-9_]+)\b", code))
handled |= set(re.findall(r"\b([A-Z][A-Z0-9_]+)\s*==", code))
def body_of(fn):
    """The body of `void fn(...) {...}` (its last definition), or None."""
    starts = [m.end() for m in re.finditer(r"\b(?:void|bool|int)\s+" + fn + r"\s*\([^;{]*\)\s*\{", code)]
    if not starts:
        return None
    i, depth = starts[-1], 1
    while depth and i < len(code):
        depth += {"{": 1, "}": -1}.get(code[i], 0)
        i += 1
    return code[starts[-1]:i]

# A range test that hands the command to a function covers only the commands that function has a case for.
ranges = []   # (low, high, function or None)
for a, b, rest in re.findall(r"cmd\s*>=\s*([A-Z][A-Z0-9_]+)\s*&&\s*cmd\s*<=\s*([A-Z][A-Z0-9_]+)\s*\)\s*\{?([^\n]*)", code):
    if a in values and b in values: ranges.append((values[a], values[b], rest))
for a, b, k, rest in re.findall(r"cmd\s*>=\s*([A-Z][A-Z0-9_]+)\s*&&\s*cmd\s*<\s*([A-Z][A-Z0-9_]+)\s*\+\s*(\d+)\s*\)\s*\{?([^\n]*)", code):
    if a in values and b in values: ranges.append((values[a], values[b] + int(k) - 1, rest))

def covered(u):
    for lo, hi, rest in ranges:
        if not lo <= values[u] <= hi:
            continue
        call = re.match(r"\s*(?:\{\s*)?(\w+)\(\s*cmd\s*[,)]", rest)   # the command itself is passed on (not an index)
        if not call:
            return True   # handled right there (an index into a table, a setting)
        body = body_of(call.group(1))
        if body is None or re.search(r"\bcase\s+" + u + r"\s*:", body):
            return True
    return False

missing = sorted(u for u in used if u not in handled and not covered(u))
for m in missing:
    print("not handled:", m, values[m])
print(f"menu commands={len(used)} not handled={len(missing)}")
sys.exit(1 if missing else 0)
