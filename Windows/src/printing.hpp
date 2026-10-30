#pragma once
// Laying pages out on printer sheets (several per sheet, fitted or actual size), in colour or greyscale, and the
// Print window that shows the same sheets.
#include <windows.h>
#include <string>
#include <vector>
#include "document.hpp"

namespace printing {

struct Layout {
    std::vector<int> pages;      // 0-based, in print order
    int perSheet = 1;            // 1, 2, 4, 6 or 9
    bool actualSize = false;     // 1 per sheet only: 100 % instead of fitted
    bool autoRotate = true;      // 1 per sheet: turn a page that suits the other orientation
    bool grey = false;           // print in shades of grey
};

// The printer's paper, in device pixels: the whole sheet and the part the printer can print on.
struct Paper {
    int physW = 0, physH = 0;    // whole sheet
    int offX = 0, offY = 0;      // where the printable area starts
    int areaW = 0, areaH = 0;    // printable area
    double ppp = 1;              // device pixels per point
};
Paper paperOf(HDC printerDc);

int sheetCount(const Layout& l);
// Draws one sheet on a printer page (origin = printable area). Greyscale sheets are drawn as a picture.
void printSheet(HDC dc, Document& d, const Layout& l, int sheet, const Paper& p);
// Renders one whole sheet (paper included) scaled by `scale` into BGRx pixels, `w` x `h`.
bool renderSheet(Document& d, const Layout& l, int sheet, const Paper& p, double scale, std::vector<unsigned char>& px, int& w, int& h);

bool savePng(const std::vector<unsigned char>& bgrx, int w, int h, const std::wstring& path);

// The Print window: printer and settings on the left, every sheet as it will print on the right, updated as the settings
// change. Prints when the user chooses Print; true when the job was sent.
bool run(HWND parent, HINSTANCE inst, Document& d, int currentPage);

} // namespace printing
