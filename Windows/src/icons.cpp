#include "icons.hpp"
#include <objbase.h>
#include <algorithm>
using std::min; using std::max;
#include <gdiplus.h>

namespace icons {
namespace {

using namespace Gdiplus;

bool ready() {
    static bool started = false, ok = false;
    if (!started) {   // GDI+ stays up for the life of the program
        started = true;
        GdiplusStartupInput in;
        ULONG_PTR token = 0;
        ok = GdiplusStartup(&token, &in, nullptr) == Ok;
    }
    return ok;
}

// A rounded rectangle as a closed figure.
void roundRect(GraphicsPath& p, REAL x, REAL y, REAL w, REAL h, REAL r) {
    const REAL d = r * 2;
    p.StartFigure();
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

} // namespace

void paint(Graphics& g, int icon, COLORREF colour);
void paintBadge(Graphics& g, COLORREF fill);

// Drawn four times larger into a transparent bitmap, then averaged down and blended in: smooth edges whatever GDI+
// does itself on the target. `painter` draws on a 24-unit square.
template <typename Fn>
void smooth(HDC dc, const RECT& r, int size, Fn painter) {
    if (!ready() || size < 4) return;
    const int S = size, K = 4, B = S * K;
    Bitmap bmp(B, B, PixelFormat32bppPARGB);
    {
        Graphics g(&bmp);
        g.Clear(Color(0, 0, 0, 0));
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        const REAL k = static_cast<REAL>(K) * static_cast<REAL>(S) / 24.0f;   // the 24-unit design grid to pixels
        g.ScaleTransform(k, k);
        painter(g);
    }
    BitmapData bd;
    Rect all(0, 0, B, B);
    if (bmp.LockBits(&all, ImageLockModeRead, PixelFormat32bppPARGB, &bd) != Ok) return;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = S;
    bi.bmiHeader.biHeight = -S;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits) {
        auto* dst = static_cast<unsigned char*>(bits);
        const auto* src = static_cast<const unsigned char*>(bd.Scan0);
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x) {
                unsigned sum[4] = {0, 0, 0, 0};
                for (int dy = 0; dy < K; ++dy) {
                    const unsigned char* row = src + (y * K + dy) * bd.Stride + x * K * 4;
                    for (int dx = 0; dx < K * 4; ++dx) sum[dx & 3] += row[dx];
                }
                for (int ch = 0; ch < 4; ++ch) dst[(y * S + x) * 4 + ch] = static_cast<unsigned char>(sum[ch] / (K * K));   // premultiplied stays premultiplied
            }
        HGDIOBJ old = SelectObject(mem, dib);
        BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        AlphaBlend(dc, (r.left + r.right) / 2 - S / 2, (r.top + r.bottom) / 2 - S / 2, S, S, mem, 0, 0, S, S, bf);
        SelectObject(mem, old);
    }
    if (dib) DeleteObject(dib);
    DeleteDC(mem);
    bmp.UnlockBits(&bd);
}

