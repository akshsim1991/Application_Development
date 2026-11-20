// Feather Image and Video Viewer - v0.1 (image viewing)
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
       ID_ROTR, ID_ROTL, ID_FULL, ID_ABOUT, ID_BG };

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
} S;

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

static void UpdateTitle() {
    std::wstring t;
    if (S.index >= 0 && S.index < (int)S.files.size()) {
        wchar_t buf[512];
        if (S.hbm)
            swprintf(buf, 512, L"%ls  [%d/%d]  %dx%d  %d%%  - %ls", S.files[S.index].c_str(), S.index + 1,
                     (int)S.files.size(), S.iw, S.ih, (int)(S.zoom * 100 + 0.5), APP_NAME);
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
    delete g_src; g_src = nullptr;
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
        MakeHBitmap(g_src);
        if (!S.hbm) S.error = L"Not enough memory to display this image.";
    }
    S.fit = true;
    Refresh();
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
    g_src->RotateFlip(cw ? Rotate90FlipNone : Rotate270FlipNone);
    MakeHBitmap(g_src);
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
        SetMenu(h, nullptr);
        SetWindowLong(h, GWL_STYLE, S.savedStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        S.fullscreen = true;
    } else {
        SetWindowLong(h, GWL_STYLE, S.savedStyle);
        SetMenu(h, S.menu);
        SetWindowPlacement(h, &S.savedPlace);
        SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        S.fullscreen = false;
    }
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
        SetBkMode(mdc, TRANSPARENT);
        SetTextColor(mdc, S.bgMode == 1 ? RGB(60, 60, 60) : RGB(200, 200, 200));
        HFONT f = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HGDIOBJ oldf = SelectObject(mdc, f);
        std::wstring msg = S.error.empty()
            ? L"Open an image (Ctrl+O), or drag and drop a file here." : S.error;
        DrawTextW(mdc, msg.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(mdc, oldf);
    }
    BitBlt(hdc, 0, 0, cw, ch, mdc, 0, 0, SRCCOPY);
    SelectObject(mdc, oldm); DeleteObject(mbm); DeleteDC(mdc);
}

static HMENU BuildMenu() {
    HMENU bar = CreateMenu();
    HMENU f = CreatePopupMenu(), v = CreatePopupMenu(), n = CreatePopupMenu(), h = CreatePopupMenu();
    AppendMenuW(f, MF_STRING, ID_OPEN, L"&Open...\tCtrl+O");
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
    AppendMenuW(v, MF_STRING, ID_ROTL, L"Rotate &left\tShift+R");
    AppendMenuW(v, MF_SEPARATOR, 0, nullptr);
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
        case ID_BG:
            S.bgMode = (S.bgMode + 1) % 3;
            if (g_src) { MakeHBitmap(g_src); Refresh(); } else InvalidateRect(S.hwnd, nullptr, FALSE);
            break;
        case ID_ABOUT:
            MessageBoxW(S.hwnd,
                L"Feather Image and Video Viewer 0.1\n\nLightweight viewer that also runs in Windows Safe Mode "
                L"(native Win32, software rendering, no network, no extra dependencies).\n\n"
                L"Formats: JPEG, PNG, BMP, GIF (first frame), TIFF, ICO, EMF, WMF.\n"
                L"Video playback: planned.\n\nDesigned and developed by Akshaya Simha.",
                APP_NAME, MB_OK | MB_ICONINFORMATION);
            break;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: S.hwnd = hwnd; DragAcceptFiles(hwnd, TRUE); return 0;
        case WM_ERASEBKGND: return 1;
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
                case 'B': Command(ID_BG); break;
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
        case WM_LBUTTONDOWN:
            SetFocus(hwnd); SetCapture(hwnd);
            S.dragging = true; S.dragStart = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            S.panStartX = S.panX; S.panStartY = S.panY;
            return 0;
        case WM_MOUSEMOVE:
            if (S.dragging) {
                S.panX = S.panStartX + GET_X_LPARAM(lp) - S.dragStart.x;
                S.panY = S.panStartY + GET_Y_LPARAM(lp) - S.dragStart.y;
                ClampPan();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
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
                                    1100, 750, nullptr, S.menu, hi, nullptr);
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
    }
    GdiplusShutdown(token);
    return 0;
}
