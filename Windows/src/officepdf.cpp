#include "officepdf.hpp"
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <shellapi.h>
#include <cwctype>
#include <vector>

namespace officepdf {

namespace {

const wchar_t* const kExts[] = {L".ppt", L".pptx", L".pps", L".ppsx", L".odp", L".doc", L".docx", L".rtf", L".odt", L".xls", L".xlsx", L".ods"};

std::wstring lowerExt(const std::wstring& p) {
    const size_t slash = p.find_last_of(L"\\/"), dot = p.find_last_of(L'.');
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash)) return L"";
    std::wstring e = p.substr(dot);
    for (auto& c : e) c = static_cast<wchar_t>(std::towlower(c));
    return e;
}

bool exists(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// ---- late-bound automation: obj.Name(args...) and obj.Name / obj.Name = v ----

struct Var {
    VARIANT v;
    Var() { VariantInit(&v); }
    explicit Var(const std::wstring& s) { VariantInit(&v); v.vt = VT_BSTR; v.bstrVal = SysAllocString(s.c_str()); }
    explicit Var(long n) { VariantInit(&v); v.vt = VT_I4; v.lVal = n; }
    static Var boolean(bool b) { Var x; x.v.vt = VT_BOOL; x.v.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE; return x; }
    Var(const Var& o) { VariantInit(&v); VariantCopy(&v, const_cast<VARIANT*>(&o.v)); }
    Var& operator=(const Var& o) { if (this != &o) { VariantClear(&v); VariantCopy(&v, const_cast<VARIANT*>(&o.v)); } return *this; }
    ~Var() { VariantClear(&v); }
    IDispatch* dispatch() const { return v.vt == VT_DISPATCH ? v.pdispVal : nullptr; }
    long number() const { VARIANT t; VariantInit(&t); long n = 0; if (SUCCEEDED(VariantChangeType(&t, const_cast<VARIANT*>(&v), 0, VT_I4))) n = t.lVal; VariantClear(&t); return n; }
};

// Calls `name` on `obj` (`kind`: DISPATCH_METHOD, DISPATCH_PROPERTYGET or DISPATCH_PROPERTYPUT) with the arguments in
// their written order. The program's own message comes back in `error` when it refuses.
bool invoke(IDispatch* obj, WORD kind, const wchar_t* name, std::vector<Var> args, Var* result, std::wstring& error) {
    if (!obj) { error = L"the program did not answer"; return false; }
    DISPID id;
    LPOLESTR n = const_cast<LPOLESTR>(name);
    if (FAILED(obj->GetIDsOfNames(IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id))) { error = std::wstring(L"the program has no ") + name; return false; }
    std::vector<VARIANT> rev;   // automation takes the arguments last first
    for (auto it = args.rbegin(); it != args.rend(); ++it) rev.push_back(it->v);
    DISPPARAMS dp{rev.empty() ? nullptr : rev.data(), nullptr, static_cast<UINT>(rev.size()), 0};
    DISPID put = DISPID_PROPERTYPUT;
    if (kind == DISPATCH_PROPERTYPUT) { dp.rgdispidNamedArgs = &put; dp.cNamedArgs = 1; }
    EXCEPINFO ex{};
    VARIANT res; VariantInit(&res);
    const HRESULT hr = obj->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, kind, &dp, &res, &ex, nullptr);
    if (FAILED(hr)) {
        error = ex.bstrDescription ? std::wstring(ex.bstrDescription) : std::wstring(name) + L" failed (" + std::to_wstring(static_cast<long>(hr)) + L")";
        if (ex.bstrDescription) SysFreeString(ex.bstrDescription);
        if (ex.bstrSource) SysFreeString(ex.bstrSource);
        if (ex.bstrHelpFile) SysFreeString(ex.bstrHelpFile);
        VariantClear(&res);
        return false;
    }
    if (result) { VariantClear(&result->v); result->v = res; } else VariantClear(&res);
    return true;
}
bool call(IDispatch* o, const wchar_t* n, std::vector<Var> a, Var* r, std::wstring& e) { return invoke(o, DISPATCH_METHOD, n, std::move(a), r, e); }
bool get(IDispatch* o, const wchar_t* n, Var* r, std::wstring& e) { return invoke(o, DISPATCH_PROPERTYGET, n, {}, r, e); }
bool put(IDispatch* o, const wchar_t* n, Var v, std::wstring& e) { return invoke(o, DISPATCH_PROPERTYPUT, n, {std::move(v)}, nullptr, e); }

// The program by its automation name ("PowerPoint.Application"); nullptr when it is not installed.
IDispatch* start(const wchar_t* progId) {
    CLSID clsid;
    if (FAILED(CLSIDFromProgID(progId, &clsid))) return nullptr;
    IDispatch* d = nullptr;
    if (FAILED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&d)))) return nullptr;
    return d;
}

