#pragma once
// The toolbar icons: outline drawings on a 20 x 20 grid, anti-aliased (GDI+), in one colour.
#include <windows.h>

namespace icons {

// Same order as the toolbar's icon numbers.
enum { None, Open, Save, Undo, Redo, Edit, Annotate, Comment, Geometry, SideBySide, Prev, Next, ZoomOut, ZoomIn, Search, More };

void draw(HDC dc, const RECT& r, int icon, COLORREF colour);

} // namespace icons
