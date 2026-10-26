#pragma once
// The start page, shown in place of the pages while no document is open: an Open PDF button inside a drop area, and the
// recently opened files. Drawn in the window's theme colours and scaled to the screen's dots per inch.
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

namespace startpage {

struct Actions {
    std::function<void()> open;                       // the Open PDF button or a click in the drop area
    std::function<void(size_t)> openRecent;           // a recent file (index into the list given to setRecent)
    std::function<void(size_t)> forget;               // right-click > Remove from the list
    std::function<void()> clearAll;                   // the "Clear list" link
};

void registerClass(HINSTANCE inst);
HWND create(HWND parent, HINSTANCE inst, Actions actions);
void setRecent(HWND w, const std::vector<std::wstring>& paths);   // full paths, most recent first
void setDpi(HWND w, int dpi);                                       // fonts and sizes follow the screen
void themeChanged(HWND w);

} // namespace startpage
