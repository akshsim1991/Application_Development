#include "fontlist.hpp"
#ifdef _WIN32
#include <commctrl.h>
#endif
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <map>
#include <set>
#include "pdfedit.hpp"
#include "pdfium_api.hpp"
#ifdef _WIN32
#include "themeui.hpp"   // the window (Windows only; the Mac app shows the report with its own screens)
#endif

namespace fontlist {
namespace {

std::wstring widen(const std::string& s) {
    std::wstring w;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '#' && i + 2 < s.size() && isxdigit(static_cast<unsigned char>(s[i + 1])) && isxdigit(static_cast<unsigned char>(s[i + 2]))) {   // name escapes
            w += static_cast<wchar_t>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else w += static_cast<wchar_t>(static_cast<unsigned char>(s[i]));
    }
    return w;
}

// "/Name" value of a key, without the slash
std::string nameOf(const std::string& dict, const char* key) {
    size_t b, e;
    if (!pdfedit::valueSpan(dict, key, b, e)) return "";
    std::string v = dict.substr(b, e - b);
    const size_t s = v.find('/');
    if (s == std::string::npos) return "";
    size_t t = s + 1;
    while (t < v.size() && !strchr(" \t\r\n/<>[]()", v[t])) ++t;
    return v.substr(s + 1, t - s - 1);
}

// Font dictionary types by base font name, read from the file itself (PDFium does not report them). When fonts of
// different kinds share a name, all kinds are given ("TrueType / Type 0 (CID, TrueType)").
std::map<std::string, std::wstring> typesByName(Document& d) {
    std::map<std::string, std::wstring> out;
    std::vector<unsigned char> bytes;
    if (!d.serialize(bytes, FPDF_REMOVE_SECURITY)) return out;
    const std::string s(bytes.begin(), bytes.end());
    pdfedit::File f(s);
    if (!f.parseXref()) return out;
    for (const auto& kv : f.offsets) {
        std::string body;
        if (!f.object(kv.first, body) || body.find("/BaseFont") == std::string::npos) continue;
        const size_t st = body.find("stream");
        if (st != std::string::npos) body.resize(st);
        const std::string sub = nameOf(body, "/Subtype"), base = nameOf(body, "/BaseFont");
        if (base.empty()) continue;
        std::wstring type;
        if (sub == "Type1") type = body.find("/FontFile3") != std::string::npos ? L"Type 1 (CFF)" : L"Type 1";
        else if (sub == "MMType1") type = L"Type 1 (multiple master)";
        else if (sub == "TrueType") type = L"TrueType";
        else if (sub == "Type3") type = L"Type 3";
        else if (sub == "Type0") {
            type = L"Type 0 (CID)";
            size_t b, e;
            if (pdfedit::valueSpan(body, "/DescendantFonts", b, e)) {
                std::string arr = body.substr(b, e - b), desc;
                if (arr.find('[') == std::string::npos) f.object(atoi(arr.c_str()), arr);   // the array itself may be an object
                const size_t open = arr.find('[');
                if (open != std::string::npos && f.object(atoi(arr.c_str() + open + 1), desc)) {
                    const std::string ds = nameOf(desc, "/Subtype");
                    if (ds == "CIDFontType2") type = L"Type 0 (CID, TrueType)";
                    else if (ds == "CIDFontType0") type = L"Type 0 (CID, CFF)";
                }
            }
        } else continue;   // CIDFontType0/2 descendants: their Type 0 parent names them
        std::wstring& known = out[base];
        if (known.empty()) known = type;
        else if ((L" / " + known + L" / ").find(L" / " + type + L" / ") == std::wstring::npos) known += L" / " + type;
    }
    return out;
}

struct Acc {
    Font font;
    std::set<int> pages;
};

void scan(FPDF_PAGEOBJECT o, int page, std::map<FPDF_FONT, Acc>& acc, int depth, bool& anyText, bool& anyVisible) {
    const auto& api = pdfium::api();
    const int t = api.FPDFPageObj_GetType(o);
    if (t == FPDF_PAGEOBJ_FORM && depth < 12) {
        const int n = api.FPDFFormObj_CountObjects(o);
        for (int i = 0; i < n; ++i)
            if (FPDF_PAGEOBJECT c = api.FPDFFormObj_GetObject(o, static_cast<unsigned long>(i))) scan(c, page, acc, depth + 1, anyText, anyVisible);
        return;
    }
    if (t != FPDF_PAGEOBJ_TEXT) return;
    FPDF_FONT f = api.FPDFTextObj_GetFont(o);
    if (!f) return;
    const bool invisible = api.FPDFTextObj_GetTextRenderMode(o) == FPDF_TEXTRENDERMODE_INVISIBLE;
    anyText = true;
    if (!invisible) anyVisible = true;
    Acc& a = acc[f];
    a.font.embedded = api.FPDFFont_GetIsEmbedded(f) != 0;
    (invisible ? a.font.invisibleRuns : a.font.visibleRuns)++;
    a.pages.insert(page);
}

std::string baseName(FPDF_FONT f) {
    const size_t n = pdfium::api().FPDFFont_GetBaseFontName(f, nullptr, 0);
    std::string s(n, '\0');
    if (n) pdfium::api().FPDFFont_GetBaseFontName(f, s.data(), n);
    while (!s.empty() && s.back() == '\0') s.pop_back();
    return s;
}

std::wstring textUse(const Font& f) {
    if (!f.invisibleRuns) return L"visible";
    if (!f.visibleRuns) return L"invisible only (OCR text layer)";
    return L"visible and invisible";
}

#ifdef _WIN32
// ---- the window ----
enum { ID_LIST = 201, ID_COPY, ID_CLOSE, ID_SUMMARY };

struct View {
    const Report* r = nullptr;
    std::wstring summary;
    bool done = false;
    HFONT font = nullptr;
};

void layout(HWND w) {
    RECT rc;
    GetClientRect(w, &rc);
    const int pad = 12, bh = 28, sumH = 92;
    MoveWindow(GetDlgItem(w, ID_SUMMARY), pad, pad, rc.right - 2 * pad, sumH, TRUE);
    MoveWindow(GetDlgItem(w, ID_LIST), pad, pad + sumH + 6, rc.right - 2 * pad, rc.bottom - sumH - bh - 3 * pad - 6, TRUE);
    MoveWindow(GetDlgItem(w, ID_COPY), rc.right - 2 * pad - 200, rc.bottom - pad - bh, 100, bh, TRUE);
    MoveWindow(GetDlgItem(w, ID_CLOSE), rc.right - pad - 90, rc.bottom - pad - bh, 90, bh, TRUE);
}

LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    auto* v = reinterpret_cast<View*>(GetWindowLongPtrW(w, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            v = static_cast<View*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(v));
            HINSTANCE inst = reinterpret_cast<CREATESTRUCTW*>(lp)->hInstance;
            NONCLIENTMETRICSW ncm{sizeof(ncm)};
            SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
            v->font = CreateFontIndirectW(&ncm.lfMessageFont);
            auto mk = [&](DWORD ex, const wchar_t* cls, const wchar_t* text, DWORD style, int id) {
                HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, w, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), inst, nullptr);
                SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(v->font), TRUE);
                return c;
            };
            mk(0, L"STATIC", v->summary.c_str(), SS_LEFT, ID_SUMMARY);
            HWND lv = mk(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | WS_TABSTOP, ID_LIST);
            ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
            static const wchar_t* cols[] = {L"Font", L"Type", L"Embedded", L"Subset", L"Text", L"Pages"};
            static const int widths[] = {230, 150, 80, 60, 190, 140};
            for (int i = 0; i < 6; ++i) {
                LVCOLUMNW c{};
                c.mask = LVCF_TEXT | LVCF_WIDTH;
                c.pszText = const_cast<wchar_t*>(cols[i]);
                c.cx = widths[i];
                ListView_InsertColumn(lv, i, &c);
            }
            for (size_t i = 0; i < v->r->fonts.size(); ++i) {
                const Font& f = v->r->fonts[i];
                const std::wstring cells[] = {f.name, f.type.empty() ? L"-" : f.type, f.embedded ? L"yes" : L"NO", f.subset ? L"yes" : L"no", textUse(f), pageList(f.pages)};
                LVITEMW it{};
                it.mask = LVIF_TEXT;
                it.iItem = static_cast<int>(i);
                it.pszText = const_cast<wchar_t*>(cells[0].c_str());
                ListView_InsertItem(lv, &it);
                for (int c = 1; c < 6; ++c) ListView_SetItemText(lv, static_cast<int>(i), c, const_cast<wchar_t*>(cells[c].c_str()));
            }
            mk(0, L"BUTTON", L"Copy", WS_TABSTOP | BS_PUSHBUTTON, ID_COPY);
            mk(0, L"BUTTON", L"Close", WS_TABSTOP | BS_DEFPUSHBUTTON, ID_CLOSE);
            layout(w);
            return 0;
        }
        case WM_SIZE: layout(w); return 0;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
            SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        case WM_COMMAND:
            if (LOWORD(wp) == ID_COPY) {
                const std::wstring t = asText(*v->r);
                if (OpenClipboard(w)) {
                    EmptyClipboard();
                    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (t.size() + 1) * sizeof(wchar_t));
                    if (g) {
                        memcpy(GlobalLock(g), t.c_str(), (t.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(g);
                        if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);
                    }
                    CloseClipboard();
                }
                return 0;
            }
            if (LOWORD(wp) == ID_CLOSE || LOWORD(wp) == IDCANCEL) { v->done = true; return 0; }
            break;
        case WM_CLOSE: v->done = true; return 0;
        case WM_DESTROY: if (v && v->font) DeleteObject(v->font); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}
