// Feather Image and Video Viewer - v0.3 (image viewing, animated GIF, rotate, delete)
// Pure Win32 + GDI/GDI+, statically linked. No .NET, no DirectX/D2D, no WinRT/UWP,
// no network, no services, no extra DLLs beyond core system ones that are present in
// Safe Mode (user32, gdi32, gdiplus, comdlg32, shell32, shlwapi). Software rendering only.
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>

using namespace Gdiplus;

static const wchar_t* APP_NAME = L"Feather Image and Video Viewer";
static const wchar_t* CLASS_NAME = L"FeatherViewerWnd";

enum { ID_OPEN = 100, ID_EXIT, ID_NEXT, ID_PREV, ID_FIRST, ID_LAST, ID_ZIN, ID_ZOUT, ID_FIT, ID_ACTUAL,
       ID_ROTR, ID_ROTL, ID_FULL, ID_ABOUT, ID_BG, ID_PAUSE, ID_DELETE };
static const UINT_PTR HIDE_TIMER = 2;

struct State {
    HWND hwnd = nullptr;
    HBITMAP hbm = nullptr;          // current image as DIB section
    int iw = 0, ih = 0;             // image size in px
    double zoom = 1.0;
    bool fit = true;
    double panX = 0, panY = 0;
    std::vector<std::wstring> files;
    int index = -1;
    std::wstring dir;
    std::wstring error;
    bool fullscreen = false;
    WINDOWPLACEMENT savedPlace{};
    DWORD savedStyle = 0;
    HMENU menu = nullptr;
    bool dragging = false;
    POINT dragStart{};
    double panStartX = 0, panStartY = 0;
    int bgMode = 0;                 // 0 dark, 1 light, 2 black
    int dpi = 96;
    int hover = -1;                 // hovered toolbar button
    bool barVisible = true;
    HFONT fSmall = nullptr, fBig = nullptr, fMid = nullptr;
} S;

static int Sx(int n) { return MulDiv(n, S.dpi, 96); }
static void Command(int id);
static COLORREF BgColor() {
    static const COLORREF c[3] = { RGB(32, 32, 32), RGB(235, 235, 235), RGB(0, 0, 0) };
    return c[S.bgMode];
}

// ---------- file helpers ----------
static bool IsImageExt(const wchar_t* name) {
    const wchar_t* e = PathFindExtensionW(name);
    static const wchar_t* exts[] = { L".jpg", L".jpeg", L".jpe", L".jfif", L".png", L".bmp", L".dib",
                                     L".gif", L".tif", L".tiff", L".ico", L".emf", L".wmf" };
    for (auto x : exts) if (lstrcmpiW(e, x) == 0) return true;
    return false;
}

