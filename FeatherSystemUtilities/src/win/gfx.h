// Anti-aliased drawing helpers (GDI+, Windows 7+) for the navy "card" look.
#pragma once
#include <windows.h>
namespace gfx {
enum Ico { I_CARE, I_CLEAN, I_SPEED, I_SOFT, I_TOOLBOX, I_ACTION, I_SETTINGS, I_STARTUP, I_SERVICES, I_DRIVER, I_REGISTRY, I_LARGE, I_DUP, I_EMPTY, I_DEEP, I_TRASH, I_CHECK, I_OVERVIEW, I_UPDATE, I_JUNK };
void Init(); void Shutdown();
void RoundRect(HDC dc, RECT r, int radius, COLORREF top, COLORREF bottom, COLORREF border, int borderW = 1);   // border == CLR_INVALID: none
void Ring(HDC dc, RECT r, int thickness, COLORREF c, float startDeg, float sweepDeg, bool roundCaps = true);
void Disc(HDC dc, RECT r, COLORREF top, COLORREF bottom, COLORREF border = CLR_INVALID, int borderW = 1);
void Icon(HDC dc, int kind, RECT r, COLORREF c, float stroke = 1.8f);
void Toggle(HDC dc, RECT r, bool on, COLORREF onC, COLORREF offC, COLORREF knob);
}
