"""The Windows app's toolbar on a real Windows (.github/workflows/windows-ui.yml): opens a 3-page PDF, finds the two
rotate buttons on the screen by their colour (orange, colourful toolbar icons), clicks "rotate right" (the page shown),
then Shift+clicks "rotate left" (every page), saves with Ctrl+S and checks the saved file: page 1 at 0 degrees, pages
2 and 3 at 270. Pictures of the screen go into the output folder. Usage: uicheck.py FeatherPDF.exe in.pdf out-folder"""
import ctypes, os, subprocess, sys, time
from ctypes import wintypes
from PIL import ImageGrab
import pikepdf

exe, pdf, out = sys.argv[1:4]
os.makedirs(out, exist_ok=True)
u = ctypes.windll.user32
u.SetProcessDPIAware()
fails = 0
def check(what, ok):
    global fails
    print(("  ok   " if ok else "  FAIL ") + what, flush=True)
    fails += 0 if ok else 1

def shot(name, log=False):
    img = ImageGrab.grab()
    img.save(os.path.join(out, name + ".png"))
    if log:   # into the log too, small, for checking by eye without downloading the pictures
        import base64, io
        small = img.convert("RGB"); small.thumbnail((900, 700))
        b = io.BytesIO(); small.save(b, "JPEG", quality=55)
        print(f"SCREENSHOT-BEGIN {name}\n{base64.b64encode(b.getvalue()).decode()}\nSCREENSHOT-END", flush=True)
    return img

def window():
    found = []
    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def each(h, _):
        n = u.GetWindowTextLengthW(h)
        b = ctypes.create_unicode_buffer(n + 1); u.GetWindowTextW(h, b, n + 1)
        if u.IsWindowVisible(h) and "Feather PDF" in b.value: found.append((h, b.value))
        return True
    u.EnumWindows(each, 0)
    return found

def close_app(proc):
    for h, _ in window(): u.PostMessageW(h, 0x0010, 0, 0)   # WM_CLOSE
    try: proc.wait(timeout=15)
    except Exception: proc.kill()
    time.sleep(1)

def click(x, y, shift=False):
    u.SetCursorPos(x, y); time.sleep(0.3)
    if shift: u.keybd_event(0x10, 0, 0, 0)
    u.mouse_event(2, 0, 0, 0, 0); time.sleep(0.05); u.mouse_event(4, 0, 0, 0, 0)
    if shift: time.sleep(0.05); u.keybd_event(0x10, 0, 2, 0)
    time.sleep(2)

