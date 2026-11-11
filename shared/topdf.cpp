#include "topdf.hpp"
#include <algorithm>
#include <cwctype>
#include "document.hpp"
#include "picture.hpp"
#include "platform.hpp"
#include "textpdf.hpp"

namespace topdf {

static std::wstring lowerExt(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    const size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) return L"";
    std::wstring e = path.substr(dot);
    for (auto& c : e) c = static_cast<wchar_t>(std::towlower(c));
    return e;
}

std::vector<std::wstring> extensions() {
    std::vector<std::wstring> v = {L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".tif", L".tiff", L".txt", L".md", L".markdown"};
#ifdef __APPLE__
    v.push_back(L".heic");
#endif
    return v;
}

Kind kindOf(const std::wstring& path) {
    const std::wstring e = lowerExt(path);
    if (e == L".txt") return Kind::Text;
    if (e == L".md" || e == L".markdown") return Kind::Markdown;
    for (const auto& x : extensions()) if (x == e) return Kind::Picture;
    return Kind::None;
}

Result fromPictures(const std::vector<std::wstring>& files, std::vector<unsigned char>& out) {
    Result r;
    std::vector<std::pair<double, double>> sizes;
    std::vector<std::wstring> ok;
    for (const auto& f : files) {
        int w, h;
        if (!pageobj::imageSize(f, w, h) || w <= 0 || h <= 0) { ++r.skipped; continue; }
        double pw = w * 0.75, ph = h * 0.75;   // 96 dpi
        const double big = std::max(pw, ph);
        if (big > 842) { pw *= 842 / big; ph *= 842 / big; }
        sizes.push_back({pw, ph});
        ok.push_back(f);
    }
    if (ok.empty()) { r.error = L"The picture could not be read. The file may be damaged."; return r; }
    std::vector<unsigned char> blank;
    Document doc;
    if (!Document::blankBytes(sizes, blank) || doc.load(std::move(blank), {}) != OpenResult::Ok) { r.error = L"The new document could not be made."; return r; }
    std::vector<int> bad;
    for (size_t i = 0; i < ok.size(); ++i) {
        std::wstring err;
        if (!pageobj::addPicture(doc, static_cast<int>(i), 0, sizes[i].second, ok[i], err, sizes[i].first)) bad.push_back(static_cast<int>(i));
    }
    if (bad.size() == ok.size()) { r.error = L"The picture could not be placed on a page."; return r; }
    if (!bad.empty()) doc.deletePages(bad);
    r.skipped += static_cast<int>(bad.size());
    if (!doc.bytesToSave(out)) { r.error = L"The PDF could not be written."; return r; }
    r.pages = doc.pageCount();
    r.ok = true;
    return r;
}

Result fromFile(const std::wstring& path, std::vector<unsigned char>& out) {
    Result r;
    const Kind k = kindOf(path);
    if (k == Kind::None) { r.error = L"Feather PDF converts pictures (PNG, JPEG, BMP, GIF, TIFF) and text or Markdown files."; return r; }
    if (!platform::fileExists(path)) { r.error = L"The file was not found."; return r; }
    if (k == Kind::Picture) return fromPictures({path}, out);
    std::vector<unsigned char> raw;
    if (!platform::readFile(path, raw)) { r.error = L"The file could not be read."; return r; }
    const textpdf::Result t = textpdf::build(platform::readTextFile(path), k == Kind::Markdown, out);
    r.ok = t.ok; r.pages = t.pages; r.replaced = t.replaced;
    if (!r.ok) r.error = L"The text could not be made into a PDF.";
    return r;
}

std::wstring outputPath(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    const size_t dot = path.find_last_of(L'.');
    const std::wstring base = dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash) ? path : path.substr(0, dot);
    std::wstring p = base + L".pdf";
    for (int n = 2; platform::fileExists(p) && n < 10000; ++n) p = base + L" (" + std::to_wstring(n) + L").pdf";
    return p;
}

Result convert(const std::wstring& path, std::wstring& written) {
    std::vector<unsigned char> bytes;
    Result r = fromFile(path, bytes);
    if (!r.ok) return r;
    written = outputPath(path);
    if (!platform::writeFile(written, bytes)) {
        r.ok = false;
        r.error = L"The PDF could not be saved next to the file (" + written + L"). The folder may be read-only.";
        written.clear();
    }
    return r;
}

} // namespace topdf
