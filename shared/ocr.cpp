#include "ocr.hpp"
#include "contentgen.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include "pageobjects.hpp"
#include "platform.hpp"
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef FEATHER_OCR
#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>
#include <leptonica/allheaders.h>
#endif

namespace ocr {
namespace {

Engine g_engine;   // set by the app on systems without the bundled engine

#ifdef _WIN32
std::wstring exeDir() {
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring d(exe);
    d.erase(d.find_last_of(L"\\/") + 1);
    return d;
}
#endif

// The page as printed (no annotations, no invisible text of an earlier run), as grey pixels at up to 300 dpi (less for
// very large pages).
bool renderGrey(Document& d, int p, double& dpi, int& w, int& h, std::vector<unsigned char>& grey, std::wstring& error) {
    const auto& pd = pdfium::api();
    FPDF_PAGE pg = d.page(p);
    if (!pg) return false;
    const double wpt = d.pageSize(p).w, hpt = d.pageSize(p).h;
    dpi = std::min(300.0, 6500.0 / (std::max(wpt, hpt) / 72.0));
    dpi = std::max(dpi, 100.0);
    w = std::max(1, static_cast<int>(std::lround(wpt * dpi / 72.0)));
    h = std::max(1, static_cast<int>(std::lround(hpt * dpi / 72.0)));
    if (1LL * w * h > 90000000LL) { error = L"Page " + std::to_wstring(p + 1) + L" is too large to recognise."; return false; }
    std::vector<unsigned char> bgra(static_cast<size_t>(w) * h * 4);
    FPDF_BITMAP bmp = pd.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, bgra.data(), w * 4);
    if (!bmp) return false;
    pd.FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0xFFFFFFFF);
    pd.FPDF_RenderPageBitmap(bmp, pg, 0, 0, w, h, 0, 0);
    pd.FPDFBitmap_Destroy(bmp);
    grey.resize(static_cast<size_t>(w) * h);
    for (size_t i = 0, n = grey.size(); i < n; ++i) {
        const unsigned char* px = &bgra[i * 4];
        grey[i] = static_cast<unsigned char>((px[0] * 29 + px[1] * 150 + px[2] * 77) >> 8);
    }
    return true;
}

// The words as invisible text over their places on the page. Returns how many were added.
int placeWords(Document& d, int p, const std::vector<Word>& words, double dpi) {
    const auto& pd = pdfium::api();
    FPDF_PAGE pg = d.page(p);
    if (!pg) return 0;
    const double wpt = d.pageSize(p).w, hpt = d.pageSize(p).h;
    const double k = 72.0 / dpi;   // pixels -> points
    int added = 0;
    for (const Word& wd : words) {
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
    contentgen::generate(d, pg);
    d.resetText(p);
    return added;
}

using Recognise = std::function<bool(const std::vector<unsigned char>&, int, int, double, std::vector<Word>&, std::wstring&)>;

// Every page: picture, recognition, text layer. One undo step.
Result runWith(Document& d, const std::vector<int>& pages, bool skipTextPages, const std::function<bool(int, int)>& progress,
               const Recognise& recognise) {
    Result res;
    bool changed = false;
    int index = 0;
    for (int p : pages) {
        if (progress && !progress(index, static_cast<int>(pages.size()))) { res.cancelled = true; break; }
        ++index;
        if (skipTextPages && d.charCount(p) > 0) { ++res.pagesSkipped; continue; }
        double dpi = 0;
        int w = 0, h = 0;
        std::vector<unsigned char> grey;
        std::wstring error;
        if (!renderGrey(d, p, dpi, w, h, grey, error)) {
            if (!error.empty()) { res.error = error; break; }
            continue;
        }
        std::vector<Word> words;
        if (!recognise(grey, w, h, dpi, words, error)) {
            res.error = error.empty() ? L"Page " + std::to_wstring(p + 1) + L" could not be recognised." : error;
            break;
        }
        words.erase(std::remove_if(words.begin(), words.end(), [](const Word& x) { return x.text.empty() || x.x1 <= x.x0 || x.y1 <= x.y0; }),
                    words.end());
        if (words.empty()) { ++res.pagesDone; continue; }
        if (!changed) { d.beginChange(L"Recognise text (OCR)"); changed = true; }
        res.words += placeWords(d, p, words, dpi);
        ++res.pagesDone;
    }
    if (changed && res.words == 0) d.cancelChange();
    return res;
}

} // namespace

void setEngine(Engine engine) { g_engine = std::move(engine); }

#ifdef FEATHER_OCR   // the bundled Tesseract engine (the Windows app)

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
    setMsgSeverity(L_SEVERITY_NONE);   // Leptonica prints notes about missing image libraries we do not need
    tesseract::TessBaseAPI api;
    const std::string dir = platform::utf8(exeDir() + L"tessdata");
    if (api.Init(dir.c_str(), platform::utf8(language).c_str(), tesseract::OEM_LSTM_ONLY) != 0) {
        Result res;
        res.error = L"The text recognition engine could not load the language \"" + language + L"\". Reinstall Feather PDF to restore its language data.";
        return res;
    }
    api.SetPageSegMode(tesseract::PSM_AUTO);
    Result res = runWith(d, pages, skipTextPages, progress, [&](const std::vector<unsigned char>& grey, int w, int h, double dpi,
                                                                  std::vector<Word>& words, std::wstring&) {
        api.SetImage(grey.data(), w, h, 1, w);
        api.SetSourceResolution(static_cast<int>(dpi));
        if (api.Recognize(nullptr) != 0) return false;
        std::unique_ptr<tesseract::ResultIterator> it(api.GetIterator());
        if (!it) return true;
        do {
            std::unique_ptr<char[]> t(it->GetUTF8Text(tesseract::RIL_WORD));
            if (!t) continue;
            int x0, y0, x1, y1;
            if (!it->BoundingBox(tesseract::RIL_WORD, &x0, &y0, &x1, &y1)) continue;
            if (it->Confidence(tesseract::RIL_WORD) < 25.0f) continue;
            std::wstring text = platform::wide(t.get());
            while (!text.empty() && (text.back() == L'\n' || text.back() == L' ')) text.pop_back();
            words.push_back({text, x0, y0, x1, y1});
        } while (it->Next(tesseract::RIL_WORD));
        return true;
    });
    api.End();
    return res;
}

#else   // the engine the app gives (the Mac: Vision), or none

std::vector<std::wstring> languages() { return g_engine.languages ? g_engine.languages() : std::vector<std::wstring>(); }

bool available(std::wstring& whyNot) {
    if (g_engine.recognise && !languages().empty()) return true;
    whyNot = L"This copy of Feather PDF was built without the text recognition engine, so scanned pages cannot be recognised.";
    return false;
}

Result run(Document& d, const std::vector<int>& pages, const std::wstring& language, bool skipTextPages,
           const std::function<bool(int, int)>& progress) {
    if (!g_engine.recognise) {
        Result res;
        res.error = L"Text recognition is not available in this build.";
        return res;
    }
    return runWith(d, pages, skipTextPages, progress, [&](const std::vector<unsigned char>& grey, int w, int h, double,
                                                          std::vector<Word>& words, std::wstring& error) {
        return g_engine.recognise(grey, w, h, language, words, error);
    });
}

#endif

} // namespace ocr
