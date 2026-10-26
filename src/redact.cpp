#include "redact.hpp"
#include "contentgen.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace redact {

int apply(Document& d, int dpi) {
    if (d.redactions.empty()) return 0;
    std::set<int> pages;
    for (const auto& m : d.redactions) pages.insert(m.page);
    const auto& api = pdfium::api();
    d.beginChange();
    int done = 0;
    for (int p : pages) {
        FPDF_PAGE pg = d.page(p);
        if (!pg) continue;
        const double vw = d.pageSize(p).w, vh = d.pageSize(p).h;
        const int w = std::max(1, static_cast<int>(std::lround(vw * dpi / 72.0)));
        const int h = std::max(1, static_cast<int>(std::lround(vh * dpi / 72.0)));
        if (1LL * w * h > 100000000LL) continue;
        std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
        FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRA, px.data(), w * 4);
        if (!bmp) continue;
        api.FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0xFFFFFFFF);
        d.renderOutput(p, bmp, 0, 0, w, h);
        // Black out the marked areas (converted from page space to this bitmap's pixels).
        for (const auto& m : d.redactions) {
            if (m.page != p) continue;
            int x1, y1, x2, y2;
            api.FPDF_PageToDevice(pg, 0, 0, w, h, 0, m.left, m.top, &x1, &y1);
            api.FPDF_PageToDevice(pg, 0, 0, w, h, 0, m.right, m.bottom, &x2, &y2);
            const int pad = 1;
            int l = std::max(0, std::min(x1, x2) - pad), t = std::max(0, std::min(y1, y2) - pad);
            int r = std::min(w, std::max(x1, x2) + pad), b = std::min(h, std::max(y1, y2) + pad);
            if (r > l && b > t) api.FPDFBitmap_FillRect(bmp, l, t, r - l, b - t, 0xFF000000);
        }
        // Remove all original content and annotations from the page.
        while (api.FPDFPage_GetAnnotCount(pg) > 0) {
            if (!api.FPDFPage_RemoveAnnot(pg, api.FPDFPage_GetAnnotCount(pg) - 1)) break;
        }
        while (api.FPDFPage_CountObjects(pg) > 0) {
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, api.FPDFPage_CountObjects(pg) - 1);
            if (!o || !api.FPDFPage_RemoveObject(pg, o)) break;
            api.FPDFPageObj_Destroy(o);
        }
        // The page becomes an upright, unrotated picture of exactly the size it was shown at.
        api.FPDFPage_SetRotation(pg, 0);
        api.FPDFPage_SetMediaBox(pg, 0, 0, static_cast<float>(vw), static_cast<float>(vh));
        api.FPDFPage_SetCropBox(pg, 0, 0, static_cast<float>(vw), static_cast<float>(vh));
        FPDF_PAGEOBJECT img = api.FPDFPageObj_NewImageObj(d.handle());
        bool ok = img && api.FPDFImageObj_SetBitmap(nullptr, 0, img, bmp) &&
                  api.FPDFImageObj_SetMatrix(img, vw, 0, 0, vh, 0, 0);
        api.FPDFBitmap_Destroy(bmp);
        if (!ok) { if (img) api.FPDFPageObj_Destroy(img); continue; }
        api.FPDFPage_InsertObject(pg, img);
        contentgen::generate(d.handle(), pg);
        ++done;
    }
    if (!done) { d.cancelChange(); return 0; }
    d.redactions.clear();
    d.refreshPages();   // cached text of the redacted pages is stale
    return done;
}

int markText(Document& d, const std::wstring& query, bool matchCase) {
    int added = 0;
    const auto& api = pdfium::api();
    for (const TextHit& h : d.find(query, matchCase)) {
        FPDF_TEXTPAGE tp = d.text(h.page);
        if (!tp) continue;
        int n = api.FPDFText_CountRects(tp, h.start, h.count);
        for (int i = 0; i < n; ++i) {
            double l, t, r, b;
            if (api.FPDFText_GetRect(tp, i, &l, &t, &r, &b)) { d.redactions.push_back({h.page, l, b, r, t}); ++added; }
        }
    }
    return added;
}

} // namespace redact

#include "annot.hpp"
#include <cwctype>

