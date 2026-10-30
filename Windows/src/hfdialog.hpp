#pragma once
// The Delete header and footer window: the page with what was found outlined, before / after, which pages and what to
// delete, the list of what was found (each can be left out), an area to draw by hand, warnings, and Delete.
#include <windows.h>
#include "document.hpp"

namespace hfdialog {

// Shows the window for `d` (opened at `page`). True when something was deleted (as one undoable change).
bool run(HWND parent, HINSTANCE inst, Document& d, int page, int dpi, HFONT font, HFONT bold);

} // namespace hfdialog