static void ScanFolder(const std::wstring& dir) {
    S.files.clear();
    S.dir = dir;
    WIN32_FIND_DATAW fd;
    std::wstring pat = dir + L"\\*";
    HANDLE h = FindFirstFileW(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsImageExt(fd.cFileName))
            S.files.push_back(fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(S.files.begin(), S.files.end(),
              [](const std::wstring& a, const std::wstring& b) { return StrCmpLogicalW(a.c_str(), b.c_str()) < 0; });
}

// ---------- image loading ----------
static void FreeImage() {
    if (S.hbm) { DeleteObject(S.hbm); S.hbm = nullptr; }
    S.iw = S.ih = 0;
}

static void MakeHBitmap(Bitmap* b) {
    FreeImage();
    Color bg(255, GetRValue(BgColor()), GetGValue(BgColor()), GetBValue(BgColor()));
    HBITMAP h = nullptr;
    if (b->GetHBITMAP(bg, &h) == Ok && h) {
        S.hbm = h;
        S.iw = (int)b->GetWidth();
        S.ih = (int)b->GetHeight();
    }
}

static RotateFlipType ExifOrientation(Bitmap* b) {
    UINT size = b->GetPropertyItemSize(0x0112);
    if (!size) return RotateNoneFlipNone;
    std::vector<BYTE> buf(size);
    PropertyItem* p = (PropertyItem*)buf.data();
    if (b->GetPropertyItem(0x0112, size, p) != Ok || p->type != PropertyTagTypeShort || p->length < 2)
        return RotateNoneFlipNone;
    switch (*(unsigned short*)p->value) {
        case 2: return RotateNoneFlipX;
        case 3: return Rotate180FlipNone;
        case 4: return Rotate180FlipX;
        case 5: return Rotate90FlipX;
        case 6: return Rotate90FlipNone;
        case 7: return Rotate270FlipX;
        case 8: return Rotate270FlipNone;
    }
    return RotateNoneFlipNone;
}

static Bitmap* g_src = nullptr;   // kept so rotation does not need to reload from disk
// Animated GIF state
static UINT g_frames = 1, g_frame = 0;
static std::vector<UINT> g_delays;   // ms per frame
static int g_rot = 0;                // quarter turns clockwise (applied per frame when animated)
static bool g_paused = false;
static const UINT_PTR ANIM_TIMER = 1;

static void StopAnim() { if (S.hwnd) KillTimer(S.hwnd, ANIM_TIMER); }
static void ArmAnim() {
    if (g_frames > 1 && !g_paused && S.hwnd) SetTimer(S.hwnd, ANIM_TIMER, g_delays[g_frame], nullptr);
}
static void ShowFrame(UINT f) {
    if (!g_src) return;
    g_src->SelectActiveFrame(&FrameDimensionTime, f);
    for (int i = 0; i < g_rot; ++i) g_src->RotateFlip(Rotate90FlipNone);
    MakeHBitmap(g_src);
}
static void LoadFrameInfo(Bitmap* b) {
    g_frames = 1; g_frame = 0; g_rot = 0; g_delays.clear();
    UINT nd = b->GetFrameDimensionsCount();
    if (nd) {
        GUID id;
        if (b->GetFrameDimensionsList(&id, 1) == Ok && id == FrameDimensionTime) {
            UINT n = b->GetFrameCount(&FrameDimensionTime);
            if (n > 1) g_frames = n;
        }
    }
    if (g_frames > 1) {
        g_delays.assign(g_frames, 100);
        UINT size = b->GetPropertyItemSize(PropertyTagFrameDelay);
        if (size) {
            std::vector<BYTE> buf(size);
            PropertyItem* p = (PropertyItem*)buf.data();
            if (b->GetPropertyItem(PropertyTagFrameDelay, size, p) == Ok) {
                UINT cnt = p->length / sizeof(long);
                for (UINT i = 0; i < g_frames && i < cnt; ++i) {
                    UINT d = ((long*)p->value)[i] * 10;      // 1/100 s -> ms
                    g_delays[i] = d < 20 ? 100 : d;          // browsers treat 0/1 cs as 100 ms
                }
            }
        }
    }
}

static void UpdateTitle() {
    std::wstring t;
    if (S.index >= 0 && S.index < (int)S.files.size()) {
        wchar_t buf[512];
        if (S.hbm)
            swprintf(buf, 512, L"%ls  [%d/%d]  %dx%d  %d%%%ls  - %ls", S.files[S.index].c_str(), S.index + 1,
                     (int)S.files.size(), S.iw, S.ih, (int)(S.zoom * 100 + 0.5), g_frames > 1 ? L"  GIF" : L"", APP_NAME);
        else
            swprintf(buf, 512, L"%ls  [%d/%d]  (cannot open)  - %ls", S.files[S.index].c_str(), S.index + 1,
                     (int)S.files.size(), APP_NAME);
        t = buf;
    } else t = APP_NAME;
    SetWindowTextW(S.hwnd, t.c_str());
}

static void ClientSize(int& w, int& h) {
    RECT rc; GetClientRect(S.hwnd, &rc);
    w = rc.right; h = rc.bottom;
}

static void ClampPan() {
    int cw, ch; ClientSize(cw, ch);
    double dw = S.iw * S.zoom, dh = S.ih * S.zoom;
    double mx = dw > cw ? (dw - cw) / 2 : 0, my = dh > ch ? (dh - ch) / 2 : 0;
    S.panX = std::max(-mx, std::min(mx, S.panX));
    S.panY = std::max(-my, std::min(my, S.panY));
}

static void ApplyFit() {
    if (!S.hbm) return;
    int cw, ch; ClientSize(cw, ch);
    if (cw < 1 || ch < 1) return;
    double z = std::min((double)cw / S.iw, (double)ch / S.ih);
    S.zoom = std::min(1.0, z);
    S.panX = S.panY = 0;
}

static void Refresh() {
    if (S.fit) ApplyFit();
    ClampPan();
    UpdateTitle();
    InvalidateRect(S.hwnd, nullptr, FALSE);
}

static bool LoadIndex(int idx) {
    if (idx < 0 || idx >= (int)S.files.size()) return false;
    S.index = idx;
    S.error.clear();
    StopAnim();
    delete g_src; g_src = nullptr;
    g_frames = 1; g_frame = 0; g_rot = 0;
    FreeImage();
    std::wstring path = S.dir + L"\\" + S.files[idx];
    Bitmap* b = new Bitmap(path.c_str(), FALSE);
    if (b->GetLastStatus() != Ok || b->GetWidth() == 0) {
        delete b;
        S.error = L"Cannot open this file as an image.";
    } else {
        RotateFlipType rf = ExifOrientation(b);
        if (rf != RotateNoneFlipNone) b->RotateFlip(rf);
        g_src = b;
        LoadFrameInfo(b);
        MakeHBitmap(g_src);
        if (!S.hbm) S.error = L"Not enough memory to display this image.";
    }
    S.fit = true;
    Refresh();
    ArmAnim();
    return S.hbm != nullptr;
}

static void OpenPath(const std::wstring& pathIn) {
    wchar_t full[MAX_PATH * 2];
    if (!GetFullPathNameW(pathIn.c_str(), MAX_PATH * 2, full, nullptr)) return;
    DWORD a = GetFileAttributesW(full);
    if (a == INVALID_FILE_ATTRIBUTES) { S.error = L"File not found."; Refresh(); return; }
    std::wstring dir, name;
    if (a & FILE_ATTRIBUTE_DIRECTORY) { dir = full; }
    else {
        std::wstring f = full;
        size_t p = f.find_last_of(L'\\');
        dir = f.substr(0, p); name = f.substr(p + 1);
    }
    ScanFolder(dir);
    int idx = 0;
    if (!name.empty()) {
        bool found = false;
        for (size_t i = 0; i < S.files.size(); ++i)
            if (lstrcmpiW(S.files[i].c_str(), name.c_str()) == 0) { idx = (int)i; found = true; break; }
        if (!found) { S.files.push_back(name); idx = (int)S.files.size() - 1; } // unknown extension: try anyway
    }
    if (S.files.empty()) { S.index = -1; FreeImage(); S.error = L"No images in this folder."; Refresh(); return; }
    LoadIndex(idx);
}

static void Navigate(int delta) {
    int n = (int)S.files.size();
    if (!n) return;
    LoadIndex(((S.index + delta) % n + n) % n);
}

static void Rotate(bool cw) {
    if (!g_src) return;
    if (g_frames > 1) {
        g_rot = (g_rot + (cw ? 1 : 3)) & 3;
        ShowFrame(g_frame);
    } else {
        g_src->RotateFlip(cw ? Rotate90FlipNone : Rotate270FlipNone);
        MakeHBitmap(g_src);
    }
    S.fit = true;
    Refresh();
}

static void ZoomAt(double factor, int cx, int cy) {
    if (!S.hbm) return;
    int cw, ch; ClientSize(cw, ch);
    double nz = std::max(0.02, std::min(32.0, S.zoom * factor));
    double px = cx - cw / 2.0, py = cy - ch / 2.0;
    double qx = (px - S.panX) / S.zoom, qy = (py - S.panY) / S.zoom;
    S.zoom = nz; S.fit = false;
    S.panX = px - qx * nz; S.panY = py - qy * nz;
    Refresh();
}

static void ZoomCenter(double f) { int w, h; ClientSize(w, h); ZoomAt(f, w / 2, h / 2); }

static void SetActual() {
    if (!S.hbm) return;
    S.zoom = 1.0; S.fit = false; S.panX = S.panY = 0;
    Refresh();
}

static void ToggleFullscreen() {
    HWND h = S.hwnd;
    if (!S.fullscreen) {
        S.savedPlace.length = sizeof(S.savedPlace);
        GetWindowPlacement(h, &S.savedPlace);
        S.savedStyle = GetWindowLong(h, GWL_STYLE);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLong(h, GWL_STYLE, S.savedStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        S.fullscreen = true;
    } else {
        SetWindowLong(h, GWL_STYLE, S.savedStyle);
        SetWindowPlacement(h, &S.savedPlace);
        SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        S.fullscreen = false;
    }
}

static void DeleteCurrent() {
    if (S.index < 0 || S.index >= (int)S.files.size()) return;
    int keep = S.index;
    std::wstring path = S.dir + L"\\" + S.files[keep];
    // GDI+ keeps the file locked while the image is loaded: release it before deleting.
    StopAnim();
    delete g_src; g_src = nullptr;
    FreeImage();
    std::vector<wchar_t> buf(path.begin(), path.end());
    buf.push_back(0); buf.push_back(0);
    SHFILEOPSTRUCTW op = {};
    op.hwnd = S.hwnd; op.wFunc = FO_DELETE; op.pFrom = buf.data(); op.fFlags = FOF_ALLOWUNDO;  // Recycle Bin + confirmation
    int rc = SHFileOperationW(&op);
    bool gone = GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES;
    if (rc != 0 || op.fAnyOperationsAborted || !gone) { LoadIndex(keep); return; }   // cancelled/failed: show it again
    S.files.erase(S.files.begin() + keep);
    if (S.files.empty()) { S.index = -1; S.error = L"No more images in this folder."; Refresh(); return; }
    LoadIndex(std::min(keep, (int)S.files.size() - 1));
}

static void DoOpenDialog() {
    wchar_t file[MAX_PATH * 2] = L"";
    OPENFILENAMEW o = { sizeof(o) };
    o.hwndOwner = S.hwnd;
    o.lpstrFilter = L"Images\0*.jpg;*.jpeg;*.jpe;*.jfif;*.png;*.bmp;*.dib;*.gif;*.tif;*.tiff;*.ico;*.emf;*.wmf\0All files\0*.*\0";
    o.lpstrFile = file; o.nMaxFile = MAX_PATH * 2;
    o.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER;
    if (GetOpenFileNameW(&o)) OpenPath(file);
}


// ---------- modern overlay UI (pure GDI, works with the basic display driver) ----------
static const COLORREF C_BAR = RGB(36, 37, 41), C_BORDER = RGB(70, 72, 80), C_HOVER = RGB(70, 74, 88),
                      C_ICON = RGB(236, 238, 244), C_DIM = RGB(120, 123, 132), C_ACCENT = RGB(110, 168, 254);

struct Btn { int id; bool sepBefore; };
static const Btn BTNS[] = { {ID_OPEN, false}, {ID_PREV, true}, {ID_NEXT, false}, {ID_ZOUT, true}, {ID_ZIN, false},
                            {ID_FIT, false}, {ID_ACTUAL, false}, {ID_ROTL, true}, {ID_ROTR, false}, {ID_DELETE, true}, {ID_FULL, false} };
static const int NB = sizeof(BTNS) / sizeof(BTNS[0]);

// Computes the bar rectangle and every button rectangle.
static RECT BarLayout(int cw, int ch, RECT* out) {
    int bw = Sx(40), bh = Sx(40), pad = Sx(8), sep = Sx(14);
    int total = pad * 2;
    for (int i = 0; i < NB; ++i) total += bw + (BTNS[i].sepBefore ? sep : 0);
    RECT bar = { (cw - total) / 2, ch - bh - pad * 2 - Sx(18), 0, 0 };
    bar.right = bar.left + total; bar.bottom = bar.top + bh + pad * 2;
    int x = bar.left + pad;
    for (int i = 0; i < NB; ++i) {
        if (BTNS[i].sepBefore) x += sep;
        out[i] = { x, bar.top + pad, x + bw, bar.top + pad + bh };
        x += bw;
    }
    return bar;
}

static int HitBar(int mx, int my) {
    int cw, ch; ClientSize(cw, ch);
    RECT r[16]; RECT bar = BarLayout(cw, ch, r);
    POINT p = { mx, my };
    if (!PtInRect(&bar, p)) return -1;
    for (int i = 0; i < NB; ++i) if (PtInRect(&r[i], p)) return i;
    return -2;   // on the bar, between buttons
}

static void Line(HDC dc, int x1, int y1, int x2, int y2) { MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2); }

static void DrawIcon(HDC dc, int id, RECT r, COLORREF col) {
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2, u = Sx(7);
    HPEN pen = CreatePen(PS_SOLID, std::max(1, Sx(2)), col);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    switch (id) {
        case ID_OPEN:   // folder
            Rectangle(dc, cx - u - 1, cy - u / 2, cx + u + 2, cy + u);
            Line(dc, cx - u - 1, cy - u / 2, cx - u - 1, cy - u); Line(dc, cx - u - 1, cy - u, cx - 1, cy - u);
            Line(dc, cx - 1, cy - u, cx + 1, cy - u / 2);
            break;
        case ID_PREV: Line(dc, cx + u / 2, cy - u, cx - u / 2, cy); Line(dc, cx - u / 2, cy, cx + u / 2, cy + u); break;
        case ID_NEXT: Line(dc, cx - u / 2, cy - u, cx + u / 2, cy); Line(dc, cx + u / 2, cy, cx - u / 2, cy + u); break;
        case ID_ZOUT: Line(dc, cx - u, cy, cx + u + 1, cy); break;
        case ID_ZIN: Line(dc, cx - u, cy, cx + u + 1, cy); Line(dc, cx, cy - u, cx, cy + u + 1); break;
        case ID_FIT: Rectangle(dc, cx - u - 2, cy - u + 1, cx + u + 3, cy + u); break;
        case ID_ROTR: {
            SetArcDirection(dc, AD_CLOCKWISE);
            Arc(dc, cx - u, cy - u, cx + u, cy + u, cx + u, cy, cx, cy - u);
            POINT tri[3] = { { cx - 1, cy - u - Sx(4) }, { cx - 1, cy - u + Sx(4) }, { cx + Sx(4), cy - u } };
            HBRUSH b = CreateSolidBrush(col); SelectObject(dc, b); Polygon(dc, tri, 3);
            SelectObject(dc, GetStockObject(NULL_BRUSH)); DeleteObject(b);
            break;
        }
        case ID_ROTL: {
            SetArcDirection(dc, AD_COUNTERCLOCKWISE);
            Arc(dc, cx - u, cy - u, cx + u, cy + u, cx - u, cy, cx, cy - u);
            POINT tri[3] = { { cx + 1, cy - u - Sx(4) }, { cx + 1, cy - u + Sx(4) }, { cx - Sx(4), cy - u } };
            HBRUSH b = CreateSolidBrush(col); SelectObject(dc, b); Polygon(dc, tri, 3);
            SelectObject(dc, GetStockObject(NULL_BRUSH)); DeleteObject(b);
            break;
        }
        case ID_DELETE: {   // trash can
            Line(dc, cx - u, cy - u + 2, cx + u + 1, cy - u + 2);
            Line(dc, cx - 3, cy - u + 2, cx - 3, cy - u - 1); Line(dc, cx - 3, cy - u - 1, cx + 3, cy - u - 1);
            Line(dc, cx + 3, cy - u - 1, cx + 3, cy - u + 2);
            Rectangle(dc, cx - u + 2, cy - u + 2, cx + u, cy + u + 2);
            Line(dc, cx - 2, cy - u + 6, cx - 2, cy + u - 2); Line(dc, cx + 2, cy - u + 6, cx + 2, cy + u - 2);
            break;
        }
        case ID_FULL: {
            int k = Sx(5);
            int L = cx - u - 1, R = cx + u + 1, T = cy - u - 1, B = cy + u + 1;
            Line(dc, L, T, L + k, T); Line(dc, L, T, L, T + k);
            Line(dc, R, T, R - k, T); Line(dc, R, T, R, T + k);
            Line(dc, L, B, L + k, B); Line(dc, L, B, L, B - k);
            Line(dc, R, B, R - k, B); Line(dc, R, B, R, B - k);
            break;
        }
        case ID_ACTUAL: {
            SelectObject(dc, op); SelectObject(dc, ob);
            SelectObject(dc, S.fSmall); SetTextColor(dc, col); SetBkMode(dc, TRANSPARENT);
            DrawTextW(dc, L"1:1", -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(pen);
            return;
        }
    }
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen);
}

static void FillRound(HDC dc, RECT r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill); HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
}