namespace redact {

std::vector<std::wstring> collect(Document& d) {
    std::vector<std::wstring> out;
    const auto& api = pdfium::api();
    for (const RedactMark& m : d.redactions) {
        FPDF_TEXTPAGE tp = d.text(m.page);
        if (!tp) continue;
        const int n = api.FPDFText_GetBoundedText(tp, m.left, m.top, m.right, m.bottom, nullptr, 0);
        if (n <= 0) continue;
        std::vector<unsigned short> buf(n + 1);
        api.FPDFText_GetBoundedText(tp, m.left, m.top, m.right, m.bottom, buf.data(), n);
        std::wstring t(buf.begin(), buf.begin() + std::min<size_t>(n, buf.size()));
        t.resize(t.find(L'\0') == std::wstring::npos ? t.size() : t.find(L'\0'));
        for (auto& c : t) if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
        size_t a = t.find_first_not_of(L' '), z = t.find_last_not_of(L' ');
        if (a == std::wstring::npos) continue;
        t = t.substr(a, z - a + 1);
        if (t.size() >= 3 && std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
    }
    return out;
}

namespace {
std::wstring lower(std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(towlower(c)); return s; }
bool contains(const std::wstring& hay, const std::wstring& needle) { return lower(hay).find(lower(needle)) != std::wstring::npos; }
void flatten(std::wstring& t) { for (auto& c : t) if (c == L'\r' || c == L'\n' || c == L'\t') c = L' '; }
void bookmarkTexts(const std::vector<Bookmark>& list, std::vector<std::wstring>& out) {
    for (const auto& b : list) { out.push_back(b.title); bookmarkTexts(b.children, out); }
}
}

Report verify(Document& d, const std::vector<std::wstring>& phrases) {
    Report r;
    r.phrases = static_cast<int>(phrases.size());
    const auto& api = pdfium::api();
    auto note = [&](const std::wstring& phrase, const std::wstring& where) {
        for (const auto& f : r.found) if (f.phrase == phrase && f.where == where) return;
        r.found.push_back({phrase, where});
    };
    // 1. the text of every page, 2. comments and other annotations
    for (int p = 0; p < d.pageCount(); ++p) {
        std::wstring t = d.textRange(p, 0, d.charCount(p));
        flatten(t);
        if (!t.empty()) ++r.textPagesLeft;
        for (const auto& ph : phrases) if (contains(t, ph)) note(ph, L"the text of page " + std::to_wstring(p + 1));
        for (const auto& a : annot::list(d, p)) {
            std::wstring c = a.contents;
            flatten(c);
            for (const auto& ph : phrases) if (contains(c, ph)) note(ph, L"a comment or annotation on page " + std::to_wstring(p + 1));
        }
        ++r.places;
    }
    // 3. bookmarks
    std::vector<std::wstring> marks;
    bookmarkTexts(d.bookmarks(), marks);
    for (const auto& b : marks) for (const auto& ph : phrases) if (contains(b, ph)) note(ph, L"a bookmark");
    ++r.places;
    // 4. document information
    for (const char* key : {"Title", "Author", "Subject", "Keywords", "Creator", "Producer"}) {
        const unsigned long n = api.FPDF_GetMetaText(d.handle(), key, nullptr, 0);
        if (n < 4) continue;
        std::vector<unsigned short> buf(n / 2 + 1);
        api.FPDF_GetMetaText(d.handle(), key, buf.data(), n);
        std::wstring v(buf.begin(), buf.begin() + (n / 2 - 1));
        for (const auto& ph : phrases) if (contains(v, ph)) note(ph, std::wstring(L"the document information (") + std::wstring(key, key + strlen(key)) + L")");
    }
    ++r.places;
    // 5. the raw data of the file as it would be saved
    std::vector<unsigned char> bytes;
    if (d.serialize(bytes)) {
        const std::string raw(bytes.begin(), bytes.end());
        for (const auto& ph : phrases) {
            std::string ascii, be;
            bool plain = true;
            for (wchar_t c : ph) { if (c > 0x7E || c < 0x20) plain = false; ascii += static_cast<char>(c); be += '\0'; be += static_cast<char>(c & 0xFF); }
            auto ci = [](const std::string& h, const std::string& n) {
                return std::search(h.begin(), h.end(), n.begin(), n.end(), [](char a, char b) { return tolower(static_cast<unsigned char>(a)) == tolower(static_cast<unsigned char>(b)); }) != h.end();
            };
            if (plain && (ci(raw, ascii) || raw.find(be) != std::string::npos)) note(ph, L"the file data");
        }
        ++r.places;
    }
    return r;
}

} // namespace redact
