#pragma once
// Pictures in and out, from the system: GDI+ on Windows, ImageIO / Core Graphics on the Mac, libpng on Linux (the engine
// check; PNG only there). Pixels are 4 bytes each, blue first (BGRA, as PDFium renders them), alpha not premultiplied.
#include <string>
#include <vector>

namespace image {

enum class Format { Png, Jpeg };

struct Picture {
    int w = 0, h = 0;
    std::vector<unsigned char> px;   // w * h * 4, rows top first
};

// Encodes pixels (rows `stride` bytes apart). `alpha` false: the fourth byte is ignored. `quality` 1-100 for JPEG.
bool encode(const unsigned char* bgra, int w, int h, int stride, Format f, int quality, bool alpha, std::vector<unsigned char>& out);
bool save(const unsigned char* bgra, int w, int h, int stride, Format f, int quality, bool alpha, const std::wstring& path);
// Reads a picture file (PNG, JPEG, BMP, GIF, TIFF; HEIC too on the Mac). False when it is not one.
bool load(const std::wstring& path, Picture& out);
// The size of a picture file without reading all of it.
bool sizeOf(const std::wstring& path, int& w, int& h);
// Scaled to w2 x h2 with high-quality (bicubic) filtering.
bool scale(const unsigned char* bgra, int w, int h, int stride, int w2, int h2, Picture& out);

} // namespace image
