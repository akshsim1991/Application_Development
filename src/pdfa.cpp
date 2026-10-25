#include "pdfa.hpp"
#include <windows.h>
#include <objbase.h>
#include <algorithm>
using std::min; using std::max;
#include <gdiplus.h>
#include <cmath>
#include <cstdint>
#include <map>
#include "pdfedit.hpp"
#include "pdfium_api.hpp"
#include "sanitize.hpp"

namespace pdfa {
namespace {

using namespace pdfedit;

// ---- ICC profile ----
void be32(std::string& s, uint32_t v) { for (int i = 3; i >= 0; --i) s += static_cast<char>((v >> (i * 8)) & 0xFF); }
void be16(std::string& s, uint32_t v) { s += static_cast<char>((v >> 8) & 0xFF); s += static_cast<char>(v & 0xFF); }
void fixed(std::string& s, double v) { be32(s, static_cast<uint32_t>(static_cast<int32_t>(std::lround(v * 65536.0)))); }
std::string xyz(double x, double y, double z) { std::string t = "XYZ "; be32(t, 0); fixed(t, x); fixed(t, y); fixed(t, z); return t; }

// ---- fonts ----
// The embedded font that replaces a standard (not embedded) font, by its name; empty when there is none.
std::wstring fontFileFor(std::string base) {
    for (auto& c : base) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (base.size() > 7 && base[6] == '+') base.erase(0, 7);   // subset prefix
    std::wstring family;
    if (base.find("courier") != std::string::npos || base.find("mono") != std::string::npos) family = L"cour";
    else if (base.find("times") != std::string::npos || (base.find("serif") != std::string::npos && base.find("sans") == std::string::npos)) family = L"times";
    else if (base.find("helvetica") != std::string::npos || base.find("arial") != std::string::npos || base.find("sans") != std::string::npos) family = L"arial";
    else return L"";
    const bool bold = base.find("bold") != std::string::npos || base.find("black") != std::string::npos || base.find("heavy") != std::string::npos;
    const bool italic = base.find("italic") != std::string::npos || base.find("oblique") != std::string::npos;
    std::wstring file = family + (bold && italic ? L"bi" : bold ? L"bd" : italic ? L"i" : L"");
    if (family == L"times" && !bold && !italic) file = L"times";
    wchar_t dir[MAX_PATH] = L"";
    GetWindowsDirectoryW(dir, MAX_PATH);
    const std::wstring path = std::wstring(dir) + L"\\Fonts\\" + file + L".ttf";
    return GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES ? L"" : path;
}

bool readAll(const std::wstring& path, std::vector<unsigned char>& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    out.resize(static_cast<size_t>(sz.QuadPart));
    DWORD got = 0;
    const bool ok = sz.QuadPart < 64 * 1024 * 1024 && ReadFile(f, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) && got == out.size();
    CloseHandle(f);
    return ok;
}

// Plain ASCII is the same in every simple-font encoding; anything else goes through a Unicode (CID) font.
bool plainAscii(const std::wstring& t) {
    for (wchar_t c : t) if (c < 32 || c > 126) return false;
    return true;
}

std::string baseName(FPDF_FONT font) {
    const auto& api = pdfium::api();
    const size_t n = api.FPDFFont_GetBaseFontName(font, nullptr, 0);
    std::string s(n, '\0');
    if (n) api.FPDFFont_GetBaseFontName(font, s.data(), n);
    while (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

// Visible text in a font that is not embedded, inside a form object (those cannot be set again)
bool formHasLooseText(FPDF_PAGEOBJECT form, int depth = 0) {
    const auto& api = pdfium::api();
    const int n = api.FPDFFormObj_CountObjects(form);
    for (int i = 0; i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFFormObj_GetObject(form, static_cast<unsigned long>(i));
        if (!o) continue;
        const int t = api.FPDFPageObj_GetType(o);
        if (t == FPDF_PAGEOBJ_TEXT && api.FPDFTextObj_GetTextRenderMode(o) != FPDF_TEXTRENDERMODE_INVISIBLE) {
            FPDF_FONT f = api.FPDFTextObj_GetFont(o);
            if (f && !api.FPDFFont_GetIsEmbedded(f)) return true;
        } else if (t == FPDF_PAGEOBJ_FORM && depth < 8 && formHasLooseText(o, depth + 1)) return true;
    }
    return false;
}

bool cmykImage(FPDF_PAGEOBJECT o, FPDF_PAGE pg) {
    FPDF_IMAGEOBJ_METADATA md{};
    if (!pdfium::api().FPDFImageObj_GetImageMetadata(o, pg, &md)) return false;
    return md.colorspace == FPDF_COLORSPACE_DEVICECMYK || md.colorspace == FPDF_COLORSPACE_DEVICEN || md.colorspace == FPDF_COLORSPACE_SEPARATION;
}

// ---- pages as pictures ----
struct MemAccess : FPDF_FILEACCESS { std::vector<unsigned char> data; };
int readBlock(void* param, unsigned long pos, unsigned char* buf, unsigned long size) {
    auto* m = static_cast<MemAccess*>(param);
    if (pos + size > m->data.size()) return 0;
    memcpy(buf, m->data.data() + pos, size);
    return 1;
}

bool jpegOf(const unsigned char* px, int w, int h, int stride, std::vector<unsigned char>& out) {
    Gdiplus::GdiplusStartupInput in;
    ULONG_PTR tok = 0;
    if (Gdiplus::GdiplusStartup(&tok, &in, nullptr) != Gdiplus::Ok) return false;
    bool ok = false;
    {
        Gdiplus::Bitmap bmp(w, h, stride, PixelFormat32bppRGB, const_cast<unsigned char*>(px));
        UINT n = 0, size = 0;
        Gdiplus::GetImageEncodersSize(&n, &size);
        std::vector<unsigned char> buf(size ? size : 1);
        auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
        CLSID jpg{};
        bool found = false;
        if (size && Gdiplus::GetImageEncoders(n, size, codecs) == Gdiplus::Ok)
            for (UINT i = 0; i < n; ++i) if (wcscmp(codecs[i].MimeType, L"image/jpeg") == 0) { jpg = codecs[i].Clsid; found = true; }
        IStream* st = nullptr;
        if (found && SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &st))) {
            Gdiplus::EncoderParameters ep;
            ULONG quality = 88;
            ep.Count = 1;
            ep.Parameter[0].Guid = Gdiplus::EncoderQuality;
            ep.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
            ep.Parameter[0].NumberOfValues = 1;
            ep.Parameter[0].Value = &quality;
            if (bmp.Save(st, &jpg, &ep) == Gdiplus::Ok) {
                HGLOBAL hg = nullptr;
                STATSTG stat{};
                if (SUCCEEDED(GetHGlobalFromStream(st, &hg)) && SUCCEEDED(st->Stat(&stat, STATFLAG_NONAME))) {
                    const auto* p = static_cast<const unsigned char*>(GlobalLock(hg));
                    if (p) { out.assign(p, p + stat.cbSize.QuadPart); GlobalUnlock(hg); ok = !out.empty(); }
                }
            }
            st->Release();
        }
    }
    Gdiplus::GdiplusShutdown(tok);
    return ok;
}

// The page becomes one picture (200 dpi) under its own text, which is kept but made invisible (still searchable).
bool pictureOnly(Document& d, int page) {
    const auto& api = pdfium::api();
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    const PageSize& ps = d.pageSize(page);
    const double dpi = 200;
    const int W = std::max(1, static_cast<int>(ps.w * dpi / 72)), H = std::max(1, static_cast<int>(ps.h * dpi / 72));
    FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(W, H, FPDFBitmap_BGRx, nullptr, 0);
    if (!bmp) return false;
    api.FPDFBitmap_FillRect(bmp, 0, 0, W, H, 0xFFFFFFFF);
    api.FPDF_RenderPageBitmap(bmp, pg, 0, 0, W, H, 0, FPDF_ANNOT | FPDF_PRINTING);
    MemAccess ma;
    const bool enc = jpegOf(static_cast<const unsigned char*>(api.FPDFBitmap_GetBuffer(bmp)), W, H, api.FPDFBitmap_GetStride(bmp), ma.data);
    api.FPDFBitmap_Destroy(bmp);
    if (!enc) return false;
    FPDF_PAGEOBJECT img = api.FPDFPageObj_NewImageObj(d.handle());
    ma.m_FileLen = static_cast<unsigned long>(ma.data.size());
    ma.m_GetBlock = readBlock;
    ma.m_Param = &ma;
    if (!img || !api.FPDFImageObj_LoadJpegFileInline(&pg, 1, img, &ma)) { if (img) api.FPDFPageObj_Destroy(img); return false; }
    // the picture's unit square goes over the page as displayed (whatever its rotation and crop)
    auto toPage = [&](int x, int y) { double px = 0, py = 0; api.FPDF_DeviceToPage(pg, 0, 0, W, H, 0, x, y, &px, &py); return std::pair<double, double>(px, py); };
    const auto p0 = toPage(0, H), pX = toPage(W, H), pY = toPage(0, 0);
    FS_MATRIX m{static_cast<float>(pX.first - p0.first), static_cast<float>(pX.second - p0.second), static_cast<float>(pY.first - p0.first),
                static_cast<float>(pY.second - p0.second), static_cast<float>(p0.first), static_cast<float>(p0.second)};
    api.FPDFPageObj_SetMatrix(img, &m);
    for (int i = api.FPDFPage_CountObjects(pg) - 1; i >= 0; --i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o) continue;
        if (api.FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_TEXT) api.FPDFTextObj_SetTextRenderMode(o, FPDF_TEXTRENDERMODE_INVISIBLE);
        else if (api.FPDFPage_RemoveObject(pg, o)) api.FPDFPageObj_Destroy(o);
    }
    api.FPDFPage_InsertObjectAtIndex(pg, img, 0);
    api.FPDFPage_GenerateContent(pg);
    d.resetText(page);
    return true;
}

std::string removeKeyOf(std::string dict, const char* key) {
    size_t b, e;
    if (!valueSpan(dict, key, b, e)) return dict;
    size_t k = dict.rfind(key, b);
    if (k == std::string::npos) return dict;
    dict.erase(k, e - k);
    return dict;
}

std::string xmlEscape(const std::wstring& t) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, t.c_str(), static_cast<int>(t.size()), nullptr, 0, nullptr, nullptr);
    std::string u(static_cast<size_t>(std::max(0, n)), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, t.c_str(), static_cast<int>(t.size()), u.data(), n, nullptr, nullptr);
    std::string o;
    for (char c : u) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: if (static_cast<unsigned char>(c) >= 0x20 || c == '\t') o += c;
        }
    }
    return o;
}

} // namespace

