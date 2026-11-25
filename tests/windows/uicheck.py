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
app.kill()
print("ALL PASSED" if fails == 0 else f"{fails} FAILED", flush=True)
sys.exit(1 if fails else 0)
