#pragma once
// Page colours for reading: night mode turns the page into soft light-on-dark; dimmed turns the white into a warm light
// grey. Works on rendered pages in PDFium's BGRx / BGRA layout (4 bytes a pixel, blue first). Used by both apps.
#include <cstddef>

namespace pagecolours {

enum class Mode { Normal, Dark, Dimmed };

inline void apply(Mode m, void* bits, int w, int h, int stride) {
    if (m == Mode::Normal) return;
    for (int y = 0; y < h; ++y) {
        unsigned char* p = static_cast<unsigned char*>(bits) + static_cast<size_t>(y) * stride;
        if (m == Mode::Dark) {
            for (int x = 0; x < w; ++x, p += 4)
                for (int k = 0; k < 3; ++k) p[k] = static_cast<unsigned char>(0x1C + (255 - p[k]) * (0xE4 - 0x1C) / 255);
        } else {
            for (int x = 0; x < w; ++x, p += 4) {
                p[0] = static_cast<unsigned char>(p[0] * 205 / 255);   // blue
                p[1] = static_cast<unsigned char>(p[1] * 232 / 255);   // green
                p[2] = static_cast<unsigned char>(p[2] * 240 / 255);   // red
            }
        }
    }
}

} // namespace pagecolours
