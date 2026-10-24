#include "ocr.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include "pageobjects.hpp"

#ifdef FEATHER_OCR
#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>
#include <leptonica/allheaders.h>
#endif

namespace ocr {
namespace {

std::wstring exeDir() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring d(exe);
    d.erase(d.find_last_of(L"\\/") + 1);
    return d;
}

std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring fromUtf8(const char* s) {
    if (!s || !*s) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s, -1, w.data(), n);
    return w;
}

} // namespace

std::vector<std::wstring> languages() {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((exeDir() + L"tessdata\\*.traineddata").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        std::wstring n = fd.cFileName;
        n.resize(n.size() - 12);   // ".traineddata"
        if (n != L"osd") out.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

#ifndef FEATHER_OCR

bool available(std::wstring& whyNot) {
    whyNot = L"This copy of Feather PDF was built without the text recognition engine, so scanned pages cannot be recognised.";
    return false;
}

Result run(Document&, const std::vector<int>&, const std::wstring&, bool, const std::function<bool(int, int)>&) {
    Result r;
    r.error = L"Text recognition is not available in this build.";
    return r;
}

#else

bool available(std::wstring& whyNot) {
    if (languages().empty()) {
        whyNot = L"The language data for text recognition (the \"tessdata\" folder next to FeatherPDF.exe) is missing or empty. "
                 L"Reinstall Feather PDF to restore it.";
        return false;
    }
    return true;
}

Result run(Document& d, const std::vector<int>& pages, const std::wstring& language, bool skipTextPages,
           const std::function<bool(int, int)>& progress) {
    Result res;
    setMsgSeverity(L_SEVERITY_NONE);   // Leptonica prints notes about missing image libraries we do not need
    tesseract::TessBaseAPI api;
    const std::string dir = utf8(exeDir() + L"tessdata");
    if (api.Init(dir.c_str(), utf8(language).c_str(), tesseract::OEM_LSTM_ONLY) != 0) {
        res.error = L"The text recognition engine could not load the language \"" + language + L"\". Reinstall Feather PDF to restore its language data.";
        return res;
    }
    api.SetPageSegMode(tesseract::PSM_AUTO);
    const auto& pd = pdfium::api();

    bool changed = false;
    int index = 0;
    for (int p : pages) {
        if (progress && !progress(index, static_cast<int>(pages.size()))) { res.cancelled = true; break; }
        ++index;
        if (skipTextPages && d.charCount(p) > 0) { ++res.pagesSkipped; continue; }
        FPDF_PAGE pg = d.page(p);
        if (!pg) continue;

        // Render at up to 300 dpi (less for very large pages) as grey pixels.
        const double wpt = d.pageSize(p).w, hpt = d.pageSize(p).h;
        double dpi = std::min(300.0, 6500.0 / (std::max(wpt, hpt) / 72.0));
        dpi = std::max(dpi, 100.0);
        const int w = std::max(1, static_cast<int>(std::lround(wpt * dpi / 72.0)));
        const int h = std::max(1, static_cast<int>(std::lround(hpt * dpi / 72.0)));
        if (1LL * w * h > 90000000LL) { res.error = L"Page " + std::to_wstring(p + 1) + L" is too large to recognise."; break; }
        std::vector<unsigned char> bgra(static_cast<size_t>(w) * h * 4);
        FPDF_BITMAP bmp = pd.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, bgra.data(), w * 4);
        if (!bmp) continue;
        pd.FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0xFFFFFFFF);
        // Render without annotations and without the invisible text of an earlier run: only what is printed.
        pd.FPDF_RenderPageBitmap(bmp, pg, 0, 0, w, h, 0, 0);
        pd.FPDFBitmap_Destroy(bmp);
        std::vector<unsigned char> grey(static_cast<size_t>(w) * h);
        for (size_t i = 0, n = grey.size(); i < n; ++i) {
            const unsigned char* px = &bgra[i * 4];
            grey[i] = static_cast<unsigned char>((px[0] * 29 + px[1] * 150 + px[2] * 77) >> 8);
        }
        bgra.clear(); bgra.shrink_to_fit();

        api.SetImage(grey.data(), w, h, 1, w);
        api.SetSourceResolution(static_cast<int>(dpi));
        if (api.Recognize(nullptr) != 0) { res.error = L"Page " + std::to_wstring(p + 1) + L" could not be recognised."; break; }

        struct W { std::wstring text; int x0, y0, x1, y1; };
        std::vector<W> words;
        std::unique_ptr<tesseract::ResultIterator> it(api.GetIterator());
        if (it) {
            do {
                std::unique_ptr<char[]> t(it->GetUTF8Text(tesseract::RIL_WORD));
                if (!t) continue;
                int x0, y0, x1, y1;
                if (!it->BoundingBox(tesseract::RIL_WORD, &x0, &y0, &x1, &y1)) continue;
                if (it->Confidence(tesseract::RIL_WORD) < 25.0f) continue;
                std::wstring text = fromUtf8(t.get());
                while (!text.empty() && (text.back() == L'\n' || text.back() == L' ')) text.pop_back();
                if (text.empty() || x1 <= x0 || y1 <= y0) continue;
                words.push_back({text, x0, y0, x1, y1});
            } while (it->Next(tesseract::RIL_WORD));
        }
        if (words.empty()) { ++res.pagesDone; continue; }

        if (!changed) { d.beginChange(); changed = true; }
        pg = d.page(p);
        const double k = 72.0 / dpi;   // pixels -> points
        int added = 0;
        for (const W& wd : words) {
            const double boxW = (wd.x1 - wd.x0) * k, boxH = (wd.y1 - wd.y0) * k;
            const double size = std::max(4.0, boxH * 0.85);
            FPDF_PAGEOBJECT o = pageobj::newText(d, "Helvetica", size, wd.text, 0, 100);
            if (!o) continue;
            float l, b, r, t;
            if (!pd.FPDFPageObj_GetBounds(o, &l, &b, &r, &t) || r - l < 0.1f) { pd.FPDFPageObj_Destroy(o); continue; }
            const double sx = std::clamp(boxW / (r - l), 0.15, 6.0);
            pd.FPDFTextObj_SetTextRenderMode(o, FPDF_TEXTRENDERMODE_INVISIBLE);
            pageobj::placeScaled(pg, o, wpt, hpt, wd.x0 * k, wd.y1 * k - boxH * 0.2, sx);
            pd.FPDFPage_InsertObject(pg, o);
            ++added;
        }
        pd.FPDFPage_GenerateContent(pg);
        d.resetText(p);
        res.words += added;
        ++res.pagesDone;
    }
    api.End();
    if (changed && res.words == 0) { d.cancelChange(); changed = false; }
    return res;
}

#endif

} // namespace ocr
