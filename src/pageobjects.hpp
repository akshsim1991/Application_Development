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

// ---- text boxes and editing existing text ----

enum class Family { Sans, Serif, Mono };
struct TextStyle {
    Family family = Family::Sans;
    bool bold = false, italic = false;
    double sizePt = 14;
    COLORREF color = RGB(0x20, 0x20, 0x20);
};

// Adds `text` (several lines allowed) with its top-left corner at `at` (page coordinates).
bool addTextBox(Document& d, int page, double atX, double atY, const std::wstring& text, const TextStyle& st);

struct TextHit {
    int index = -1;             // page object index
    std::wstring text;
    double sizePt = 0;
    COLORREF color = 0;
    bool embeddedFont = false;
    std::string fontName;
    unsigned long flags = 0;
};

// The text object under a page point, preferring the smallest one. index == -1 when there is none.
TextHit textAt(Document& d, int page, double x, double y);
// Replaces the text of an existing object. Characters the original (embedded) font may lack are drawn in a
// similar standard font instead, at the same place and size. An empty `text` deletes the object.
bool replaceText(Document& d, int page, int objectIndex, const std::wstring& text);

} // namespace pageobj
