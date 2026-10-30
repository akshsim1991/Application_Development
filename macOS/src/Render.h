#pragma once
// Pages drawn by PDFium into pictures macOS can show (CGImage), with the reading colours applied.
#import <CoreGraphics/CoreGraphics.h>
#include "document.hpp"
#include "pagecolours.hpp"

// Renders the part (rx, ry, rw, rh) of page `page` drawn at pw x ph pixels. Returns a +1 image (CGImageRelease it),
// or nullptr when the page could not be drawn.
CGImageRef FPRenderPage(Document& doc, int page, int pw, int ph, int rx, int ry, int rw, int rh, pagecolours::Mode colours);

// Writes `image` as a PNG file. False when it could not be written.
bool FPWritePNG(CGImageRef image, const char* path);
