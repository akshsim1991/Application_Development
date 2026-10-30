#pragma once
// The Edit table window: the table as a grid to type in, with buttons for rows, columns, merging, alignment, text style,
// cell colour, lines, and the table's place and size on the page.
#include <windows.h>
#include "tables.hpp"

namespace tableeditor {

enum class Result { Cancel, Apply, Delete };

// Edits `t` (only changed when the result is Apply). `pageW` / `pageH`: the page size in points, for the position fields.
Result run(HWND parent, HINSTANCE inst, tables::Table& t, double pageW, double pageH, int dpi);

} // namespace tableeditor
