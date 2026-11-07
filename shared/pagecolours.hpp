#pragma once
// Page colours for reading: night mode turns the page into soft light-on-dark; dimmed turns the white into a warm light
// grey; blackboard makes the page pure black (AMOLED) with white text, either everything inverted, or with pictures and
// coloured text, lines and highlights in their own colours. Works on rendered pages in PDFium's BGRx / BGRA layout
// (4 bytes a pixel, blue first). Used by both apps.
#include <algorithm>
#include <cstddef>
#include <vector>
#include "document.hpp"
#include "pdfium_api.hpp"

namespace pagecolours {

enum class Mode { Normal, Dark, Dimmed, BlackboardInverted, BlackboardOriginal };

// A rectangle of the bitmap (pixels, x0/y0 included, x1/y1 not) left exactly as drawn: a picture on the page.
struct Box { int x0, y0, x1, y1; };

inline void apply(Mode m, void* bits, int w, int h, int stride, const std::vector<Box>& keep = {}) {
    if (m == Mode::Normal) return;
    for (int y = 0; y < h; ++y) {
        unsigned char* p = static_cast<unsigned char*>(bits) + static_cast<size_t>(y) * stride;
        if (m == Mode::Dark) {
            for (int x = 0; x < w; ++x, p += 4)
                for (int k = 0; k < 3; ++k) p[k] = static_cast<unsigned char>(0x1C + (255 - p[k]) * (0xE4 - 0x1C) / 255);
        } else if (m == Mode::Dimmed) {
            for (int x = 0; x < w; ++x, p += 4) {
                p[0] = static_cast<unsigned char>(p[0] * 205 / 255);   // blue
                p[1] = static_cast<unsigned char>(p[1] * 232 / 255);   // green
                p[2] = static_cast<unsigned char>(p[2] * 240 / 255);   // red
            }
        } else if (m == Mode::BlackboardInverted) {
            // everything as a negative: white paper pure black, black text white, colours and pictures inverted
            for (int x = 0; x < w; ++x, p += 4)
                for (int k = 0; k < 3; ++k) p[k] = static_cast<unsigned char>(255 - p[k]);
        } else {
            // A pixel is a colour mixed with white paper: the white part becomes black and the grey part flips (black
            // text white, grey grey), while the colour itself stays: red stays red, a light tint around red text becomes
            // a dark tint on the black, dark blue becomes a lighter blue. Pictures are not touched.
            std::vector<std::pair<int, int>> spans;   // the parts of this row inside pictures
            for (const Box& b : keep)
                if (y >= b.y0 && y < b.y1 && b.x1 > 0 && b.x0 < w) spans.push_back({std::max(0, b.x0), std::min(w, b.x1)});
            for (int x = 0; x < w; ++x, p += 4) {
                bool inside = false;
                for (const auto& s : spans) if (x >= s.first && x < s.second) { inside = true; break; }
                if (inside) continue;
                const int lo = std::min({p[0], p[1], p[2]}), hi = std::max({p[0], p[1], p[2]});
                for (int k = 0; k < 3; ++k) p[k] = static_cast<unsigned char>(p[k] - lo + (255 - hi));
            }
        }
    }
}

// Where the pictures of `page` are in a bitmap that has the page drawn at (startX, startY) with size (sizeX, sizeY), as
// Document::renderPage draws it. Pictures inside a group (a form object) keep the whole group. A picture covering most of
// the page is a scanned page, not a picture on it: it is left out, so a scan turns black like any other page.
inline std::vector<Box> pictureBoxes(Document& d, int page, int startX, int startY, int sizeX, int sizeY) {
    std::vector<Box> out;
    const auto& api = pdfium::api();
    FPDF_PAGE pg = d.page(page);
    if (!pg) return out;
    auto hasImage = [&](FPDF_PAGEOBJECT o, auto& self, int depth) -> bool {
        const int t = api.FPDFPageObj_GetType(o);
        if (t == FPDF_PAGEOBJ_IMAGE) return true;
        if (t != FPDF_PAGEOBJ_FORM || depth > 6) return false;
        const int n = api.FPDFFormObj_CountObjects(o);
        for (int i = 0; i < n; ++i) if (self(api.FPDFFormObj_GetObject(o, static_cast<unsigned long>(i)), self, depth + 1)) return true;
        return false;
    };
    const float pageArea = api.FPDF_GetPageWidthF(pg) * api.FPDF_GetPageHeightF(pg);
    const int n = api.FPDFPage_CountObjects(pg);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o || !hasImage(o, hasImage, 0)) continue;
        float l, b, r, t;
        if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        if (pageArea > 0 && (r - l) * (t - b) >= 0.7f * pageArea) continue;   // a scanned page
        int x1, y1, x2, y2;
        api.FPDF_PageToDevice(pg, startX, startY, sizeX, sizeY, 0, l, t, &x1, &y1);
        api.FPDF_PageToDevice(pg, startX, startY, sizeX, sizeY, 0, r, b, &x2, &y2);
        out.push_back({std::min(x1, x2), std::min(y1, y2), std::max(x1, x2), std::max(y1, y2)});   // the edge pixels are paper too
    }
    return out;
}

} // namespace pagecolours