#endif

} // namespace

std::wstring pageList(const std::vector<int>& pages) {
    std::wstring out;
    for (size_t i = 0; i < pages.size();) {
        size_t j = i;
        while (j + 1 < pages.size() && pages[j + 1] == pages[j] + 1) ++j;
        if (!out.empty()) out += L", ";
        out += std::to_wstring(pages[i] + 1);
        if (j > i) out += L"-" + std::to_wstring(pages[j] + 1);
        i = j + 1;
    }
    return out;
}

Report build(Document& d) {
    const auto& api = pdfium::api();
    Report r;
    r.pageCount = d.pageCount();
    std::map<FPDF_FONT, Acc> acc;
    std::map<FPDF_FONT, std::string> names;
    for (int p = 0; p < d.pageCount(); ++p) {
        FPDF_PAGE pg = d.page(p);
        if (!pg) continue;
        bool anyText = false, anyVisible = false;
        const int n = api.FPDFPage_CountObjects(pg);
        for (int i = 0; i < n; ++i)
            if (FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i)) scan(o, p, acc, 0, anyText, anyVisible);
        if (!anyText) r.pagesWithoutText.push_back(p);
        else if (!anyVisible) r.pagesInvisibleOnly.push_back(p);
    }
    const auto types = typesByName(d);
    // the same font can be loaded more than once (once per page that uses it): merge by name and kind
    std::map<std::wstring, Font> merged;
    for (auto& kv : acc) {
        std::string base = baseName(kv.first);
        Font f = kv.second.font;
        auto t = types.find(base);
        if (t != types.end()) f.type = t->second;
        if (base.size() > 7 && base[6] == '+' && std::all_of(base.begin(), base.begin() + 6, [](char c) { return c >= 'A' && c <= 'Z'; })) { f.subset = true; base.erase(0, 7); }
        f.name = base.empty() ? L"(no name)" : widen(base);
        if (f.type.empty() && base.empty()) f.type = L"Type 3";
        const std::wstring key = f.name + L"|" + f.type + L"|" + (f.embedded ? L"e" : L"") + (f.subset ? L"s" : L"");
        Font& m = merged[key];
        if (m.name.empty()) { m = f; m.visibleRuns = m.invisibleRuns = 0; }
        m.visibleRuns += f.visibleRuns;
        m.invisibleRuns += f.invisibleRuns;
        std::set<int> pages(m.pages.begin(), m.pages.end());
        pages.insert(kv.second.pages.begin(), kv.second.pages.end());
        m.pages.assign(pages.begin(), pages.end());
    }
    for (auto& kv : merged) r.fonts.push_back(kv.second);
    std::stable_sort(r.fonts.begin(), r.fonts.end(), [](const Font& a, const Font& b) { return std::lexicographical_compare(a.name.begin(), a.name.end(), b.name.begin(), b.name.end(),
        [](wchar_t x, wchar_t y) { return towlower(x) < towlower(y); }); });
    return r;
}

