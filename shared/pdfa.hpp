#pragma once
// Saving a copy as PDF/A-2b (for long-term archiving): every font embedded, an sRGB output intent, XMP metadata
// that names the standard, and nothing PDF/A forbids (scripts, attachments, encryption, unprinted comments).
#include <string>
#include <vector>
#include "document.hpp"

namespace pdfa {

struct Result {
    int pages = 0;
    int fontsReplaced = 0;     // text set again in an embedded font (Arial, Times New Roman or Courier New)
    int picturePages = 0;      // pages kept as a picture with invisible, searchable text
    std::vector<std::wstring> notes;
};

// allPictures: every page becomes a picture with invisible text (looks exactly the same everywhere, larger file).
// Otherwise a page only becomes a picture when its fonts cannot be embedded.
bool convert(Document& src, bool allPictures, std::vector<unsigned char>& out, Result& r, std::wstring& error);

// A small sRGB ICC profile (version 2) for the output intent.
std::string srgbProfile();

} // namespace pdfa
