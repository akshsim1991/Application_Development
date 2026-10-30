#include "textpdf.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cstring>
#include <cwctype>
#include <map>
#include "pdfium_api.hpp"

namespace textpdf {
namespace {

enum Style { Normal, Bold, Italic, BoldItalic, Code };
struct Run { std::wstring text; Style style; };

const char* fontOf(Style s, bool mono) {
    if (mono || s == Code) return s == Bold || s == BoldItalic ? "Courier-Bold" : "Courier";
    switch (s) { case Bold: return "Helvetica-Bold"; case Italic: return "Helvetica-Oblique"; case BoldItalic: return "Helvetica-BoldOblique"; default: return "Helvetica"; }
}

struct Sink : FPDF_FILEWRITE { std::vector<unsigned char>* out; };
int writeBlock(FPDF_FILEWRITE* self, const void* data, unsigned long size) {
    auto* s = static_cast<Sink*>(self);
    const auto* p = static_cast<const unsigned char*>(data);
    s->out->insert(s->out->end(), p, p + size);
    return 1;
}

// characters the standard fonts can draw (Latin-1 and the extra WinAnsi ones)
bool drawable(wchar_t c) {
    if (c >= 0x20 && c <= 0x7E) return true;
    if (c >= 0xA0 && c <= 0xFF) return true;
    static const wchar_t extra[] = {0x2022, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026, 0x20AC, 0x2122, 0x2020, 0x2021, 0x2030, 0x0152, 0x0153, 0x0160, 0x0161, 0x0178, 0x017D, 0x017E, 0x0192, 0x02C6, 0x02DC, 0x201A, 0x201E, 0x2039, 0x203A};
    return std::find(std::begin(extra), std::end(extra), c) != std::end(extra);
}

class Writer {
public:
    explicit Writer(int& replaced) : replaced_(replaced) {
        const auto& api = pdfium::api();
        doc_ = api.FPDF_CreateNewDocument();
    }
    ~Writer() { if (page_) close(); if (doc_) pdfium::api().FPDF_CloseDocument(doc_); }
    bool ok() const { return doc_ != nullptr; }

    static constexpr double W = 595, H = 842, L = 56, R = 56, T = 64, B = 64;

    double width(Style st, bool mono, double size, const std::wstring& s) {
        if (s.empty()) return 0;
        const char* f = fontOf(st, mono);
        auto key = std::string(f) + "|" + std::string(s.begin(), s.end());
        auto it = cache_.find(key);
        double w1000;
        if (it != cache_.end()) w1000 = it->second;
        else {
            // the advance width: "H" + s + "H" minus "HH", so side bearings and spaces count exactly
            const auto& api = pdfium::api();
            auto ink = [&](const std::wstring& t) {
                FPDF_PAGEOBJECT o = api.FPDFPageObj_NewTextObj(doc_, f, 1000.0f);
                std::vector<unsigned short> u(t.begin(), t.end());
                u.push_back(0);
                api.FPDFText_SetText(o, u.data());
                float l, b, r, tt;
                double v = api.FPDFPageObj_GetBounds(o, &l, &b, &r, &tt) ? r - l : 500.0 * t.size();
                api.FPDFPageObj_Destroy(o);
                return v;
            };
            static std::map<std::string, double> hh;
            auto it2 = hh.find(f);
            const double base = it2 != hh.end() ? it2->second : (hh[f] = ink(L"HH"));
            w1000 = ink(L"H" + s + L"H") - base;
            cache_[key] = w1000;
        }
        return w1000 * size / 1000.0;
    }

    double spaceWidth(const char* f) {
        if (strncmp(f, "Courier", 7) == 0) return 600;
        return strstr(f, "Bold") ? 278 : 278;
    }

    void newPage() {
        if (page_) close();
        const auto& api = pdfium::api();
        page_ = api.FPDFPage_New(doc_, pages_++, W, H);
        y_ = H - T;
    }
    void close() {
        const auto& api = pdfium::api();
        api.FPDFPage_GenerateContent(page_);
        api.FPDF_ClosePage(page_);
        page_ = nullptr;
    }
    // makes room for a line of the given height, starting a new page when needed; returns the baseline
    double lineBaseline(double ascent, double lineH) {
        if (!page_ || y_ - lineH < B) newPage();
        double base = y_ - ascent;
        y_ -= lineH;
        return base;
    }
    void space(double h) { y_ -= h; if (page_ && y_ < B) newPage(); }