void paint(Graphics& g, int icon, COLORREF colour) {
    const Color c(255, GetRValue(colour), GetGValue(colour), GetBValue(colour));
    Pen pen(c, 1.6f);
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    SolidBrush fill(c);
    GraphicsPath p;
    switch (icon) {
        case Open:   // an open folder
            p.StartFigure();
            p.AddLine(2.5f, 15.5f, 2.5f, 5.0f);
            p.AddLine(2.5f, 5.0f, 3.5f, 4.0f);
            p.AddLine(3.5f, 4.0f, 7.5f, 4.0f);
            p.AddLine(7.5f, 4.0f, 9.0f, 5.8f);
            p.AddLine(9.0f, 5.8f, 15.0f, 5.8f);
            p.AddLine(15.0f, 5.8f, 15.8f, 6.6f);
            p.AddLine(15.8f, 6.6f, 15.8f, 8.5f);
            g.DrawPath(&pen, &p);
            p.Reset();
            p.StartFigure();
            p.AddLine(2.5f, 15.5f, 5.0f, 8.5f);
            p.AddLine(5.0f, 8.5f, 18.0f, 8.5f);
            p.AddLine(18.0f, 8.5f, 15.5f, 15.5f);
            p.CloseFigure();
            g.DrawPath(&pen, &p);
            break;
        case Save:   // a floppy disk
            p.StartFigure();
            p.AddLine(3.0f, 4.0f, 3.0f, 16.0f);
            p.AddLine(3.0f, 16.0f, 4.0f, 17.0f);
            p.AddLine(4.0f, 17.0f, 16.0f, 17.0f);
            p.AddLine(16.0f, 17.0f, 17.0f, 16.0f);
            p.AddLine(17.0f, 16.0f, 17.0f, 6.0f);
            p.AddLine(17.0f, 6.0f, 14.0f, 3.0f);
            p.AddLine(14.0f, 3.0f, 4.0f, 3.0f);
            p.CloseFigure();
            g.DrawPath(&pen, &p);
            g.DrawRectangle(&pen, 6.5f, 3.0f, 6.0f, 4.0f);
            g.DrawRectangle(&pen, 6.0f, 11.0f, 8.0f, 6.0f);
            break;
        case Undo: case Redo: {   // a hooked arrow; redo is undo mirrored
            if (icon == Redo) { g.TranslateTransform(20, 0); g.ScaleTransform(-1, 1); }
            p.StartFigure();
            p.AddLine(4.0f, 7.5f, 11.5f, 7.5f);
            p.AddArc(7.0f, 7.5f, 9.0f, 9.0f, 270, 180);   // down the right-hand side
            p.AddLine(11.5f, 16.5f, 7.5f, 16.5f);
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 4.0f, 7.5f, 7.5f, 4.0f);
            g.DrawLine(&pen, 4.0f, 7.5f, 7.5f, 11.0f);
            break;
        }
        case Edit: {   // a page with a pencil
            p.StartFigure();
            p.AddLine(11.0f, 17.0f, 3.5f, 17.0f);
            p.AddLine(3.5f, 17.0f, 3.5f, 3.0f);
            p.AddLine(3.5f, 3.0f, 10.0f, 3.0f);
            p.AddLine(10.0f, 3.0f, 13.5f, 6.5f);
            p.AddLine(13.5f, 6.5f, 13.5f, 8.0f);
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 6.0f, 8.0f, 10.0f, 8.0f);
            g.DrawLine(&pen, 6.0f, 11.0f, 9.0f, 11.0f);
            g.DrawLine(&pen, 6.0f, 14.0f, 8.0f, 14.0f);
            GraphicsPath pencil;
            pencil.AddLine(17.5f, 9.0f, 18.8f, 10.3f);
            pencil.AddLine(18.8f, 10.3f, 13.0f, 16.1f);
            pencil.AddLine(13.0f, 16.1f, 11.0f, 16.8f);
            pencil.AddLine(11.0f, 16.8f, 11.7f, 14.8f);
            pencil.CloseFigure();
            g.DrawPath(&pen, &pencil);
            break;
        }
        case Annotate: {   // a pen over a line
            GraphicsPath pn;
            pn.AddLine(13.5f, 3.0f, 17.0f, 6.5f);
            pn.AddLine(17.0f, 6.5f, 8.0f, 15.5f);
            pn.AddLine(8.0f, 15.5f, 4.0f, 16.0f);
            pn.AddLine(4.0f, 16.0f, 4.5f, 12.0f);
            pn.CloseFigure();
            g.DrawPath(&pen, &pn);
            g.DrawLine(&pen, 11.5f, 5.0f, 15.0f, 8.5f);
            g.DrawLine(&pen, 3.0f, 18.5f, 17.0f, 18.5f);
            break;
        }
        case Comment:   // a speech bubble with two lines of text
            roundRect(p, 2.5f, 3.5f, 15.0f, 10.5f, 2.0f);
            g.DrawPath(&pen, &p);
            p.Reset();
            p.StartFigure();
            p.AddLine(6.5f, 14.0f, 5.5f, 17.5f);
            p.AddLine(5.5f, 17.5f, 10.0f, 14.0f);
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 6.0f, 7.5f, 14.0f, 7.5f);
            g.DrawLine(&pen, 6.0f, 10.5f, 11.0f, 10.5f);
            break;
        case Geometry: {   // a set square with ruler ticks
            PointF tri[] = {{3.5f, 3.0f}, {3.5f, 16.5f}, {17.0f, 16.5f}};
            g.DrawPolygon(&pen, tri, 3);
            PointF inner[] = {{7.0f, 9.5f}, {7.0f, 13.0f}, {10.5f, 13.0f}};
            g.DrawPolygon(&pen, inner, 3);
            for (int i = 0; i < 3; ++i) g.DrawLine(&pen, 3.5f, 6.0f + i * 3.0f, 5.0f, 6.0f + i * 3.0f);
            break;
        }
        case SideBySide:   // two panes
            roundRect(p, 2.5f, 3.5f, 6.8f, 13.0f, 1.5f);
            roundRect(p, 10.7f, 3.5f, 6.8f, 13.0f, 1.5f);
            g.DrawPath(&pen, &p);
            break;
        case Prev:
            g.DrawLine(&pen, 12.5f, 4.5f, 7.0f, 10.0f);
            g.DrawLine(&pen, 7.0f, 10.0f, 12.5f, 15.5f);
            break;
        case Next:
            g.DrawLine(&pen, 7.5f, 4.5f, 13.0f, 10.0f);
            g.DrawLine(&pen, 13.0f, 10.0f, 7.5f, 15.5f);
            break;
        case ZoomOut: case ZoomIn: case Search: {   // a magnifying glass, with minus or plus
            g.DrawEllipse(&pen, 2.8f, 2.8f, 11.0f, 11.0f);
            Pen handle(c, 2.4f);
            handle.SetStartCap(LineCapRound);
            handle.SetEndCap(LineCapRound);
            g.DrawLine(&handle, 12.3f, 12.3f, 17.0f, 17.0f);
            if (icon != Search) g.DrawLine(&pen, 5.6f, 8.3f, 11.0f, 8.3f);
            if (icon == ZoomIn) g.DrawLine(&pen, 8.3f, 5.6f, 8.3f, 11.0f);
            break;
        }
        case More:
            for (int i = 0; i < 3; ++i) g.FillEllipse(&fill, 2.6f + i * 6.0f, 8.6f, 2.8f, 2.8f);
            break;
        case File:   // a page with a folded corner and a few lines
            p.StartFigure();
            p.AddLine(11.5f, 2.5f, 4.5f, 2.5f);
            p.AddLine(4.5f, 2.5f, 4.5f, 17.5f);
            p.AddLine(4.5f, 17.5f, 15.5f, 17.5f);
            p.AddLine(15.5f, 17.5f, 15.5f, 6.5f);
            p.CloseFigure();
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 11.5f, 2.5f, 11.5f, 6.5f);
            g.DrawLine(&pen, 11.5f, 6.5f, 15.5f, 6.5f);
            g.DrawLine(&pen, 7.5f, 10.5f, 12.5f, 10.5f);
            g.DrawLine(&pen, 7.5f, 13.5f, 12.5f, 13.5f);
            break;
        case Drop:   // an arrow going down into a tray
            g.DrawLine(&pen, 10.0f, 2.5f, 10.0f, 11.5f);
            g.DrawLine(&pen, 6.5f, 8.0f, 10.0f, 11.5f);
            g.DrawLine(&pen, 13.5f, 8.0f, 10.0f, 11.5f);
            p.StartFigure();
            p.AddLine(2.5f, 11.5f, 2.5f, 16.5f);
            p.AddLine(2.5f, 16.5f, 17.5f, 16.5f);
            p.AddLine(17.5f, 16.5f, 17.5f, 11.5f);
            g.DrawPath(&pen, &p);
            break;
        case Plus:
            g.DrawLine(&pen, 10.0f, 4.0f, 10.0f, 16.0f);
            g.DrawLine(&pen, 4.0f, 10.0f, 16.0f, 10.0f);
            break;
        case ChevronDown:
            g.DrawLine(&pen, 5.5f, 8.0f, 10.0f, 12.5f);
            g.DrawLine(&pen, 10.0f, 12.5f, 14.5f, 8.0f);
            break;
        case RotateLeft: case RotateRight: {   // a page under a curved arrow; left is right mirrored
            if (icon == RotateLeft) { g.TranslateTransform(20, 0); g.ScaleTransform(-1, 1); }
            g.DrawRectangle(&pen, 6.5f, 10.5f, 7.0f, 7.0f);
            p.StartFigure();
            p.AddArc(4.5f, 2.5f, 11.0f, 11.0f, 195.0f, 165.0f);   // over the top, ending on the right, going down
            g.DrawPath(&pen, &p);
            g.DrawLine(&pen, 15.5f, 9.4f, 13.4f, 7.3f);
            g.DrawLine(&pen, 15.5f, 9.4f, 17.4f, 7.1f);
            break;
        }
    }
}

void paintBadge(Graphics& g, COLORREF fillColour) {   // on the 24-unit square: a 23-unit circle, a cross of 10
    SolidBrush fill(Color(255, GetRValue(fillColour), GetGValue(fillColour), GetBValue(fillColour)));
    g.FillEllipse(&fill, 0.5f, 0.5f, 23.0f, 23.0f);
    Pen cross(Color(255, 255, 255, 255), 2.6f);
    cross.SetStartCap(LineCapRound);
    cross.SetEndCap(LineCapRound);
    g.DrawLine(&cross, 7.5f, 7.5f, 16.5f, 16.5f);
    g.DrawLine(&cross, 7.5f, 16.5f, 16.5f, 7.5f);
}

void draw(HDC dc, const RECT& r, int icon, COLORREF colour, int size) {
    if (icon == None) return;
    smooth(dc, r, size, [&](Graphics& g) {
        g.TranslateTransform(2, 2);   // the 20 x 20 grid in the middle
        paint(g, icon, colour);
    });
}

void closeBadge(HDC dc, const RECT& r, COLORREF fillColour) {
    smooth(dc, r, std::min(r.right - r.left, r.bottom - r.top), [&](Graphics& g) { paintBadge(g, fillColour); });
}

} // namespace icons
