#pragma once
// The fonts a PDF uses (name, type, embedded or not, pages), and which pages have no text at all (scans that need OCR).
#include <windows.h>
#include <string>
#include <vector>
#include "document.hpp"

namespace fontlist {

struct Font {
    std::wstring name;            // without the subset prefix
    std::wstring type;            // Type 1, TrueType, Type 0 (CID), Type 3 ...
    bool embedded = false, subset = false;
    int visibleRuns = 0, invisibleRuns = 0;   // text objects drawn / not drawn (an OCR text layer is invisible)
    std::vector<int> pages;       // 0-based, ascending
};

struct Report {
    std::vector<Font> fonts;
    std::vector<int> pagesWithoutText;        // nothing to search: scanned pages, OCR finds their words
    std::vector<int> pagesInvisibleOnly;      // only invisible text: scanned pages that already had OCR
    int pageCount = 0;
};

Report build(Document& d);

// "1-3, 7" for 0-based pages
std::wstring pageList(const std::vector<int>& pages);
// The report as text (tab-separated table plus the page notes).
std::wstring asText(const Report& r);

// Shows the report in a window with a table, Copy and Close.
void show(HWND parent, HINSTANCE inst, const Report& r, const std::wstring& title);

} // namespace fontlist
