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
bool replaceText(Document& d, int page, int objectIndex, const std::wstring& text, bool track = true);

// Replaces `find` by `with` inside every text object of the document (a match must lie within one object).
// One undo step. Returns the number of replacements; `pages` lists the pages that changed.
int replaceAll(Document& d, const std::wstring& find, const std::wstring& with, bool matchCase, std::vector<int>& pages);

// ---- tables drawn on the page (kept recognisable through a mark, so they can be edited or deleted later) ----
struct TableSpec {
    int rows = 3, cols = 3;
    double colWidth = 90, rowHeight = 24, fontSize = 11;   // points
    bool header = true;
    std::vector<std::wstring> cells;                        // rows * cols, row by row
};
struct TableHit { bool found = false; int page = 0; std::string id; TableSpec spec; double left = 0, top = 0; };   // left/top: page coordinates of the top-left corner
bool addTable(Document& d, int page, double atX, double atY, const TableSpec& spec, bool track = true);
TableHit tableAt(Document& d, int page, double x, double y);
bool removeTable(Document& d, int page, const std::string& id, bool track = true);

// Puts the baseline-left origin of a text object at (originX, originY) measured in points from the top-left of the
// page as displayed, stretching it horizontally by `sx`. Used for the invisible text layer of recognised text.
bool placeScaled(FPDF_PAGE page, FPDF_PAGEOBJECT obj, double visW, double visH, double originX, double originY, double sx);
// A text object with a standard font (for measuring or adding); the caller owns it until it is inserted.
FPDF_PAGEOBJECT newText(Document& d, const char* font, double size, const std::wstring& text, COLORREF color, int alphaPercent);

// A boxed stamp (for example APPROVED) centred on a page point, drawn in bold with a border of the same colour.
bool addStamp(Document& d, int page, double centreX, double centreY, const std::wstring& text, COLORREF color);

} // namespace pageobj
