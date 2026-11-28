#pragma once
// The toolbar icons: outline drawings on a 20 x 20 grid, anti-aliased (GDI+), in one colour.
#include <windows.h>

namespace icons {

// Same order as the toolbar's icon numbers.
enum { None, Open, Save, Undo, Redo, Edit, Annotate, Comment, Geometry, SideBySide, Prev, Next, ZoomOut, ZoomIn, Search, More,
       File, Drop, Plus, ChevronDown, RotateLeft, RotateRight,
       Sidebar, Print, FitWidth, FitPage, FullScreen, Night };

// Drawn `size` pixels square, centred in `r`; the strokes grow with the size, so every icon has the same weight.
void draw(HDC dc, const RECT& r, int icon, COLORREF colour, int size = 24);
// A filled circle with a white cross (a tab's close button), smooth-edged.
void closeBadge(HDC dc, const RECT& r, COLORREF fill);

} // namespace icons