std::wstring asText(const Report& r) {
    std::wstring t = L"Font\tType\tEmbedded\tSubset\tText\tPages\r\n";
    for (const Font& f : r.fonts)
        t += f.name + L"\t" + (f.type.empty() ? L"-" : f.type) + L"\t" + (f.embedded ? L"yes" : L"no") + L"\t" + (f.subset ? L"yes" : L"no") + L"\t" + textUse(f) + L"\t" + pageList(f.pages) + L"\r\n";
    t += L"\r\nPages: " + std::to_wstring(r.pageCount) + L"\r\n";
    t += L"Pages without any text (scanned; OCR can find their words): " + (r.pagesWithoutText.empty() ? L"none" : pageList(r.pagesWithoutText)) + L"\r\n";
    t += L"Pages with only invisible text (already recognised by OCR): " + (r.pagesInvisibleOnly.empty() ? L"none" : pageList(r.pagesInvisibleOnly)) + L"\r\n";
    return t;
}

#ifdef _WIN32
void show(HWND parent, HINSTANCE inst, const Report& r, const std::wstring& title) {
    static bool registered = false;
    if (!registered) {
        INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&icc);
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        wc.lpszClassName = L"FeatherFontList";
        registered = RegisterClassExW(&wc) != 0;
    }
    View v;
    v.r = &r;
    int notEmbedded = 0, visible = 0;
    for (const Font& f : r.fonts) { if (f.visibleRuns) ++visible; if (!f.embedded && f.visibleRuns) ++notEmbedded; }
    v.summary = std::to_wstring(r.fonts.size()) + L" font(s) on " + std::to_wstring(r.pageCount) + L" page(s)";
    if (!visible) v.summary += L"; no visible text.";
    else v.summary += notEmbedded ? L"; " + std::to_wstring(notEmbedded) + L" visible font(s) not embedded (other computers show a similar font instead)." : L"; every visible font is embedded.";
    v.summary += L"\nPages without any text (scanned; OCR can find their words): " + (r.pagesWithoutText.empty() ? L"none" : pageList(r.pagesWithoutText)) + L".";
    v.summary += L"\nPages with only invisible text (already recognised by OCR): " + (r.pagesInvisibleOnly.empty() ? L"none" : pageList(r.pagesInvisibleOnly)) + L".";
    if (r.fonts.empty()) v.summary = L"This PDF has no text at all, so it uses no fonts. Its pages are pictures (scans): Recognise text (OCR) can find their words.";
    RECT pr;
    GetWindowRect(parent, &pr);
    const int wdt = 900, h = 520;
    const int x = pr.left + std::max(0L, (pr.right - pr.left - wdt) / 2), y = pr.top + std::max(0L, (pr.bottom - pr.top - h) / 2);
    HWND w = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FeatherFontList", title.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE,
                             x, y, wdt, h, parent, nullptr, inst, &v);
    if (!w) return;
    themeui::themeDialog(w);
    EnableWindow(parent, FALSE);
    MSG msg;
    while (!v.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { v.done = true; continue; }
        if (!IsDialogMessageW(w, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    EnableWindow(parent, TRUE);
    DestroyWindow(w);
    SetActiveWindow(parent);
}
#endif

std::wstring squeeze(const std::wstring& s) {   // lower case, letters and digits only: "Times New Roman" -> "timesnewroman"
    std::wstring r;
    for (wchar_t c : s) if (iswalnum(c)) r += static_cast<wchar_t>(towlower(c));
    return r;
}

// The installed font family a PDF font name stands for, or "" when it is not installed. "ABCDEF+Calibri-Bold",
// "Calibri,Bold", "ArialMT" and "TimesNewRomanPSMT" become Calibri, Calibri, Arial and Times New Roman.
std::wstring installedFace(const std::string& pdfName) {
    static std::vector<std::wstring> families;
    static bool listed = false;
    if (!listed) {
        listed = true;
#ifdef _WIN32
        HDC dc = GetDC(nullptr);
        LOGFONTW lf{};
        lf.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(dc, &lf, [](const LOGFONTW* f, const TEXTMETRICW*, DWORD, LPARAM out) -> int {
            if (f->lfFaceName[0] != L'@' && f->lfCharSet != SYMBOL_CHARSET) reinterpret_cast<std::vector<std::wstring>*>(out)->push_back(f->lfFaceName);
            return 1;
        }, reinterpret_cast<LPARAM>(&families), 0);
        ReleaseDC(nullptr, dc);
#endif
        // macOS: the installed fonts are not listed yet, so edited text uses the standard PDF fonts (a later stage).
    }
    std::string n = pdfName;
    const size_t plus = n.find('+');
    if (plus == 6) n = n.substr(7);   // subset prefix
    n = n.substr(0, n.find_first_of(",-"));
    std::wstring w(n.begin(), n.end());
    w = squeeze(w);
    for (const wchar_t* tail : {L"psmt", L"mt", L"ps", L"regular", L"roman"})
        if (w.size() > wcslen(tail) + 2 && w.compare(w.size() - wcslen(tail), std::wstring::npos, tail) == 0) { w.resize(w.size() - wcslen(tail)); break; }
    if (w.empty()) return L"";
    for (const auto& f : families) if (squeeze(f) == w) return f;
    return L"";
}


} // namespace fontlist
