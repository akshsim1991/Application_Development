#pragma once
// Visual comparison of two pages: both are drawn and the difference is shown as an overlay picture
// (red = only in the first document, green = only in the second, grey = the same).
#include <string>
#include "document.hpp"

namespace visualdiff {

struct PageResult {
    bool ok = false;
    long differingPixels = 0;
    double percent = 0;      // of the page area
};

// Writes the overlay of page `pa` of `a` and page `pb` of `b` as a PNG. A page that does not exist in one document is
// compared with a blank page. The second page is scaled to the size of the first.
PageResult overlay(Document& a, int pa, Document& b, int pb, int dpi, const std::wstring& pngPath);

} // namespace visualdiff