std::string srgbProfile() {
    // tag data
    std::string desc = "desc";
    be32(desc, 0);
    const std::string name = "sRGB IEC61966-2.1";
    be32(desc, static_cast<uint32_t>(name.size() + 1));
    desc += name; desc += '\0';
    be32(desc, 0); be32(desc, 0);    // Unicode language code and count
    be16(desc, 0); desc += '\0';     // ScriptCode code and count
    desc += std::string(67, '\0');
    std::string cprt = "text";
    be32(cprt, 0);
    cprt += "No copyright, use freely";
    cprt += '\0';
    std::string curv = "curv";
    be32(curv, 0);
    be32(curv, 256);
    for (int i = 0; i < 256; ++i) {
        const double v = i / 255.0, lin = v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
        be16(curv, static_cast<uint32_t>(std::lround(lin * 65535)));
    }
    struct Tag { const char* sig; std::string data; };
    std::vector<Tag> tags = {{"desc", desc}, {"cprt", cprt}, {"wtpt", xyz(0.9642, 1.0, 0.8249)},
                             {"rXYZ", xyz(0.4361, 0.2225, 0.0139)}, {"gXYZ", xyz(0.3851, 0.7169, 0.0971)}, {"bXYZ", xyz(0.1431, 0.0606, 0.7141)},
                             {"rTRC", curv}, {"gTRC", ""}, {"bTRC", ""}};   // the three curves share one block
    std::string table, data;
    const uint32_t base = 128 + 4 + 12 * static_cast<uint32_t>(tags.size());
    uint32_t curvOff = 0;
    be32(table, static_cast<uint32_t>(tags.size()));
    for (auto& t : tags) {
        uint32_t off, len;
        if (t.data.empty()) { off = curvOff; len = static_cast<uint32_t>(curv.size()); }
        else {
            while (data.size() % 4) data += '\0';
            off = base + static_cast<uint32_t>(data.size());
            len = static_cast<uint32_t>(t.data.size());
            if (std::string(t.sig) == "rTRC") curvOff = off;
            data += t.data;
        }
        table += t.sig;
        be32(table, off);
        be32(table, len);
    }
    while (data.size() % 4) data += '\0';
    std::string h;
    be32(h, static_cast<uint32_t>(128 + table.size() + data.size()));
    be32(h, 0);                       // CMM
    be32(h, 0x02100000);              // version 2.1
    h += "mntrRGB XYZ ";
    be16(h, 2026); be16(h, 1); be16(h, 1); be16(h, 0); be16(h, 0); be16(h, 0);
    h += "acsp";
    be32(h, 0); be32(h, 0); be32(h, 0); be32(h, 0); be32(h, 0); be32(h, 0);   // platform, flags, maker, model, attributes
    be32(h, 0);                       // rendering intent
    fixed(h, 0.9642); fixed(h, 1.0); fixed(h, 0.8249);
    be32(h, 0);                       // creator
    h += std::string(128 - h.size(), '\0');
    return h + table + data;
}