    void text(double x, double baseline, const std::wstring& s, Style st, bool mono, double size, unsigned rgb) {
        if (s.find_first_not_of(L' ') == std::wstring::npos) return;
        const auto& api = pdfium::api();
        FPDF_PAGEOBJECT o = api.FPDFPageObj_NewTextObj(doc_, fontOf(st, mono), static_cast<float>(size));
        std::vector<unsigned short> u(s.begin(), s.end());
        u.push_back(0);
        api.FPDFText_SetText(o, u.data());
        api.FPDFPageObj_SetFillColor(o, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 255);
        api.FPDFPageObj_Transform(o, 1, 0, 0, 1, x, baseline);
        api.FPDFPage_InsertObject(page_, o);
    }
    void rect(double x, double y, double w, double h, unsigned rgb) {
        const auto& api = pdfium::api();
        FPDF_PAGEOBJECT o = api.FPDFPageObj_CreateNewRect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h));
        api.FPDFPageObj_SetFillColor(o, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 255);
        api.FPDFPath_SetDrawMode(o, FPDF_FILLMODE_ALTERNATE, 0);
        api.FPDFPage_InsertObject(page_, o);
    }
    double y() const { return y_; }

    std::wstring clean(const std::wstring& s) {
        std::wstring o;
        for (wchar_t c : s) {
            if (c == L'\t') { o += L"    "; continue; }
            if (drawable(c)) o += c; else { o += L'?'; ++replaced_; }
        }
        return o;
    }

    // Lays out runs as wrapped lines between x0 and the right margin. `first` is drawn before the first line (a bullet).
    void paragraph(const std::vector<Run>& runs, double x0, double size, bool mono, unsigned rgb, double lineFactor = 1.35, const std::wstring& marker = L"", double markerX = 0) {
        const double maxW = W - R - x0;
        struct Word { std::wstring t; Style s; bool spaceBefore; };
        std::vector<Word> words;
        bool pendingSpace = false;
        for (const Run& r : runs) {
            std::wstring t = clean(r.text);
            std::wstring cur;
            for (wchar_t c : t) {
                if (c == L' ') { if (!cur.empty()) { words.push_back({cur, r.style, pendingSpace}); cur.clear(); } pendingSpace = true; }
                else {
                    cur += c;
                }
            }
            if (!cur.empty()) {
                // a word glued to the previous run (for example "**bold**,") continues it
                words.push_back({cur, r.style, pendingSpace});
                pendingSpace = false;
            }
        }
        const double ascent = size * 0.95, lineH = size * lineFactor;
        if (words.empty()) { space(lineH); return; }
        size_t i = 0;
        bool firstLine = true;
        while (i < words.size()) {
            // collect the words of one line
            std::vector<Word> line;
            double w = 0;
            const double sp = width(Normal, mono, size, L" ");
            while (i < words.size()) {
                double ww = width(words[i].s, mono, size, words[i].t);
                if (!line.empty() && w + (words[i].spaceBefore ? sp : 0) + ww > maxW && words[i].spaceBefore) break;
                if (line.empty() && ww > maxW) {   // a very long word: cut it
                    std::wstring head = words[i].t;
                    while (head.size() > 1 && width(words[i].s, mono, size, head) > maxW) head.pop_back();
                    line.push_back({head, words[i].s, false});
                    words[i].t = words[i].t.substr(head.size());
                    w = maxW;
                    break;
                }
                w += (line.empty() || !words[i].spaceBefore ? 0 : sp) + ww;
                line.push_back(words[i]);
                ++i;
            }
            const double base = lineBaseline(ascent, lineH);
            if (firstLine && !marker.empty()) text(markerX, base, marker, Normal, mono, size, rgb);
            firstLine = false;
            // one text object per run of the same style
            double x = x0, bufX = x0;
            std::wstring buf;
            Style cur = line.empty() ? Normal : line[0].s;
            for (size_t k = 0; k < line.size(); ++k) {
                if (k > 0) {
                    const bool gap = line[k].spaceBefore;
                    if (line[k].s == cur) { if (gap) { buf += L" "; x += width(cur, mono, size, L" "); } }
                    else {
                        text(bufX, base, buf, cur, mono, size, rgb);
                        buf.clear();
                        if (gap) x += width(cur, mono, size, L" ");
                        bufX = x;
                        cur = line[k].s;
                    }
                }
                buf += line[k].t;
                x += width(line[k].s, mono, size, line[k].t);
            }
            text(bufX, base, buf, cur, mono, size, rgb);
        }
    }