def orange_buttons(img):
    """The centres of the orange icons in the toolbar (top of the screen), left to right."""
    w, h = img.size
    px = img.convert("RGB").load()
    pts = [(x, y) for y in range(0, min(160, h)) for x in range(w)
           if abs(px[x, y][0] - 0xD9) < 24 and abs(px[x, y][1] - 0x77) < 24 and abs(px[x, y][2] - 0x06) < 40]
    groups = []
    for x, y in sorted(pts):
        if groups and x - groups[-1][-1][0] <= 6: groups[-1].append((x, y))
        else: groups.append([(x, y)])
    groups = [g for g in groups if len(g) >= 15]
    return [(sum(p[0] for p in g) // len(g), sum(p[1] for p in g) // len(g)) for g in groups]

app = subprocess.Popen([exe, pdf])
for _ in range(60):
    time.sleep(1)
    if window(): break
time.sleep(4)
wins = window()
check(f"the window opened ({wins[0][1] if wins else 'none'})", bool(wins))
if wins:
    h = wins[0][0]
    u.ShowWindow(h, 3); u.SetForegroundWindow(h); time.sleep(3)
img = shot("01-open", log=True)
btns = orange_buttons(img)
print("  note orange icons at", btns, flush=True)
check("two rotate buttons in the toolbar", len(btns) == 2)
if len(btns) == 2:
    (lx, ly), (rx, ry) = btns
    u.SetCursorPos(rx, ry); time.sleep(2.5); shot("02-tooltip", log=True)
    click(rx, ry)
    shot("03-rotated-right")
    click(lx, ly, shift=True)
    shot("04-all-left", log=True)
    u.keybd_event(0x11, 0, 0, 0); u.keybd_event(0x53, 0, 0, 0); u.keybd_event(0x53, 0, 2, 0); u.keybd_event(0x11, 0, 2, 0)
    time.sleep(3)
    shot("05-saved")
    rot = [int(p.obj.get("/Rotate", 0)) % 360 for p in pikepdf.open(pdf).pages]
    check(f"the saved pages are turned as asked: {rot}", rot == [0, 270, 270])
close_app(app)

# ---- the Move tool: Ctrl+click adds to the selection; a box drawn picks everything it touches, each whole ----
def key(*codes):
    for c in codes: u.keybd_event(c, 0, 0, 0)
    for c in reversed(codes): u.keybd_event(c, 0, 2, 0)
    time.sleep(0.4)

def typed(text):
    for ch in text:
        vk = u.VkKeyScanW(ord(ch)) & 0xFF
        shift = (u.VkKeyScanW(ord(ch)) >> 8) & 1
        key(*( [0x10, vk] if shift else [vk] ))

def page_map(img):
    """Where the page is: from the test page's red band (595 points wide, its top edge the page's top, 842)."""
    w, h = img.size
    px = img.convert("RGB").load()
    red = lambda c: c[0] > 170 and c[1] < 80 and c[2] < 80
    rows = [y for y in range(0, min(h, 500)) if sum(1 for x in range(0, w, 4) if red(px[x, y])) > w // 16]
    if not rows: return None
    y0 = rows[0]
    xs = [x for x in range(w) if red(px[x, y0 + 3])]
    left, right = min(xs), max(xs)
    s = (right - left + 1) / 595.0
    return lambda x, y: (int(left + x * s), int(y0 + (842 - y) * s))

def text_of(path):
    from pdfminer.high_level import extract_text
    return extract_text(path)

import shutil
for scenario in ("ctrl", "box"):
    work = os.path.join(out, f"move-{scenario}.pdf")
    shutil.copy(sys.argv[4] if len(sys.argv) > 4 else pdf, work)
    app = subprocess.Popen([exe, work])
    for _ in range(60):
        time.sleep(1)
        if window(): break
    time.sleep(4)
    h = window()[0][0] if window() else None
    if h: u.ShowWindow(h, 3); u.SetForegroundWindow(h); time.sleep(3)
    shot(f"10-{scenario}-open")
    key(0x11, 0x4B)                       # Ctrl+K: the command palette
    typed("Move or resize"); time.sleep(0.5); key(0x0D)
    time.sleep(2)
    # (the Move tool shows the editing row, which moves and zooms the page: where it is, found now)
    to = page_map(shot(f"10-{scenario}-tool", log=True))
    check(f"{scenario}: the page is found on the screen", to is not None)
    if not to: close_app(app); continue
    if scenario == "ctrl":
        click(*to(110, 765))                  # the heading
        u.SetCursorPos(*to(160, 703)); time.sleep(0.3)
        u.keybd_event(0x11, 0, 0, 0); u.mouse_event(2, 0, 0, 0, 0); time.sleep(0.05); u.mouse_event(4, 0, 0, 0, 0); u.keybd_event(0x11, 0, 2, 0)
        time.sleep(2)
        shot("11-ctrl-picked", log=True)
    else:
        (x0, y0), (x1, y1) = to(85, 584), to(300, 448)
        u.SetCursorPos(x0, y0); time.sleep(0.3); u.mouse_event(2, 0, 0, 0, 0)
        for k in range(1, 21):
            u.SetCursorPos(x0 + (x1 - x0) * k // 20, y0 + (y1 - y0) * k // 20); time.sleep(0.03)
        u.mouse_event(4, 0, 0, 0, 0); time.sleep(2.5)
        shot("12-box-picked", log=True)
    key(0x2E); time.sleep(1.5)            # Delete
    key(0x11, 0x53); time.sleep(3)        # Ctrl+S
    shot(f"13-{scenario}-deleted", log=True)
    close_app(app)
    t = text_of(work)
    print("  note saved text: " + " | ".join(l for l in t.splitlines() if l.strip())[:600], flush=True)
    if scenario == "ctrl":
        check("Ctrl+click: the heading and the paragraph clicked are deleted together, the rest kept",
              "Quarterly Report" not in [l.strip() for l in t.splitlines()] and "Sales rose" not in t and "Online orders" in t and "Next steps" in t)
    else:
        check("a box drawn: the chart's labels it touched are deleted, the text beside it kept",
              "Jul" not in t and "Sep" not in t and "Next steps" in t and "Sales by month" in t and "Online orders" in t)

# ---- the toolbar's other buttons: thumbnails and bookmarks, print, fit, night colours, full screen ----
def pixel(img, xy):
    return img.convert("RGB").getpixel(xy)

def page_box(img):
    """The red band's left and right ends on the screen (the page's width)."""
    w, h = img.size
    px = img.convert("RGB").load()
    red = lambda c: c[0] > 150 and c[1] < 90 and c[2] < 90
    for y in range(80, min(h, 500)):
        xs = [x for x in range(0, w, 2) if red(px[x, y])]
        if len(xs) > w // 12: return min(xs), max(xs), y
    return None

work = os.path.join(out, "buttons.pdf")
shutil.copy(pdf, work)
app = subprocess.Popen([exe, work])
for _ in range(60):
    time.sleep(1)
    if window(): break
time.sleep(4)
h = window()[0][0] if window() else None
if h: u.ShowWindow(h, 3); u.SetForegroundWindow(h); time.sleep(3)
img = shot("20-buttons", log=True)
btns = orange_buttons(img)
box0 = page_box(img)
check(f"buttons: the rotate buttons and the page are found ({btns}, {box0})", len(btns) == 2 and box0 is not None)
if len(btns) == 2 and box0:
    (lx, by), (rx, _) = btns
    # (positions from the toolbar's layout: 36-pixel icons, 13-pixel separators)
    sidebar, printer = (26, by), (147, by)
    fit, night, full = (lx - 49, by), (rx + 49, by), (rx + 85, by)
    click(*sidebar); time.sleep(1.5)
    box1 = page_box(shot("21-sidebar", log=True))
    check(f"the thumbnails button shows the sidebar: the page moves right ({box0[0]} -> {box1[0] if box1 else None})", box1 is not None and box1[0] > box0[0] + 40)
    click(*sidebar); time.sleep(1.5)
    box2 = page_box(shot("22-sidebar-hidden", log=True))
    check(f"and hides it again ({box2})", box2 is not None and abs(box2[0] - box0[0]) < 12)
    click(*fit); time.sleep(1.5)
    box3 = page_box(shot("23-fit-page", log=True))
    check(f"the fit button shows the whole page: narrower ({box0[1]-box0[0]} -> {box3[1]-box3[0] if box3 else None})", box3 is not None and (box3[1] - box3[0]) < (box0[1] - box0[0]) * 0.9)
    click(*fit); time.sleep(1.5)
    box4 = page_box(shot("24-fit-width", log=True))
    check(f"and the width again ({box4})", box4 is not None and abs((box4[1] - box4[0]) - (box0[1] - box0[0])) < 12)
    probe = ((box0[0] + box0[1]) // 2, box0[2] + 420)   # white paper below the text
    before = pixel(shot("25-normal"), probe)
    click(*night); time.sleep(2)
    after = pixel(shot("26-night", log=True), probe)
    check(f"the night button darkens the page ({before} -> {after})", sum(before) > 600 and sum(after) < 300)
    click(*night); time.sleep(2)
    back = pixel(shot("27-normal-again"), probe)
    check(f"and back to normal ({back})", sum(back) > 600)
    click(*full); time.sleep(2.5)
    fs = shot("28-full-screen", log=True)
    check("the full screen button hides the toolbar", len(orange_buttons(fs)) == 0)
    key(0x1B); time.sleep(2)
    check("Esc brings it back", len(orange_buttons(shot("29-full-screen-off"))) == 2)
    before_titles = {t for _, t in window()}
    click(*printer); time.sleep(4)
    shot("30-print", log=True)
    found = []
    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def each(hh, _):
        n = u.GetWindowTextLengthW(hh)
        b = ctypes.create_unicode_buffer(n + 1); u.GetWindowTextW(hh, b, n + 1)
        if u.IsWindowVisible(hh) and "print" in b.value.lower(): found.append((hh, b.value))
        return True
    u.EnumWindows(each, 0)
    check(f"the print button opens the print window ({[t for _, t in found]})", len(found) > 0)
    for hh, _ in found: u.PostMessageW(hh, 0x0010, 0, 0)
    time.sleep(2)
close_app(app)

print("ALL PASSED" if fails == 0 else f"{fails} FAILED", flush=True)
sys.exit(1 if fails else 0)