// The program is closed again when it has nothing else open (it may have been open with the user's own files).
void quitIfIdle(IDispatch* app, const wchar_t* collection) {
    std::wstring e;
    Var c, n;
    if (get(app, collection, &c, e) && get(c.dispatch(), L"Count", &n, e) && n.number() == 0) call(app, L"Quit", {}, nullptr, e);
}

bool slides(const wchar_t* progId, const std::wstring& in, const std::wstring& out, std::wstring& error) {
    IDispatch* app = start(progId);
    if (!app) return false;
    std::wstring e;
    Var pres, list;
    bool ok = get(app, L"Presentations", &list, e) &&
              // Open(FileName, ReadOnly = msoTrue, Untitled = msoFalse, WithWindow = msoFalse)
              call(list.dispatch(), L"Open", {Var(in), Var(-1L), Var(0L), Var(0L)}, &pres, e) &&
              call(pres.dispatch(), L"SaveAs", {Var(out), Var(32L)}, nullptr, e);   // ppSaveAsPDF
    if (pres.dispatch()) { std::wstring e2; call(pres.dispatch(), L"Close", {}, nullptr, e2); }
    quitIfIdle(app, L"Presentations");
    app->Release();
    if (!ok) error = e;
    return ok;
}

bool text(const wchar_t* progId, const std::wstring& in, const std::wstring& out, std::wstring& error) {
    IDispatch* app = start(progId);
    if (!app) return false;
    std::wstring e;
    put(app, L"DisplayAlerts", Var(0L), e);   // wdAlertsNone
    Var doc, list;
    bool ok = get(app, L"Documents", &list, e) &&
              // Open(FileName, ConfirmConversions = False, ReadOnly = True, AddToRecentFiles = False)
              call(list.dispatch(), L"Open", {Var(in), Var::boolean(false), Var::boolean(true), Var::boolean(false)}, &doc, e) &&
              call(doc.dispatch(), L"ExportAsFixedFormat", {Var(out), Var(17L)}, nullptr, e);   // wdExportFormatPDF
    if (doc.dispatch()) { std::wstring e2; call(doc.dispatch(), L"Close", {Var(0L)}, nullptr, e2); }   // wdDoNotSaveChanges
    quitIfIdle(app, L"Documents");
    app->Release();
    if (!ok) error = e;
    return ok;
}

bool sheet(const wchar_t* progId, const std::wstring& in, const std::wstring& out, std::wstring& error) {
    IDispatch* app = start(progId);
    if (!app) return false;
    std::wstring e;
    put(app, L"DisplayAlerts", Var::boolean(false), e);
    Var book, list;
    bool ok = get(app, L"Workbooks", &list, e) &&
              // Open(Filename, UpdateLinks = 0, ReadOnly = True)
              call(list.dispatch(), L"Open", {Var(in), Var(0L), Var::boolean(true)}, &book, e) &&
              call(book.dispatch(), L"ExportAsFixedFormat", {Var(0L), Var(out)}, nullptr, e);   // xlTypePDF, Filename
    if (book.dispatch()) { std::wstring e2; call(book.dispatch(), L"Close", {Var::boolean(false)}, nullptr, e2); }
    quitIfIdle(app, L"Workbooks");
    app->Release();
    if (!ok) error = e;
    return ok;
}

// LibreOffice's soffice.exe: from its registry entry, else the usual folders.
std::wstring sofficePath() {
    wchar_t buf[MAX_PATH * 2];
    DWORD size = sizeof(buf);
    for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER}) {
        size = sizeof(buf);
        if (RegGetValueW(root, L"SOFTWARE\\LibreOffice\\UNO\\InstallPath", nullptr, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) {
            const std::wstring p = std::wstring(buf) + L"\\soffice.exe";
            if (exists(p)) return p;
        }
    }
    for (const wchar_t* env : {L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramW6432"}) {
        if (!GetEnvironmentVariableW(env, buf, MAX_PATH)) continue;
        const std::wstring p = std::wstring(buf) + L"\\LibreOffice\\program\\soffice.exe";
        if (exists(p)) return p;
    }
    return L"";
}

bool libreOffice(const std::wstring& in, const std::wstring& out, std::wstring& error) {
    const std::wstring soffice = sofficePath();
    if (soffice.empty()) return false;
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    const std::wstring dir = std::wstring(tmp) + L"FeatherPDF-convert-" + std::to_wstring(GetCurrentProcessId());
    CreateDirectoryW(dir.c_str(), nullptr);
    // a profile of its own, so a LibreOffice already open does not take the job (and ignore it)
    std::wstring profile = dir + L"\\profile";
    for (auto& c : profile) if (c == L'\\') c = L'/';
    std::wstring cmd = L"\"" + soffice + L"\" -env:UserInstallation=file:///" + profile + L" --headless --norestore --convert-to pdf --outdir \"" + dir + L"\" \"" + in + L"\"";
    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { error = L"LibreOffice could not be started"; return false; }
    const bool finished = WaitForSingleObject(pi.hProcess, 180000) == WAIT_OBJECT_0;
    if (!finished) TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    const size_t slash = in.find_last_of(L"\\/"), dot = in.find_last_of(L'.');
    const std::wstring made = dir + L"\\" + in.substr(slash + 1, dot - slash - 1) + L".pdf";
    const bool ok = finished && exists(made) && MoveFileExW(made.c_str(), out.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);
    if (!ok) error = finished ? L"LibreOffice made no PDF of it" : L"LibreOffice took more than three minutes";
    SHFILEOPSTRUCTW op{};   // the folder and its profile go
    std::wstring from = dir + L'\0';
    op.wFunc = FO_DELETE; op.pFrom = from.c_str(); op.fFlags = FOF_NO_UI;
    SHFileOperationW(&op);
    return ok;
}

} // namespace