    // Fixed-width text keeps every space; long lines are cut at the margin.
    void monoLines(const std::wstring& raw, double x0, double size, unsigned rgb, unsigned band = 0) {
        const std::wstring t = clean(raw);
        const size_t perLine = std::max<size_t>(10, static_cast<size_t>((W - R - x0) / (size * 0.6)));
        size_t p = 0;
        do {
            std::wstring piece = t.substr(p, perLine);
            const double base = lineBaseline(size * 0.95, size * 1.3);
            if (band) rect(L, base - size * 0.3, W - L - R, size * 1.3, band);
            text(x0, base, piece, Normal, true, size, rgb);
            p += perLine;
        } while (p < t.size());
    }

    bool save(std::vector<unsigned char>& out) {
        if (page_) close();
        if (pages_ == 0) { newPage(); close(); }
        Sink sink;
        sink.version = 1; sink.WriteBlock = writeBlock; sink.out = &out;
        return pdfium::api().FPDF_SaveAsCopy(doc_, &sink, 0) && out.size() > 8;
    }
    int pages() const { return pages_; }

private:
    FPDF_DOCUMENT doc_ = nullptr;
    FPDF_PAGE page_ = nullptr;
    int pages_ = 0;
    double y_ = 0;
    std::map<std::string, double> cache_;
    int& replaced_;
};

// **bold**, *italic*, `code`, [text](url), ![alt](src)
std::vector<Run> inlineRuns(const std::wstring& s, Style base = Normal) {
    std::vector<Run> out;
    std::wstring cur;
    bool bold = base == Bold || base == BoldItalic, italic = base == Italic || base == BoldItalic;
    auto style = [&] { return bold && italic ? BoldItalic : bold ? Bold : italic ? Italic : Normal; };
    auto flush = [&](Style st) { if (!cur.empty()) { out.push_back({cur, st}); cur.clear(); } };
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c == L'\\' && i + 1 < s.size() && wcschr(L"\\`*_[]()#+-.!", s[i + 1])) { cur += s[++i]; continue; }
        if (c == L'`') {
            size_t e = s.find(L'`', i + 1);
            if (e != std::wstring::npos) { flush(style()); out.push_back({s.substr(i + 1, e - i - 1), Code}); i = e; continue; }
        }
        if ((c == L'*' || c == L'_') && i + 1 < s.size() && s[i + 1] == c) { flush(style()); bold = !bold; ++i; continue; }
        if ((c == L'*' || c == L'_') && i + 1 < s.size() && s[i + 1] != L' ' && (c == L'*' || i == 0 || !iswalnum(s[i - 1]))) { flush(style()); italic = !italic; continue; }
        if ((c == L'*' || c == L'_') && italic) { flush(style()); italic = false; continue; }
        if (c == L'!' && i + 1 < s.size() && s[i + 1] == L'[') {
            size_t e = s.find(L']', i), p = e == std::wstring::npos ? e : s.find(L')', e);
            if (e != std::wstring::npos && p != std::wstring::npos && e + 1 < s.size() && s[e + 1] == L'(') { cur += L"[" + s.substr(i + 2, e - i - 2) + L"]"; i = p; continue; }
        }
        if (c == L'[') {
            size_t e = s.find(L']', i), p = e == std::wstring::npos ? e : s.find(L')', e);
            if (e != std::wstring::npos && p != std::wstring::npos && e + 1 < s.size() && s[e + 1] == L'(') {
                cur += s.substr(i + 1, e - i - 1);
                std::wstring url = s.substr(e + 2, p - e - 2);
                if (!url.empty() && url != s.substr(i + 1, e - i - 1)) cur += L" (" + url + L")";
                i = p;
                continue;
            }
        }
        cur += c;
    }
    flush(style());
    return out;
}

std::wstring trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t"), z = s.find_last_not_of(L" \t");
    return a == std::wstring::npos ? L"" : s.substr(a, z - a + 1);
}

} // namespace