static void DrawUI(HDC dc, int cw, int ch) {
    if (!S.hbm || S.barVisible) {
        RECT r[16]; RECT bar = BarLayout(cw, ch, r);
        FillRound(dc, bar, Sx(18), C_BAR, C_BORDER);
        for (int i = 0; i < NB; ++i) {
            if (i == S.hover) FillRound(dc, r[i], Sx(12), C_HOVER, C_HOVER);
            int id = BTNS[i].id;
            bool needsImg = id != ID_OPEN && id != ID_FULL;
            bool danger = id == ID_DELETE && i == S.hover;
            DrawIcon(dc, id, r[i], (!S.hbm && needsImg) ? C_DIM : (danger ? RGB(255, 120, 120) : i == S.hover ? C_ACCENT : C_ICON));
            if (BTNS[i].sepBefore) {
                HPEN p = CreatePen(PS_SOLID, 1, C_BORDER); HGDIOBJ op = SelectObject(dc, p);
                int sx = r[i].left - Sx(7); Line(dc, sx, r[i].top + Sx(8), sx, r[i].bottom - Sx(8));
                SelectObject(dc, op); DeleteObject(p);
            }
        }
        // info pill (top-left)
        if (S.hbm && S.index >= 0 && S.index < (int)S.files.size()) {
            wchar_t meta[128];
            swprintf(meta, 128, L"%d / %d   %d × %d   %d%%%ls", S.index + 1, (int)S.files.size(), S.iw, S.ih,
                     (int)(S.zoom * 100 + 0.5), g_frames > 1 ? (g_paused ? L"   GIF (paused)" : L"   GIF") : L"");
            std::wstring name = S.files[S.index];
            SelectObject(dc, S.fSmall);
            SIZE a, b; GetTextExtentPoint32W(dc, name.c_str(), (int)name.size(), &a);
            GetTextExtentPoint32W(dc, meta, (int)wcslen(meta), &b);
            int w = std::max(a.cx, b.cx) + Sx(28), h = a.cy + b.cy + Sx(18);
            RECT pill = { Sx(16), Sx(16), Sx(16) + std::min(w, cw - Sx(32)), Sx(16) + h };
            FillRound(dc, pill, Sx(14), C_BAR, C_BORDER);
            SetBkMode(dc, TRANSPARENT);
            RECT t1 = { pill.left + Sx(14), pill.top + Sx(8), pill.right - Sx(10), pill.top + Sx(8) + a.cy };
            SetTextColor(dc, C_ICON); DrawTextW(dc, name.c_str(), -1, &t1, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT t2 = { t1.left, t1.bottom, t1.right, t1.bottom + b.cy };
            SetTextColor(dc, C_DIM); DrawTextW(dc, meta, -1, &t2, DT_LEFT | DT_SINGLELINE);
        }
    }
}

static void DrawWelcome(HDC dc, RECT rc) {
    SetBkMode(dc, TRANSPARENT);
    bool light = S.bgMode == 1;
    COLORREF main = light ? RGB(40, 40, 46) : RGB(240, 242, 248), sub = light ? RGB(110, 110, 120) : RGB(150, 154, 166);
    int cy = (rc.top + rc.bottom) / 2 - Sx(40);
    RECT a = { rc.left, cy - Sx(30), rc.right, cy + Sx(30) };
    SelectObject(dc, S.fBig); SetTextColor(dc, S.error.empty() ? main : RGB(255, 140, 140));
    DrawTextW(dc, S.error.empty() ? L"Feather" : S.error.c_str(), -1, &a, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    RECT b = { rc.left, cy + Sx(34), rc.right, cy + Sx(60) };
    SelectObject(dc, S.fMid); SetTextColor(dc, sub);
    DrawTextW(dc, L"Image and Video Viewer", -1, &b, DT_CENTER | DT_TOP | DT_SINGLELINE);
    RECT c = { rc.left, cy + Sx(70), rc.right, cy + Sx(96) };
    SelectObject(dc, S.fSmall);
    DrawTextW(dc, L"Drop a picture here, or press Ctrl+O to open one.", -1, &c, DT_CENTER | DT_TOP | DT_SINGLELINE);
    RECT d = { rc.left, cy + Sx(94), rc.right, cy + Sx(120) };
    DrawTextW(dc, L"Right-click for more options.", -1, &d, DT_CENTER | DT_TOP | DT_SINGLELINE);
}

static void Paint(HDC hdc) {
    RECT rc; GetClientRect(S.hwnd, &rc);
    int cw = rc.right, ch = rc.bottom;
    if (cw < 1 || ch < 1) return;
    HDC mdc = CreateCompatibleDC(hdc);
    HBITMAP mbm = CreateCompatibleBitmap(hdc, cw, ch);
    HGDIOBJ oldm = SelectObject(mdc, mbm);
    HBRUSH br = CreateSolidBrush(BgColor());
    FillRect(mdc, &rc, br); DeleteObject(br);

    if (S.hbm) {
        HDC sdc = CreateCompatibleDC(hdc);
        HGDIOBJ olds = SelectObject(sdc, S.hbm);
        int dw = std::max(1, (int)(S.iw * S.zoom + 0.5)), dh = std::max(1, (int)(S.ih * S.zoom + 0.5));
        int x = (int)((cw - dw) / 2.0 + S.panX), y = (int)((ch - dh) / 2.0 + S.panY);
        if (S.zoom < 1.0) { SetStretchBltMode(mdc, HALFTONE); SetBrushOrgEx(mdc, 0, 0, nullptr); }
        else SetStretchBltMode(mdc, COLORONCOLOR);
        // Draw only the visible part of the source to keep huge zooms fast.
        int vx0 = std::max(0, -x), vy0 = std::max(0, -y);
        int vx1 = std::min(dw, cw - x), vy1 = std::min(dh, ch - y);
        if (vx1 > vx0 && vy1 > vy0) {
            int sx0 = (int)(vx0 / S.zoom), sy0 = (int)(vy0 / S.zoom);
            int sx1 = std::min(S.iw, (int)(vx1 / S.zoom) + 2), sy1 = std::min(S.ih, (int)(vy1 / S.zoom) + 2);
            int ddx = x + (int)(sx0 * S.zoom + 0.5), ddy = y + (int)(sy0 * S.zoom + 0.5);
            int ddw = (int)((sx1 - sx0) * S.zoom + 0.5), ddh = (int)((sy1 - sy0) * S.zoom + 0.5);
            if (S.zoom == 1.0) BitBlt(mdc, x, y, dw, dh, sdc, 0, 0, SRCCOPY);
            else StretchBlt(mdc, ddx, ddy, std::max(1, ddw), std::max(1, ddh), sdc, sx0, sy0, sx1 - sx0, sy1 - sy0, SRCCOPY);
        }
        SelectObject(sdc, olds); DeleteDC(sdc);
    } else {
        DrawWelcome(mdc, rc);
    }
    DrawUI(mdc, cw, ch);
    BitBlt(hdc, 0, 0, cw, ch, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, oldm); DeleteObject(mbm); DeleteDC(mdc);
}

static HMENU BuildMenu() {
    HMENU bar = CreatePopupMenu();
    HMENU f = CreatePopupMenu(), v = CreatePopupMenu(), n = CreatePopupMenu(), h = CreatePopupMenu();
    AppendMenuW(f, MF_STRING, ID_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(f, MF_STRING, ID_DELETE, L"&Delete (Recycle Bin)\tDel");
    AppendMenuW(f, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(f, MF_STRING, ID_EXIT, L"E&xit\tAlt+F4");
    AppendMenuW(n, MF_STRING, ID_PREV, L"&Previous\tLeft");
    AppendMenuW(n, MF_STRING, ID_NEXT, L"&Next\tRight");
    AppendMenuW(n, MF_STRING, ID_FIRST, L"&First\tHome");
    AppendMenuW(n, MF_STRING, ID_LAST, L"&Last\tEnd");
    AppendMenuW(v, MF_STRING, ID_ZIN, L"Zoom &in\t+");
    AppendMenuW(v, MF_STRING, ID_ZOUT, L"Zoom &out\t-");
    AppendMenuW(v, MF_STRING, ID_FIT, L"&Fit to window\t0");
    AppendMenuW(v, MF_STRING, ID_ACTUAL, L"&Actual size\t1");
    AppendMenuW(v, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(v, MF_STRING, ID_ROTR, L"Rotate &right\tR");
    AppendMenuW(v, MF_STRING, ID_ROTL, L"Rotate &left\tL / Shift+R");
    AppendMenuW(v, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(v, MF_STRING, ID_PAUSE, L"&Pause/resume animation\tP");
    AppendMenuW(v, MF_STRING, ID_BG, L"Change &background\tB");
    AppendMenuW(v, MF_STRING, ID_FULL, L"F&ull screen\tF11");
    AppendMenuW(h, MF_STRING, ID_ABOUT, L"&About");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)f, L"&File");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)n, L"&Navigate");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)v, L"&View");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)h, L"&Help");
    return bar;
}

