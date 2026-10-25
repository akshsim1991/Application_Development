#pragma once
#include <string>
#include <vector>
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

// ---- changing pictures and other selected objects (each call is one undo step; `objs` are page object indices) ----
// Turns the objects a quarter turn at a time about the centre of their bounding box (as the page is displayed).
bool rotateGroup(Document& d, int page, const std::vector<int>& objs, int quarterTurnsClockwise);
// Mirrors the objects about the centre of their bounding box (left-right when `horizontal`, as the page is displayed).
bool flipGroup(Document& d, int page, const std::vector<int>& objs, bool horizontal);
// Puts the objects in front of (or behind) everything else on the page; `objs` receives their new indices.
bool restackGroup(Document& d, int page, std::vector<int>& objs, bool toFront);
// True when the objects are exactly one picture.
bool isPicture(Document& d, int page, const std::vector<int>& objs);
// Keeps the part of the picture inside the page rectangle. The picture is redrawn at its own resolution.
bool cropPicture(Document& d, int page, int obj, double left, double bottom, double right, double top);
// brightness and contrast: -100..100; grey: drop the colour.
bool adjustPicture(Document& d, int page, int obj, int brightness, int contrast, bool grey);
// Puts another picture file in the same place, fitted inside the old picture's box (aspect ratio kept).
bool replacePicture(Document& d, int page, int obj, const std::wstring& path, std::wstring& error);
// Saves the picture as it appears on the page (turned, mirrored, masked) as a PNG file at its own resolution.
bool savePicture(Document& d, int page, int obj, const std::wstring& pngPath);
// True when the clipboard holds a picture.
bool clipboardHasPicture();
// Places the picture on the clipboard on the page, top-left corner at the page point. One undo step.
bool pastePicture(Document& d, int page, double atX, double atY, std::wstring& error);

} // namespace pageobj