bool convert(Document& src, bool allPictures, std::vector<unsigned char>& out, Result& r, std::wstring& error) {
    const auto& api = pdfium::api();
    std::vector<unsigned char> bytes;
    if (!src.serialize(bytes, FPDF_REMOVE_SECURITY)) { error = L"The document could not be read."; return false; }
    Document w;
    if (w.load(std::move(bytes), {}) != OpenResult::Ok) { error = L"The document could not be read."; return false; }
    std::wstring title;
    {
        unsigned long n = api.FPDF_GetMetaText(w.handle(), "Title", nullptr, 0);
        if (n > 2) { std::vector<unsigned short> b(n / 2 + 1); api.FPDF_GetMetaText(w.handle(), "Title", b.data(), n); title.assign(b.begin(), b.begin() + (n / 2 - 1)); }
    }
    if (title.empty()) { title = src.title(); if (title.find_last_of(L'.') != std::wstring::npos) title.resize(title.find_last_of(L'.')); }

    w.flattenAll();   // comments and form fields become part of the pages
    r.pages = w.pageCount();
    std::map<std::wstring, std::pair<FPDF_FONT, std::vector<unsigned char>>> fonts;   // file + "|cid" -> loaded font
    auto fontFor = [&](const std::wstring& file, bool cid) -> FPDF_FONT {
        const std::wstring key = file + (cid ? L"|cid" : L"");
        auto it = fonts.find(key);
        if (it != fonts.end()) return it->second.first;
        std::vector<unsigned char> data;
        FPDF_FONT f = readAll(file, data) ? api.FPDFText_LoadFont(w.handle(), data.data(), static_cast<uint32_t>(data.size()), FPDF_FONT_TRUETYPE, cid) : nullptr;
        fonts[key] = {f, std::move(data)};
        return f;
    };
    for (int p = 0; p < w.pageCount(); ++p) {
        FPDF_PAGE pg = w.page(p);
        FPDF_TEXTPAGE tp = w.text(p);
        if (!pg) continue;
        // annotations left after flattening: links stay (printable), the rest go
        for (int i = api.FPDFPage_GetAnnotCount(pg) - 1; i >= 0; --i) {
            FPDF_ANNOTATION a = api.FPDFPage_GetAnnot(pg, i);
            if (!a) continue;
            const bool link = api.FPDFAnnot_GetSubtype(a) == FPDF_ANNOT_LINK;
            if (link) api.FPDFAnnot_SetFlags(a, FPDF_ANNOT_FLAG_PRINT);
            api.FPDFPage_CloseAnnot(a);
            if (!link) api.FPDFPage_RemoveAnnot(pg, i);
        }
        bool picture = allPictures;
        std::vector<std::pair<int, std::wstring>> swap;   // text objects to set again, with their font file
        const int n = api.FPDFPage_CountObjects(pg);
        for (int i = 0; i < n && !picture; ++i) {
            FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
            const int t = o ? api.FPDFPageObj_GetType(o) : 0;
            if (t == FPDF_PAGEOBJ_TEXT) {
                if (api.FPDFTextObj_GetTextRenderMode(o) == FPDF_TEXTRENDERMODE_INVISIBLE) continue;
                FPDF_FONT f = api.FPDFTextObj_GetFont(o);
                if (!f || api.FPDFFont_GetIsEmbedded(f)) continue;
                const std::wstring file = fontFileFor(baseName(f));
                if (file.empty()) picture = true; else swap.push_back({i, file});
            } else if (t == FPDF_PAGEOBJ_IMAGE && cmykImage(o, pg)) picture = true;
            else if (t == FPDF_PAGEOBJ_FORM && formHasLooseText(o)) picture = true;
        }
        if (!picture) {
            for (auto it = swap.rbegin(); it != swap.rend(); ++it) {   // from the top down, so indices stay valid
                FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, it->first);
                const unsigned long nb = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
                std::vector<unsigned short> buf(nb / 2 + 1, 0);
                if (nb) api.FPDFTextObj_GetText(o, tp, buf.data(), nb);
                std::wstring text(buf.begin(), buf.begin() + (nb >= 2 ? nb / 2 - 1 : 0));
                if (text.empty()) { picture = true; break; }
                float size = 12;
                api.FPDFTextObj_GetFontSize(o, &size);
                FPDF_FONT nf = fontFor(it->second, !plainAscii(text));
                FPDF_PAGEOBJECT no = nf ? api.FPDFPageObj_CreateTextObj(w.handle(), nf, size) : nullptr;
                std::vector<unsigned short> wt(text.begin(), text.end());
                wt.push_back(0);
                if (!no || !api.FPDFText_SetText(no, wt.data())) { if (no) api.FPDFPageObj_Destroy(no); picture = true; break; }
                FS_MATRIX m;
                api.FPDFPageObj_GetMatrix(o, &m);
                api.FPDFPageObj_SetMatrix(no, &m);
                unsigned R, G, B, A;
                if (api.FPDFPageObj_GetFillColor(o, &R, &G, &B, &A)) api.FPDFPageObj_SetFillColor(no, R, G, B, A);
                if (api.FPDFPageObj_GetStrokeColor(o, &R, &G, &B, &A)) api.FPDFPageObj_SetStrokeColor(no, R, G, B, A);
                api.FPDFTextObj_SetTextRenderMode(no, api.FPDFTextObj_GetTextRenderMode(o));
                api.FPDFPage_InsertObjectAtIndex(pg, no, static_cast<size_t>(it->first));
                if (api.FPDFPage_RemoveObject(pg, o)) api.FPDFPageObj_Destroy(o);
                ++r.fontsReplaced;
            }
            if (!swap.empty()) { api.FPDFPage_GenerateContent(pg); w.resetText(p); }
        }
        if (picture) {
            if (!pictureOnly(w, p)) { error = L"Page " + std::to_wstring(p + 1) + L" could not be turned into a picture."; return false; }
            ++r.picturePages;
        }
    }
    std::vector<unsigned char> b1, b2;
    const bool saved = w.serialize(b1);
    for (auto& kv : fonts) if (kv.second.first) api.FPDFFont_Close(kv.second.first);
    if (!saved) { error = L"The converted pages could not be saved."; return false; }

    // no scripts, attachments, old metadata or hidden content
    sanitize::Options so;
    so.metadata = so.attachments = so.scripts = so.hidden = true;
    sanitize::Report rep;
    if (!sanitize::run(b1, so, b2, rep, error)) return false;

    // XMP that names the standard, the sRGB output intent; no form dictionary (its fields are flattened)
    const std::string s(b2.begin(), b2.end());
    File f(s);
    if (!f.parseXref()) { error = L"The cleaned file could not be read."; return false; }
    const int root = refOf(f.trailer, "/Root");
    std::string cat;
    if (root < 0 || !f.object(root, cat)) { error = L"The document catalogue is missing."; return false; }
    SYSTEMTIME st;
    GetSystemTime(&st);
    char when[40];
    snprintf(when, sizeof when, "%04d-%02d-%02dT%02d:%02d:%02dZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    const std::string xmp =
        "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
        "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
        "<rdf:Description rdf:about=\"\" xmlns:pdfaid=\"http://www.aiim.org/pdfa/ns/id/\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\""
        " xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\" xmlns:pdf=\"http://ns.adobe.com/pdf/1.3/\">\n"
        "<pdfaid:part>2</pdfaid:part>\n<pdfaid:conformance>B</pdfaid:conformance>\n"
        "<dc:title><rdf:Alt><rdf:li xml:lang=\"x-default\">" + xmlEscape(title) + "</rdf:li></rdf:Alt></dc:title>\n"
        "<xmp:CreatorTool>Feather PDF</xmp:CreatorTool>\n<xmp:CreateDate>" + when + "</xmp:CreateDate>\n<xmp:ModifyDate>" + when + "</xmp:ModifyDate>\n"
        "<pdf:Producer>Feather PDF</pdf:Producer>\n"
        "</rdf:Description>\n</rdf:RDF>\n</x:xmpmeta>\n" + std::string(20, ' ') + "\n<?xpacket end=\"w\"?>";
    const std::string icc = srgbProfile();
    int next = f.size;
    const int xmpObj = next++, iccObj = next++, oiObj = next++;
    std::map<int, std::string> write;
    write[xmpObj] = "<< /Type /Metadata /Subtype /XML /Length " + std::to_string(xmp.size()) + " >>\nstream\n" + xmp + "\nendstream";
    write[iccObj] = "<< /N 3 /Length " + std::to_string(icc.size()) + " >>\nstream\n" + icc + "\nendstream";
    write[oiObj] = "<< /Type /OutputIntent /S /GTS_PDFA1 /OutputConditionIdentifier (sRGB IEC61966-2.1) /Info (sRGB IEC61966-2.1) /DestOutputProfile " + std::to_string(iccObj) + " 0 R >>";
    // PDFium leaves the encoding out of the simple TrueType fonts it makes; PDF/A wants it named
    for (const auto& kv : f.offsets) {
        std::string body;
        if (f.object(kv.first, body) && body.find("/CIDFontType2") != std::string::npos && body.find("/CIDToGIDMap") == std::string::npos && body.find("stream") == std::string::npos) {
            write[kv.first] = setKey(body, "/CIDToGIDMap", "/Identity");   // the default, but PDF/A wants it written
            continue;
        }
        if (!f.object(kv.first, body) || body.find("/TrueType") == std::string::npos || body.find("/Font") == std::string::npos ||
            body.find("stream") != std::string::npos || body.find("/Encoding") != std::string::npos) continue;
        std::string fd;
        size_t fb, fe;
        const int desc = refOf(body, "/FontDescriptor");
        if (desc < 0 || !f.object(desc, fd) || !valueSpan(fd, "/Flags", fb, fe) || (atol(fd.c_str() + fb) & 4)) continue;   // symbolic fonts keep their own
        write[kv.first] = setKey(body, "/Encoding", "/WinAnsiEncoding");
    }
    cat = removeKeyOf(cat, "/AcroForm");
    cat = setKey(cat, "/Metadata", std::to_string(xmpObj) + " 0 R");
    cat = setKey(cat, "/OutputIntents", "[" + std::to_string(oiObj) + " 0 R]");
    write[root] = cat;
    std::vector<unsigned char> b3 = appendIncremental(s, f, write, next);

    // one clean revision, written by PDFium (it also gives the file its /ID)
    Document fin;
    if (fin.load(std::move(b3), {}) != OpenResult::Ok || !fin.serialize(out)) { error = L"The PDF/A file could not be written."; return false; }
    if (r.picturePages) r.notes.push_back(std::to_wstring(r.picturePages) + L" page(s) kept as pictures with searchable text, because their fonts could not be embedded.");
    if (r.fontsReplaced) r.notes.push_back(L"Text in fonts that were not embedded was set again in Arial, Times New Roman or Courier New, which are embedded.");
    return true;
}

} // namespace pdfa
