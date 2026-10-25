#include "sigpad.hpp"
#include <objbase.h>
#include <propidl.h>
#include <algorithm>
using std::min; using std::max;
#include <gdiplus.h>
#include <vector>
#include <memory>

namespace sigpad {
namespace {

bool encoderFor(const wchar_t* mime, CLSID& out) {
    UINT n = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&n, &size);
    if (!size) return false;
    std::vector<unsigned char> buf(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(n, size, codecs);
    for (UINT i = 0; i < n; ++i) if (wcscmp(codecs[i].MimeType, mime) == 0) { out = codecs[i].Clsid; return true; }
    return false;
}

struct Gdi {
    ULONG_PTR token = 0;
    bool ok = false;
    Gdi() { Gdiplus::GdiplusStartupInput in; ok = Gdiplus::GdiplusStartup(&token, &in, nullptr) == Gdiplus::Ok; }
    ~Gdi() { if (ok) Gdiplus::GdiplusShutdown(token); }
};

const Gdiplus::Color kInk(255, 0x14, 0x24, 0x6B);

bool savePng(Gdiplus::Bitmap& bmp, const std::wstring& path) {
    CLSID png;
    return encoderFor(L"image/png", png) && bmp.Save(path.c_str(), &png, nullptr) == Gdiplus::Ok;
}

} // namespace

bool fromText(const std::wstring& name, const std::wstring& pngPath) {
    if (name.empty()) return false;
    Gdi g;
    if (!g.ok) return false;
    static const wchar_t* fonts[] = {L"Segoe Script", L"Lucida Handwriting", L"Brush Script MT", L"Segoe UI"};
    std::unique_ptr<Gdiplus::FontFamily> fam;
    for (const wchar_t* f : fonts) {
        auto c = std::make_unique<Gdiplus::FontFamily>(f);
        if (c->IsAvailable()) { fam = std::move(c); break; }
    }
    if (!fam) fam = std::make_unique<Gdiplus::FontFamily>(L"Arial");
    Gdiplus::Font font(fam.get(), 72, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Bitmap probe(8, 8, PixelFormat32bppARGB);
    Gdiplus::Graphics pg(&probe);
    Gdiplus::RectF box;
    pg.MeasureString(name.c_str(), -1, &font, Gdiplus::PointF(0, 0), &box);
    const int w = static_cast<int>(box.Width) + 40, h = static_cast<int>(box.Height) + 24;
    Gdiplus::Bitmap bmp(w, h, PixelFormat32bppARGB);
    Gdiplus::Graphics gr(&bmp);
    gr.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    gr.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
    gr.Clear(Gdiplus::Color(0, 255, 255, 255));
    Gdiplus::SolidBrush ink(kInk);
    gr.DrawString(name.c_str(), -1, &font, Gdiplus::PointF(16, 8), &ink);
    return savePng(bmp, pngPath);
}

namespace {

struct Pad {
    std::vector<std::vector<POINT>> strokes;
    bool down = false, done = false, accepted = false;
    HWND wnd = nullptr;
    RECT area{16, 16, 536, 236};
};
enum { ID_CLEAR = 1, ID_USE, ID_CANCEL };

LRESULT CALLBACK padProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    auto* p = reinterpret_cast<Pad*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    switch (m) {
        case WM_NCCREATE:
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            HBRUSH bg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
            FillRect(dc, &ps.rcPaint, bg); DeleteObject(bg);
            HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(dc, &p->area, white); DeleteObject(white);
            HPEN frame = CreatePen(PS_SOLID, 1, RGB(0x90, 0x90, 0x90));
            HGDIOBJ op = SelectObject(dc, frame), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, p->area.left, p->area.top, p->area.right, p->area.bottom);
            HPEN guide = CreatePen(PS_DOT, 1, RGB(0xC0, 0xC0, 0xC0));
            SelectObject(dc, guide);
            MoveToEx(dc, p->area.left + 20, p->area.bottom - 50, nullptr); LineTo(dc, p->area.right - 20, p->area.bottom - 50);
            HPEN ink = CreatePen(PS_SOLID, 3, RGB(0x14, 0x24, 0x6B));
            SelectObject(dc, ink);
            for (const auto& s : p->strokes) {
                if (s.empty()) continue;
                MoveToEx(dc, s[0].x, s[0].y, nullptr);
                if (s.size() == 1) LineTo(dc, s[0].x + 1, s[0].y);
                for (size_t i = 1; i < s.size(); ++i) LineTo(dc, s[i].x, s[i].y);
            }
            SelectObject(dc, ob); SelectObject(dc, op);
            DeleteObject(frame); DeleteObject(guide); DeleteObject(ink);
            SetBkMode(dc, TRANSPARENT);
            SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
            RECT hint{p->area.left, p->area.bottom + 4, p->area.right, p->area.bottom + 22};
            DrawTextW(dc, L"Draw your signature with the mouse.", -1, &hint, DT_LEFT | DT_SINGLELINE);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            POINT pt{static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l))};
            if (!PtInRect(&p->area, pt)) return 0;
            p->down = true; SetCapture(h);
            p->strokes.push_back({pt});
            InvalidateRect(h, &p->area, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (p->down) {
                POINT pt{static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l))};
                pt.x = std::clamp<LONG>(pt.x, p->area.left, p->area.right); pt.y = std::clamp<LONG>(pt.y, p->area.top, p->area.bottom);
                p->strokes.back().push_back(pt);
                InvalidateRect(h, &p->area, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (p->down) { p->down = false; ReleaseCapture(); }
            return 0;
        case WM_COMMAND:
            if (LOWORD(w) == ID_CLEAR) { p->strokes.clear(); InvalidateRect(h, nullptr, FALSE); }
            else if (LOWORD(w) == ID_USE) { if (!p->strokes.empty()) { p->accepted = true; p->done = true; } }
            else if (LOWORD(w) == ID_CANCEL) p->done = true;
            return 0;
        case WM_CLOSE: p->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

bool draw(HWND parent, HINSTANCE inst, const std::wstring& pngPath) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = padProc; wc.hInstance = inst; wc.hCursor = LoadCursor(nullptr, IDC_CROSS);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1); wc.lpszClassName = L"FeatherSigPad";
        RegisterClassExW(&wc);
        registered = true;
    }
    Pad pad;
    RECT r{0, 0, 552, 310};
    AdjustWindowRect(&r, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE);
    RECT pr; GetWindowRect(parent, &pr);
    const int w = r.right - r.left, h = r.bottom - r.top;
    HWND wnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FeatherSigPad", L"Draw your signature", WS_POPUP | WS_CAPTION | WS_SYSMENU,
                               (pr.left + pr.right - w) / 2, (pr.top + pr.bottom - h) / 2, w, h, parent, nullptr, inst, &pad);
    if (!wnd) return false;
    pad.wnd = wnd;
    auto button = [&](const wchar_t* t, int id, int x, int wd) {
        HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP, x, 262, wd, 30, wnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
        SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    };
    button(L"Clear", ID_CLEAR, 16, 90);
    button(L"Use this signature", ID_USE, 318, 140);
    button(L"Cancel", ID_CANCEL, 466, 70);
    EnableWindow(parent, FALSE);
    ShowWindow(wnd, SW_SHOW);
    MSG msg;
    while (!pad.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(wnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(wnd);
    SetForegroundWindow(parent);
    if (!pad.accepted) return false;

    // crop to the ink and write a transparent PNG
    LONG minx = 1 << 20, miny = 1 << 20, maxx = -1, maxy = -1;
    for (const auto& s : pad.strokes) for (const POINT& p : s) { minx = std::min(minx, p.x); miny = std::min(miny, p.y); maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y); }
    const int pad2 = 8, sc = 2;
    const int bw = (maxx - minx + 2 * pad2) * sc, bh = (maxy - miny + 2 * pad2) * sc;
    Gdi g;
    if (!g.ok) return false;
    Gdiplus::Bitmap bmp(bw, bh, PixelFormat32bppARGB);
    Gdiplus::Graphics gr(&bmp);
    gr.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    gr.Clear(Gdiplus::Color(0, 255, 255, 255));
    Gdiplus::Pen pen(kInk, 3.0f * sc);
    pen.SetLineJoin(Gdiplus::LineJoinRound); pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
    for (const auto& s : pad.strokes) {
        std::vector<Gdiplus::Point> pts;
        for (const POINT& p : s) pts.push_back(Gdiplus::Point((p.x - minx + pad2) * sc, (p.y - miny + pad2) * sc));
        if (pts.size() == 1) gr.DrawEllipse(&pen, pts[0].X, pts[0].Y, 2, 2);
        else gr.DrawLines(&pen, pts.data(), static_cast<INT>(pts.size()));
    }
    return savePng(bmp, pngPath);
}

} // namespace sigpad
