#pragma once
// The theme beyond the main window: menus, dialogs, message boxes and scroll bars drawn in the colours of pal(), so every
// theme reaches them (Windows' own drawing knows only light and, on recent versions, a grey dark).
#include <windows.h>
#include <string>
#include <vector>

namespace themeui {

// The interface font and the screen's dots per inch, set by the main window (and again when they change).
void setFont(HFONT font, HFONT bold);
void setDpi(int dpi);
// Called after pal() changes: Windows' own parts (menu frames, common dialogs, scroll bars of system controls) follow
// light or dark where Windows allows it.
void paletteChanged(HWND mainWindow);

// ---- menus ----
// Makes every item of `m` (and its submenus) drawn by the theme; release() after the menu has closed.
void prepareMenu(HMENU m);
void releaseMenus();
// For WM_MEASUREITEM / WM_DRAWITEM of the window that owns the menu: true when the item was a menu item handled here.
bool measureMenu(MEASUREITEMSTRUCT* mi);
bool drawMenu(DRAWITEMSTRUCT* di);

// ---- dialogs ----
// Colours a dialog-like window and its controls (labels, boxes, lists, buttons, check boxes) from the palette. Push
// buttons are drawn by the theme; call once after the controls are made.
void themeDialog(HWND dlg);
// The brush and colours for WM_CTLCOLOR* (for windows that handle the message themselves).
LRESULT ctlColour(UINT msg, HDC dc, HWND ctl);
// Draws a push button made BS_OWNERDRAW (themeDialog does this for its buttons).
void drawButton(const DRAWITEMSTRUCT* di, bool isDefault);

// MessageBoxW in the theme: MB_OK, MB_OKCANCEL, MB_YESNO, MB_YESNOCANCEL, MB_RETRYCANCEL; the four icons; MB_DEFBUTTON2/3.
int messageBox(HWND owner, const wchar_t* text, const wchar_t* caption, UINT flags);
// A message with buttons of one's own; returns the index of the button chosen, or `cancelIndex` for Esc / close.
int choiceBox(HWND owner, const wchar_t* caption, const std::wstring& text, const std::vector<std::wstring>& buttons,
              int defaultIndex, int cancelIndex, UINT icon = MB_ICONINFORMATION);

// ---- scroll bars ----
// A window's own scroll bars (WS_VSCROLL / WS_HSCROLL) drawn flat in the theme, with a dragged thumb sending
// SB_THUMBTRACK / SB_THUMBPOSITION; read the dragged position with trackPos(). Use setScroll*() to change them.
void flatScroll(HWND w);
void setScrollInfo(HWND w, int bar, SCROLLINFO& si);
void setScrollPos(HWND w, int bar, int pos);
int trackPos(HWND w, int bar);

} // namespace themeui
