#pragma once
#include <windows.h>

// Colours of the window itself (tabs, toolbar, panels). Page colours are a separate setting in the viewer.
struct Palette {
    COLORREF tabBar, tabIdle, tabActive, toolbar, text, textDim, textOff, line, panel, panelDark, hover, accent, accentSoft, view, viewText, thumbText;
    bool dark;
    COLORREF accentText;   // text and icons on accentSoft (the toolbar group of the tool in use)
    COLORREF field;        // inside the page and zoom boxes
};

// Light: a pale title row over a white toolbar. Dark: the title row darkest, the toolbar a step lighter, the pages' surround
// in between; text near white, disabled controls still readable (about 4:1 against the toolbar).
inline Palette lightPalette() {
    return {RGB(0xE4, 0xE6, 0xE9), RGB(0xD5, 0xD8, 0xDC), RGB(0xFB, 0xFB, 0xFC), RGB(0xFB, 0xFB, 0xFC), RGB(0x1B, 0x1D, 0x21), RGB(0x55, 0x5A, 0x62),
            RGB(0x94, 0x99, 0xA1), RGB(0xD3, 0xD6, 0xDB), RGB(0xF3, 0xF4, 0xF6), RGB(0xE9, 0xEB, 0xEE), RGB(0xE6, 0xE9, 0xEE), RGB(0x1F, 0x6F, 0xEB),
            RGB(0xDB, 0xE8, 0xFD), RGB(0xE5, 0xE7, 0xEA), RGB(0x4A, 0x4F, 0x57), RGB(0x30, 0x30, 0x30), false, RGB(0x12, 0x4F, 0xB8), RGB(0xFF, 0xFF, 0xFF)};
}
inline Palette darkPalette() {
    return {RGB(0x15, 0x16, 0x18), RGB(0x2A, 0x2C, 0x30), RGB(0x24, 0x26, 0x29), RGB(0x24, 0x26, 0x29), RGB(0xF1, 0xF2, 0xF4), RGB(0xBD, 0xC1, 0xC7),
            RGB(0x80, 0x85, 0x8C), RGB(0x3A, 0x3D, 0x42), RGB(0x1E, 0x20, 0x23), RGB(0x1C, 0x1D, 0x20), RGB(0x34, 0x37, 0x3C), RGB(0x4C, 0x9A, 0xFF),
            RGB(0x1E, 0x3A, 0x63), RGB(0x1A, 0x1B, 0x1E), RGB(0xBD, 0xC1, 0xC7), RGB(0xD6, 0xD8, 0xDC), true, RGB(0xD6, 0xE6, 0xFF), RGB(0x2E, 0x31, 0x35)};
}
inline Palette& pal() { static Palette p = lightPalette(); return p; }

// Paints into an off-screen bitmap and copies it to the window in one go, so nothing flickers.
struct PaintBuf {
    HDC target, dc;
    HBITMAP bmp, old;
    RECT clip;
    PaintBuf(HDC t, const RECT& client, const RECT& clipRect) : target(t), clip(clipRect) {
        dc = CreateCompatibleDC(t);
        bmp = CreateCompatibleBitmap(t, client.right > 0 ? client.right : 1, client.bottom > 0 ? client.bottom : 1);
        old = static_cast<HBITMAP>(SelectObject(dc, bmp));
        SelectClipRgn(dc, nullptr);
    }
    void done() {
        BitBlt(target, clip.left, clip.top, clip.right - clip.left, clip.bottom - clip.top, dc, clip.left, clip.top, SRCCOPY);
        SelectObject(dc, old);
        DeleteObject(bmp);
        DeleteDC(dc);
    }
};