Kind kindOf(const std::wstring& path) {
    const std::wstring e = lowerExt(path);
    if (e == L".ppt" || e == L".pptx" || e == L".pps" || e == L".ppsx" || e == L".odp") return Kind::Slides;
    if (e == L".doc" || e == L".docx" || e == L".rtf" || e == L".odt") return Kind::Text;
    if (e == L".xls" || e == L".xlsx" || e == L".ods") return Kind::Sheet;
    return Kind::None;
}

std::wstring selfTest(const std::wstring& existingFile) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::wstring rep, e;
    IDispatch* fso = nullptr;
    CLSID clsid;
    if (SUCCEEDED(CLSIDFromProgID(L"Scripting.FileSystemObject", &clsid)))
        CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&fso));
    if (!fso) return L"no FileSystemObject\n";
    Var r;
    rep += L"FileExists(file)=" + std::wstring(call(fso, L"FileExists", {Var(existingFile)}, &r, e) && r.v.vt == VT_BOOL && r.v.boolVal ? L"true" : L"false") + L"\n";
    rep += L"FileExists(none)=" + std::wstring(call(fso, L"FileExists", {Var(existingFile + L".none")}, &r, e) && r.v.vt == VT_BOOL && r.v.boolVal ? L"true" : L"false") + L"\n";
    // two arguments, in order: BuildPath("C:\\a", "b") is "C:\\a\\b"
    rep += L"BuildPath=" + std::wstring(call(fso, L"BuildPath", {Var(std::wstring(L"C:\\a")), Var(std::wstring(L"b"))}, &r, e) && r.v.vt == VT_BSTR ? r.v.bstrVal : L"?") + L"\n";
    Var drives, count;
    rep += L"Drives.Count=" + std::to_wstring(get(fso, L"Drives", &drives, e) && get(drives.dispatch(), L"Count", &count, e) ? count.number() : -1) + L"\n";
    rep += L"NoSuchMethod=" + std::wstring(call(fso, L"NoSuchMethod", {}, nullptr, e) ? L"called" : L"refused: " + e) + L"\n";
    fso->Release();
    CoUninitialize();
    return rep;
}

const wchar_t* const* extensions(int& count) { count = static_cast<int>(sizeof(kExts) / sizeof(kExts[0])); return kExts; }

bool convert(const std::wstring& in, const std::wstring& out, std::wstring& program, std::wstring& error) {
    const Kind k = kindOf(in);
    if (k == Kind::None) { error = L"This is not an Office file."; return false; }
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::wstring why;
    auto tryOne = [&](const wchar_t* name, bool (*fn)(const wchar_t*, const std::wstring&, const std::wstring&, std::wstring&), const wchar_t* progId) {
        std::wstring e;
        if (fn(progId, in, out, e) && exists(out)) { program = name; return true; }
        if (!e.empty()) why += std::wstring(L"\n") + name + L": " + e;
        return false;
    };
    bool ok = false;
    switch (k) {
        case Kind::Slides: ok = tryOne(L"Microsoft PowerPoint", slides, L"PowerPoint.Application") || tryOne(L"WPS Presentation", slides, L"KWPP.Application"); break;
        case Kind::Text: ok = tryOne(L"Microsoft Word", text, L"Word.Application") || tryOne(L"WPS Writer", text, L"KWPS.Application"); break;
        case Kind::Sheet: ok = tryOne(L"Microsoft Excel", sheet, L"Excel.Application") || tryOne(L"WPS Spreadsheets", sheet, L"KET.Application"); break;
        default: break;
    }
    if (!ok) {
        std::wstring e;
        if (libreOffice(in, out, e)) { program = L"LibreOffice"; ok = true; }
        else if (!e.empty()) why += L"\nLibreOffice: " + e;
    }
    if (SUCCEEDED(co)) CoUninitialize();
    if (!ok) error = why.empty() ? L"Feather PDF makes Office files into PDFs with the office program on this PC (Microsoft Office, WPS Office or LibreOffice), and none of them was found."
                                 : L"The office program could not make a PDF of it:" + why;
    return ok;
}

} // namespace officepdf
