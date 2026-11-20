#include "gfx.h"
#include <gdiplus.h>
#include <cmath>
using namespace Gdiplus;
namespace gfx {
static ULONG_PTR g_tok = 0;
void Init() { if (g_tok) return; GdiplusStartupInput in; GdiplusStartup(&g_tok, &in, nullptr); }
void Shutdown() { if (g_tok) { GdiplusShutdown(g_tok); g_tok = 0; } }
static Color C(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }
static void Path(GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2; if (d > w) d = w; if (d > h) d = h; p.AddArc(x, y, d, d, 180, 90); p.AddArc(x + w - d, y, d, d, 270, 90); p.AddArc(x + w - d, y + h - d, d, d, 0, 90); p.AddArc(x, y + h - d, d, d, 90, 90); p.CloseFigure();
}
void RoundRect(HDC dc, RECT r, int radius, COLORREF top, COLORREF bottom, COLORREF border, int bw) {
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias); float x = (float)r.left + 0.5f, y = (float)r.top + 0.5f, w = (float)(r.right - r.left) - 1, h = (float)(r.bottom - r.top) - 1; GraphicsPath p; Path(p, x, y, w, h, (float)radius);
    LinearGradientBrush b(PointF(x, y), PointF(x, y + h + 1), C(top), C(bottom)); g.FillPath(&b, &p);
    if (border != CLR_INVALID) { Pen pen(C(border), (float)bw); g.DrawPath(&pen, &p); }
}
void Ring(HDC dc, RECT r, int th, COLORREF c, float start, float sweep, bool caps) {
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias); Pen p(C(c), (float)th); if (caps) { p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound); }
    float t = th / 2.0f; g.DrawArc(&p, r.left + t, r.top + t, (float)(r.right - r.left) - th, (float)(r.bottom - r.top) - th, start, sweep);
}
void Disc(HDC dc, RECT r, COLORREF top, COLORREF bottom, COLORREF border, int bw) {
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias); float x = (float)r.left, y = (float)r.top, w = (float)(r.right - r.left - 1), h = (float)(r.bottom - r.top - 1);
    LinearGradientBrush b(PointF(x, y), PointF(x, y + h), C(top), C(bottom)); g.FillEllipse(&b, x, y, w, h); if (border != CLR_INVALID) { Pen p(C(border), (float)bw); g.DrawEllipse(&p, x, y, w, h); }
}
void Toggle(HDC dc, RECT r, bool on, COLORREF onC, COLORREF offC, COLORREF knob) {
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias); float x = (float)r.left, y = (float)r.top, w = (float)(r.right - r.left), h = (float)(r.bottom - r.top); GraphicsPath p; Path(p, x, y, w, h, h / 2); SolidBrush b(C(on ? onC : offC)); g.FillPath(&b, &p);
    SolidBrush k(C(knob)); float d = h - 6; g.FillEllipse(&k, on ? x + w - d - 3 : x + 3, y + 3, d, d);
}
void Icon(HDC dc, int kind, RECT r, COLORREF col, float sw) {
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias); float x = (float)r.left, y = (float)r.top, s = (float)(r.right - r.left); Pen p(C(col), sw); p.SetLineJoin(LineJoinRound); p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound); SolidBrush fill(C(col));
    auto L = [&](float a, float b, float c2, float d) { g.DrawLine(&p, x + a * s, y + b * s, x + c2 * s, y + d * s); };
    auto E = [&](float a, float b, float c2, float d) { g.DrawEllipse(&p, x + a * s, y + b * s, c2 * s, d * s); };
    auto R = [&](float a, float b, float c2, float d, float rad) { GraphicsPath gp; Path(gp, x + a * s, y + b * s, c2 * s, d * s, rad * s); g.DrawPath(&p, &gp); };
    auto A = [&](float a, float b, float c2, float d, float st, float sw2) { g.DrawArc(&p, x + a * s, y + b * s, c2 * s, d * s, st, sw2); };
    auto Dot = [&](float a, float b, float rr) { g.FillEllipse(&fill, x + (a - rr) * s, y + (b - rr) * s, 2 * rr * s, 2 * rr * s); };
    switch (kind) {
    case I_CARE: E(0.1f, 0.1f, 0.8f, 0.8f); E(0.3f, 0.3f, 0.4f, 0.4f); break;
    case I_CLEAN: case I_JUNK: { PointF pts[8] = {{x + 0.5f * s, y + 0.05f * s}, {x + 0.62f * s, y + 0.38f * s}, {x + 0.95f * s, y + 0.5f * s}, {x + 0.62f * s, y + 0.62f * s}, {x + 0.5f * s, y + 0.95f * s}, {x + 0.38f * s, y + 0.62f * s}, {x + 0.05f * s, y + 0.5f * s}, {x + 0.38f * s, y + 0.38f * s}}; g.DrawPolygon(&p, pts, 8); break; }
    case I_SPEED: A(0.08f, 0.1f, 0.84f, 0.84f, 150, 240); L(0.5f, 0.55f, 0.72f, 0.32f); Dot(0.5f, 0.55f, 0.06f); break;
    case I_SOFT: R(0.12f, 0.35f, 0.76f, 0.52f, 0.08f); L(0.5f, 0.12f, 0.5f, 0.6f); L(0.34f, 0.28f, 0.5f, 0.12f); L(0.66f, 0.28f, 0.5f, 0.12f); break;
    case I_TOOLBOX: R(0.08f, 0.3f, 0.84f, 0.55f, 0.08f); R(0.35f, 0.14f, 0.3f, 0.16f, 0.04f); L(0.08f, 0.55f, 0.92f, 0.55f); break;
    case I_ACTION: Dot(0.3f, 0.3f, 0.09f); Dot(0.7f, 0.3f, 0.09f); Dot(0.3f, 0.7f, 0.09f); Dot(0.7f, 0.7f, 0.09f); break;
    case I_SETTINGS: case I_SERVICES: { E(0.3f, 0.3f, 0.4f, 0.4f); for (int i = 0; i < 8; ++i) { float a = i * 0.785398f; L(0.5f + 0.27f * cosf(a), 0.5f + 0.27f * sinf(a), 0.5f + 0.42f * cosf(a), 0.5f + 0.42f * sinf(a)); } E(0.18f, 0.18f, 0.64f, 0.64f); break; }
    case I_STARTUP: A(0.15f, 0.2f, 0.7f, 0.7f, -55, 290); L(0.5f, 0.08f, 0.5f, 0.5f); break;
    case I_DRIVER: R(0.25f, 0.25f, 0.5f, 0.5f, 0.05f); for (int i = 0; i < 3; ++i) { float q = 0.35f + i * 0.15f; L(q, 0.1f, q, 0.25f); L(q, 0.75f, q, 0.9f); L(0.1f, q, 0.25f, q); L(0.75f, q, 0.9f, q); } break;
    case I_REGISTRY: R(0.12f, 0.12f, 0.32f, 0.32f, 0.04f); R(0.56f, 0.12f, 0.32f, 0.32f, 0.04f); R(0.12f, 0.56f, 0.32f, 0.32f, 0.04f); R(0.56f, 0.56f, 0.32f, 0.32f, 0.04f); break;
    case I_LARGE: E(0.1f, 0.1f, 0.55f, 0.55f); L(0.55f, 0.55f, 0.9f, 0.9f); break;
    case I_DUP: R(0.1f, 0.1f, 0.55f, 0.55f, 0.06f); R(0.35f, 0.35f, 0.55f, 0.55f, 0.06f); break;
    case I_EMPTY: { PointF pts[7] = {{x + 0.08f * s, y + 0.25f * s}, {x + 0.38f * s, y + 0.25f * s}, {x + 0.46f * s, y + 0.35f * s}, {x + 0.92f * s, y + 0.35f * s}, {x + 0.92f * s, y + 0.8f * s}, {x + 0.08f * s, y + 0.8f * s}, {x + 0.08f * s, y + 0.25f * s}}; g.DrawLines(&p, pts, 7); break; }
    case I_DEEP: E(0.1f, 0.1f, 0.8f, 0.8f); E(0.3f, 0.3f, 0.4f, 0.4f); L(0.5f, 0.5f, 0.85f, 0.25f); break;
    case I_TRASH: L(0.15f, 0.25f, 0.85f, 0.25f); L(0.38f, 0.25f, 0.38f, 0.12f); L(0.38f, 0.12f, 0.62f, 0.12f); L(0.62f, 0.12f, 0.62f, 0.25f); L(0.24f, 0.25f, 0.3f, 0.9f); L(0.76f, 0.25f, 0.7f, 0.9f); L(0.3f, 0.9f, 0.7f, 0.9f); L(0.5f, 0.4f, 0.5f, 0.75f); break;
    case I_CHECK: E(0.06f, 0.06f, 0.88f, 0.88f); L(0.28f, 0.52f, 0.45f, 0.68f); L(0.45f, 0.68f, 0.74f, 0.34f); break;
    case I_OVERVIEW: R(0.12f, 0.12f, 0.35f, 0.35f, 0.06f); R(0.53f, 0.12f, 0.35f, 0.35f, 0.06f); R(0.12f, 0.53f, 0.35f, 0.35f, 0.06f); R(0.53f, 0.53f, 0.35f, 0.35f, 0.06f); break;
    case I_UPDATE: A(0.12f, 0.12f, 0.76f, 0.76f, 20, 280); L(0.8f, 0.1f, 0.84f, 0.3f); L(0.84f, 0.3f, 0.64f, 0.28f); break;
    }
}
}
