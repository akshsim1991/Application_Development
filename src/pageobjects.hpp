#pragma once
// Content added to the page itself: watermarks and page numbers (and text boxes).
#include <windows.h>
#include <string>
#include <vector>
#include "document.hpp"

namespace pageobj {

struct WatermarkStyle {
    COLORREF color = RGB(0xE5, 0x39, 0x35);
    int opacityPercent = 30;
    int sizeMode = 0;          // 0 = fit the page, 1 = small, 2 = medium, 3 = large
    bool diagonal = true;
    bool behind = false;       // put it behind the page content
};

// True when every character can be drawn with the built-in Latin fonts.
bool isLatin(const std::wstring& text);

bool addWatermark(Document& d, const std::vector<int>& pages, const std::wstring& text, const WatermarkStyle& style);

struct NumberStyle {
    int format = 0;            // 0 "1", 1 "Page 1", 2 "Page 1 of N", 3 "1 / N"
    int position = 0;          // 0..5: bottom centre/left/right, top centre/left/right
    int startAt = 1;
    double sizePt = 10;
    COLORREF color = RGB(0x30, 0x30, 0x30);
};

bool addPageNumbers(Document& d, const std::vector<int>& pages, const NumberStyle& style);

} // namespace pageobj
