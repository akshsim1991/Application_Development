#pragma once
#include <string>
#include "document.hpp"

namespace pageobj {

// Places a picture file (PNG, JPEG, BMP, GIF or TIFF) on a page with its top-left corner at the given page point.
// The width is the picture's natural size at 96 dpi, limited to half the page width. One undo step.
bool addPicture(Document& d, int page, double atX, double atY, const std::wstring& path, std::wstring& error, double forceWidthPt = 0);
// Pixel size of a picture file.
bool imageSize(const std::wstring& path, int& w, int& h);
// Puts a rectangle of a page (page coordinates) on the clipboard as a picture.
bool copyAreaToClipboard(Document& d, int page, double left, double bottom, double right, double top, void* ownerWindow, int dpi = 200);

// Puts a page on the clipboard as a picture.
bool copyPageToClipboard(Document& d, int page, void* ownerWindow, int dpi = 150);

} // namespace pageobj