Result build(const std::wstring& textIn, bool markdown, std::vector<unsigned char>& out) {
    Result res;
    Writer w(res.replaced);
    if (!w.ok()) return res;
    std::vector<std::wstring> lines;
    {
        std::wstring t = textIn;
        if (!t.empty() && t[0] == 0xFEFF) t.erase(0, 1);
        size_t p = 0;
        while (p <= t.size()) {
            size_t e = t.find(L'\n', p);
            std::wstring l = t.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p);
            if (!l.empty() && l.back() == L'\r') l.pop_back();
            lines.push_back(l);
            if (e == std::wstring::npos) break;
            p = e + 1;
        }
    }
    const unsigned black = 0x202020, grey = 0x555555;
    w.newPage();
    if (!markdown) {
        for (const auto& l : lines) w.monoLines(l, Writer::L, 10, black);
    } else {
        std::wstring para;
        auto flushPara = [&] {
            if (trim(para).empty()) { para.clear(); return; }
            w.paragraph(inlineRuns(trim(para)), Writer::L, 11, false, black);
            w.space(6);
            para.clear();
        };
        bool code = false;
        for (size_t i = 0; i < lines.size(); ++i) {
            const std::wstring& raw = lines[i];
            const std::wstring t = trim(raw);
            if (t.rfind(L"```", 0) == 0 || t.rfind(L"~~~", 0) == 0) { flushPara(); code = !code; w.space(code ? 2 : 6); continue; }
            if (code) { w.monoLines(raw, Writer::L + 6, 9.5, black, 0xF0F0F0); continue; }
            if (t.empty()) { flushPara(); continue; }
            size_t hashes = 0;
            while (hashes < t.size() && t[hashes] == L'#') ++hashes;
            if (hashes >= 1 && hashes <= 6 && hashes < t.size() && t[hashes] == L' ') {
                flushPara();
                static const double sizes[] = {22, 17, 14, 12.5, 11.5, 11};
                w.space(hashes <= 2 ? 10 : 6);
                w.paragraph(inlineRuns(trim(t.substr(hashes)), Bold), Writer::L, sizes[hashes - 1], false, 0x14243B, 1.25);
                if (hashes == 1) { w.rect(Writer::L, w.y() + 2, Writer::W - Writer::L - Writer::R, 0.8, 0xB0B8C8); w.space(6); }
                w.space(4);
                continue;
            }
            if (t == L"---" || t == L"***" || t == L"___") {
                flushPara();
                w.space(6);
                w.rect(Writer::L, w.y(), Writer::W - Writer::L - Writer::R, 0.8, 0xB0B0B0);
                w.space(10);
                continue;
            }
            // lists
            const size_t indentChars = raw.find_first_not_of(L" \t");
            const int level = static_cast<int>(std::min<size_t>(indentChars == std::wstring::npos ? 0 : indentChars, 12) / 2);
            if ((t[0] == L'-' || t[0] == L'*' || t[0] == L'+') && t.size() > 1 && t[1] == L' ') {
                flushPara();
                const double x = Writer::L + 18 + level * 16;
                std::wstring item = t.substr(2);
                if (item.rfind(L"[ ] ", 0) == 0) item = L"☐ " + item.substr(4);
                else if (item.rfind(L"[x] ", 0) == 0 || item.rfind(L"[X] ", 0) == 0) item = L"[x] " + item.substr(4);
                w.paragraph(inlineRuns(item), x, 11, false, black, 1.35, L"•", x - 11);
                w.space(2);
                continue;
            }
            {
                size_t d = 0;
                while (d < t.size() && iswdigit(t[d])) ++d;
                if (d > 0 && d + 1 < t.size() && (t[d] == L'.' || t[d] == L')') && t[d + 1] == L' ') {
                    flushPara();
                    const double x = Writer::L + 22 + level * 16;
                    w.paragraph(inlineRuns(t.substr(d + 2)), x, 11, false, black, 1.35, t.substr(0, d + 1), x - 20);
                    w.space(2);
                    continue;
                }
            }
            if (t[0] == L'>') {
                flushPara();
                std::wstring q = trim(t.substr(1));
                const double top = w.y();
                w.paragraph(inlineRuns(q, Italic), Writer::L + 16, 11, false, grey);
                w.rect(Writer::L + 4, w.y() + 2, 2.5, top - w.y() - 2, 0xC0C8D8);
                w.space(4);
                continue;
            }
            if (t[0] == L'|') {   // a table row: cells in a fixed-width font; the |---| line is left out
                flushPara();
                if (t.find_first_not_of(L"|-: ") == std::wstring::npos) continue;
                std::wstring row;
                size_t p = 1;
                while (p < t.size()) {
                    size_t e = t.find(L'|', p);
                    std::wstring cell = trim(t.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p));
                    if (e == std::wstring::npos && cell.empty()) break;
                    if (cell.size() < 18) cell += std::wstring(18 - cell.size(), L' ');
                    row += cell + L" ";
                    if (e == std::wstring::npos) break;
                    p = e + 1;
                }
                w.monoLines(row, Writer::L, 9.5, black);
                continue;
            }
            para += (para.empty() ? L"" : L" ") + t;
            if (raw.size() >= 2 && raw.substr(raw.size() - 2) == L"  ") { w.paragraph(inlineRuns(trim(para)), Writer::L, 11, false, black); para.clear(); }   // hard line break
        }
        flushPara();
    }
    res.ok = w.save(out);
    res.pages = w.pages();
    return res;
}

} // namespace textpdf