static void Command(int id) {
    switch (id) {
        case ID_OPEN: DoOpenDialog(); break;
        case ID_EXIT: DestroyWindow(S.hwnd); break;
        case ID_NEXT: Navigate(1); break;
        case ID_PREV: Navigate(-1); break;
        case ID_FIRST: if (!S.files.empty()) LoadIndex(0); break;
        case ID_LAST: if (!S.files.empty()) LoadIndex((int)S.files.size() - 1); break;
        case ID_ZIN: ZoomCenter(1.25); break;
        case ID_ZOUT: ZoomCenter(0.8); break;
        case ID_FIT: S.fit = true; Refresh(); break;
        case ID_ACTUAL: SetActual(); break;
        case ID_ROTR: Rotate(true); break;
        case ID_ROTL: Rotate(false); break;
        case ID_FULL: ToggleFullscreen(); break;
        case ID_DELETE: DeleteCurrent(); break;
        case ID_PAUSE: g_paused = !g_paused; if (g_paused) StopAnim(); else ArmAnim(); break;
        case ID_BG:
            S.bgMode = (S.bgMode + 1) % 3;
            if (g_src) { if (g_frames > 1) ShowFrame(g_frame); else MakeHBitmap(g_src); Refresh(); } else InvalidateRect(S.hwnd, nullptr, FALSE);
            break;
        case ID_ABOUT:
            MessageBoxW(S.hwnd,
                L"Feather Image and Video Viewer 0.3\n\nLightweight viewer that also runs in Windows Safe Mode "
                L"(native Win32, software rendering, no network, no extra dependencies).\n\n"
                L"Formats: JPEG, PNG, BMP, animated GIF, TIFF, ICO, EMF, WMF.\n"
                L"Video playback: planned.\n\nDesigned and developed by Akshaya Simha.",
                APP_NAME, MB_OK | MB_ICONINFORMATION);
            break;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: S.hwnd = hwnd; DragAcceptFiles(hwnd, TRUE); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_TIMER:
            if (wp == HIDE_TIMER) {
                KillTimer(hwnd, HIDE_TIMER);
                if (S.hover < 0 && S.hbm && !S.dragging) { S.barVisible = false; InvalidateRect(hwnd, nullptr, FALSE); }
                return 0;
            }
            if (wp == ANIM_TIMER && g_src && g_frames > 1) {
                KillTimer(hwnd, ANIM_TIMER);
                g_frame = (g_frame + 1) % g_frames;
                ShowFrame(g_frame);
                InvalidateRect(hwnd, nullptr, FALSE);
                ArmAnim();
            }
            return 0;
        case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps); Paint(dc); EndPaint(hwnd, &ps); return 0; }
        case WM_SIZE: if (wp != SIZE_MINIMIZED) Refresh(); return 0;
        case WM_COMMAND: Command(LOWORD(wp)); return 0;
        case WM_DROPFILES: {
            wchar_t p[MAX_PATH * 2];
            if (DragQueryFileW((HDROP)wp, 0, p, MAX_PATH * 2)) OpenPath(p);
            DragFinish((HDROP)wp);
            return 0;
        }
        case WM_KEYDOWN: {
            bool ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
            switch (wp) {
                case VK_RIGHT: case VK_NEXT: case VK_SPACE: Navigate(1); break;
                case VK_LEFT: case VK_PRIOR: case VK_BACK: Navigate(-1); break;
                case VK_HOME: Command(ID_FIRST); break;
                case VK_END: Command(ID_LAST); break;
                case VK_ADD: case VK_OEM_PLUS: ZoomCenter(1.25); break;
                case VK_SUBTRACT: case VK_OEM_MINUS: ZoomCenter(0.8); break;
                case '0': case VK_NUMPAD0: Command(ID_FIT); break;
                case '1': case VK_NUMPAD1: Command(ID_ACTUAL); break;
                case 'R': Rotate(!shift); break;
                case VK_DELETE: DeleteCurrent(); break;
                case 'B': Command(ID_BG); break;
                case 'L': Rotate(false); break;
                case 'P': g_paused = !g_paused; if (g_paused) StopAnim(); else ArmAnim(); break;
                case 'O': if (ctrl) DoOpenDialog(); break;
                case VK_F11: case 'F': ToggleFullscreen(); break;
                case VK_ESCAPE: if (S.fullscreen) ToggleFullscreen(); break;
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            ZoomAt(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1.15 : 1 / 1.15, pt.x, pt.y);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(hwnd);
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            int hit = (S.barVisible || !S.hbm) ? HitBar(mx, my) : -1;
            if (hit != -1) { if (hit >= 0) Command(BTNS[hit].id); return 0; }
            SetCapture(hwnd);
            S.dragging = true; S.dragStart = { mx, my };
            S.panStartX = S.panX; S.panStartY = S.panY;
            return 0;
        }
        case WM_MOUSEMOVE: {
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            if (S.dragging) {
                S.panX = S.panStartX + mx - S.dragStart.x;
                S.panY = S.panStartY + my - S.dragStart.y;
                ClampPan();
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                bool wasVisible = S.barVisible;
                S.barVisible = true;
                int hit = HitBar(mx, my);
                int nh = hit >= 0 ? hit : -1;
                if (nh != S.hover || !wasVisible) { S.hover = nh; InvalidateRect(hwnd, nullptr, FALSE); }
                SetTimer(hwnd, HIDE_TIMER, 2500, nullptr);
            }
            return 0;
        }
        case WM_CONTEXTMENU: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            if (pt.x == -1) { pt.x = pt.y = 40; ClientToScreen(hwnd, &pt); }
            TrackPopupMenu(S.menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
            return 0;
        }
        case WM_LBUTTONUP: S.dragging = false; ReleaseCapture(); return 0;
        case WM_LBUTTONDBLCLK:
            if (S.fit && S.zoom < 1.0) SetActual(); else { S.fit = true; Refresh(); }
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursor(nullptr, S.dragging ? IDC_SIZEALL : IDC_ARROW)); return TRUE; }
            break;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    // High-DPI awareness if available (absent on very old Windows; safe to skip).
    typedef BOOL(WINAPI * SetDpiAware_t)();
    if (HMODULE u = GetModuleHandleW(L"user32.dll"))
        if (auto f = (SetDpiAware_t)GetProcAddress(u, "SetProcessDPIAware")) f();

    {
        HDC d = GetDC(nullptr);
        S.dpi = GetDeviceCaps(d, LOGPIXELSX);
        ReleaseDC(nullptr, d);
        if (S.dpi < 96) S.dpi = 96;
        auto mk = [](int px, int w) {
            return CreateFontW(-Sx(px), 0, 0, 0, w, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        };
        S.fSmall = mk(13, FW_NORMAL); S.fMid = mk(17, FW_NORMAL); S.fBig = mk(40, FW_LIGHT);
    }
    GdiplusStartupInput gi;
    ULONG_PTR token;
    if (GdiplusStartup(&token, &gi, nullptr) != Ok) {
        MessageBoxW(nullptr, L"Could not initialise GDI+.", APP_NAME, MB_OK | MB_ICONERROR);
        return 1;
    }
    {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hi;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wc.lpszClassName = CLASS_NAME;
        RegisterClassExW(&wc);

        S.menu = BuildMenu();
        HWND hwnd = CreateWindowExW(0, CLASS_NAME, APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                    1100, 750, nullptr, nullptr, hi, nullptr);
        // Dark title bar where the OS supports it (Windows 10/11); silently skipped otherwise (e.g. Safe Mode/DWM off).
        if (HMODULE dw = LoadLibraryW(L"dwmapi.dll")) {
            typedef HRESULT(WINAPI * SetAttr_t)(HWND, DWORD, LPCVOID, DWORD);
            if (auto f = (SetAttr_t)GetProcAddress(dw, "DwmSetWindowAttribute")) {
                BOOL on = TRUE; f(hwnd, 20, &on, sizeof(on)); f(hwnd, 19, &on, sizeof(on));
            }
        }
        ShowWindow(hwnd, show);
        UpdateWindow(hwnd);

        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv && argc > 1) OpenPath(argv[1]);
        if (argv) LocalFree(argv);
        UpdateTitle();

        MSG m;
        while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
        delete g_src; g_src = nullptr;
        FreeImage();
        DeleteObject(S.fSmall); DeleteObject(S.fMid); DeleteObject(S.fBig);
    }
    GdiplusShutdown(token);
    return 0;
}
