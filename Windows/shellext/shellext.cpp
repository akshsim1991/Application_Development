// FeatherPDFShell.dll: "Combine into one PDF" in File Explorer's first right-click menu on Windows 11.
//
// Windows 11 shows only packaged commands in that menu: this DLL is the command (an IExplorerCommand, a COM class), and
// the sparse package made by Windows/shellext/make-package.py declares it for the file types Feather PDF can combine,
// with FeatherPDF.exe as the package's program (the package has no files of its own: they stay in the program folder).
// The installer trusts the package's certificate and registers the package; the classic entry ("Show more options",
// Windows/src/explorermenu.hpp) stays in any case.
//
// The command is shown when two or more files are selected and every one of them is a PDF or a file Feather PDF
// makes into a PDF. It writes their paths to a temporary list and starts "FeatherPDF.exe --combine-list <list>", which
// combines them all at once (Windows/src/main.cpp, combineFiles).
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <new>
#include <string>
#include <vector>

namespace {

// {89A01A5F-1EB5-4A99-9F4E-51CCED32C827}: the same in AppxManifest.xml
const CLSID kClsid = {0x89a01a5f, 0x1eb5, 0x4a99, {0x9f, 0x4e, 0x51, 0xcc, 0xed, 0x32, 0xc8, 0x27}};

HMODULE g_module = nullptr;
LONG g_objects = 0, g_locks = 0;

// The files Feather PDF combines (as Windows/src/explorermenu.hpp's kTypes, and PDFs)
const wchar_t* const kTypes[] = {L".pdf", L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".tif", L".tiff", L".txt", L".md", L".markdown",
                                 L".ppt", L".pptx", L".pps", L".ppsx", L".odp", L".doc", L".docx", L".rtf", L".odt", L".xls", L".xlsx", L".ods"};

bool combinable(const std::wstring& path) {
    const wchar_t* ext = PathFindExtensionW(path.c_str());
    for (const wchar_t* t : kTypes) if (_wcsicmp(ext, t) == 0) return true;
    return false;
}

// The file system paths of the selected items (empty when one of them is not a file Feather PDF can combine)
std::vector<std::wstring> pathsOf(IShellItemArray* items, size_t limit) {
    std::vector<std::wstring> out;
    DWORD n = 0;
    if (!items || FAILED(items->GetCount(&n))) return out;
    for (DWORD i = 0; i < n && out.size() < limit; ++i) {
        IShellItem* it = nullptr;
        if (FAILED(items->GetItemAt(i, &it))) return {};
        PWSTR p = nullptr;
        const HRESULT hr = it->GetDisplayName(SIGDN_FILESYSPATH, &p);
        it->Release();
        if (FAILED(hr) || !p) return {};
        std::wstring path(p);
        CoTaskMemFree(p);
        if (!combinable(path)) return {};
        out.push_back(path);
    }
    return out;
}

std::wstring programFolder() {
    wchar_t path[MAX_PATH * 2];
    const DWORD n = GetModuleFileNameW(g_module, path, MAX_PATH * 2);
    std::wstring s(path, n);
    return s.substr(0, s.find_last_of(L"\\/") + 1);
}

class CombineCommand : public IExplorerCommand {
public:
    CombineCommand() { InterlockedIncrement(&g_objects); }
    virtual ~CombineCommand() { InterlockedDecrement(&g_objects); }

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IExplorerCommand) { *ppv = static_cast<IExplorerCommand*>(this); AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    IFACEMETHODIMP_(ULONG) Release() override {
        const LONG r = InterlockedDecrement(&refs_);
        if (r == 0) delete this;
        return r;
    }

    // IExplorerCommand
    IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* name) override { return SHStrDupW(L"Combine into one PDF", name); }
    IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* icon) override { return SHStrDupW((programFolder() + L"FeatherPDF.exe,0").c_str(), icon); }
    IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* tip) override { *tip = nullptr; return E_NOTIMPL; }
    IFACEMETHODIMP GetCanonicalName(GUID* guid) override { *guid = kClsid; return S_OK; }
    IFACEMETHODIMP GetState(IShellItemArray* items, BOOL, EXPCMDSTATE* state) override {
        // two or more files, all of them PDFs or files Feather PDF makes into PDFs (only the first 500 are looked at)
        DWORD n = 0;
        *state = ECS_HIDDEN;
        if (!items || FAILED(items->GetCount(&n)) || n < 2) return S_OK;
        if (pathsOf(items, 500).size() >= 2) *state = ECS_ENABLED;
        return S_OK;
    }
    IFACEMETHODIMP Invoke(IShellItemArray* items, IBindCtx*) override {
        const std::vector<std::wstring> paths = pathsOf(items, SIZE_MAX);
        if (paths.size() < 2) return S_OK;
        // the list, as UTF-16 with a byte order mark, in the temporary folder (FeatherPDF.exe deletes it)
        wchar_t dir[MAX_PATH], file[MAX_PATH];
        if (!GetTempPathW(MAX_PATH, dir) || !GetTempFileNameW(dir, L"fpc", 0, file)) return E_FAIL;
        HANDLE h = CreateFileW(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (h == INVALID_HANDLE_VALUE) return E_FAIL;
        std::wstring text(1, wchar_t(0xFEFF));
        for (const auto& p : paths) text += p + L"\r\n";
        DWORD written = 0;
        const BOOL ok = WriteFile(h, text.data(), static_cast<DWORD>(text.size() * sizeof(wchar_t)), &written, nullptr);
        CloseHandle(h);
        if (!ok) { DeleteFileW(file); return E_FAIL; }
        const std::wstring exe = programFolder() + L"FeatherPDF.exe";
        std::wstring cmd = L"\"" + exe + L"\" --combine-list \"" + file + L"\"";
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { DeleteFileW(file); return E_FAIL; }
        AllowSetForegroundWindow(pi.dwProcessId);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return S_OK;
    }
    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override { *flags = ECF_DEFAULT; return S_OK; }
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** e) override { *e = nullptr; return E_NOTIMPL; }

private:
    LONG refs_ = 1;
};

class Factory : public IClassFactory {
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) { *ppv = static_cast<IClassFactory*>(this); AddRef(); return S_OK; }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    IFACEMETHODIMP_(ULONG) AddRef() override { return 2; }    // one static object
    IFACEMETHODIMP_(ULONG) Release() override { return 1; }
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;
        CombineCommand* c = new (std::nothrow) CombineCommand();
        if (!c) return E_OUTOFMEMORY;
        const HRESULT hr = c->QueryInterface(riid, ppv);
        c->Release();
        return hr;
    }
    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock) InterlockedIncrement(&g_locks); else InterlockedDecrement(&g_locks);
        return S_OK;
    }
};
Factory g_factory;

} // namespace

extern "C" BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_module = inst; DisableThreadLibraryCalls(inst); }
    return TRUE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (clsid != kClsid) return CLASS_E_CLASSNOTAVAILABLE;
    return g_factory.QueryInterface(riid, ppv);
}

extern "C" HRESULT __stdcall DllCanUnloadNow() { return g_objects == 0 && g_locks == 0 ? S_OK : S_FALSE; }
