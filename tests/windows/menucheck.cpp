// Checks FeatherPDFShell.dll (Windows 11's "Combine into one PDF") on Windows: its title, when it is shown, and that
// it hands every selected file to FeatherPDF.exe (here the stand-in tests/windows/stub.cpp). Usage: menucheck <folder>
// with FeatherPDFShell.dll and FeatherPDF.exe in it. Prints ok/FAIL lines; exit code 0 when all pass.
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <cstdio>
#include <string>
#include <vector>

static int fails = 0;
static void check(bool ok, const char* what) { std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what); if (!ok) ++fails; }

static IShellItemArray* itemsOf(const std::vector<std::wstring>& paths) {
    std::vector<PIDLIST_ABSOLUTE> ids;
    for (const auto& p : paths) ids.push_back(ILCreateFromPathW(p.c_str()));
    IShellItemArray* a = nullptr;
    SHCreateShellItemArrayFromIDLists(static_cast<UINT>(ids.size()), const_cast<PCIDLIST_ABSOLUTE_ARRAY>(ids.data()), &a);
    for (auto id : ids) ILFree(id);
    return a;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return 2;
    const std::wstring dir = argv[1];
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HMODULE m = LoadLibraryW((dir + L"\\FeatherPDFShell.dll").c_str());
    check(m != nullptr, "the DLL loads");
    if (!m) return 1;
    using GetCO = HRESULT(__stdcall*)(REFCLSID, REFIID, void**);
    auto get = reinterpret_cast<GetCO>(reinterpret_cast<void*>(GetProcAddress(m, "DllGetClassObject")));
    const CLSID clsid = {0x89a01a5f, 0x1eb5, 0x4a99, {0x9f, 0x4e, 0x51, 0xcc, 0xed, 0x32, 0xc8, 0x27}};
    IClassFactory* f = nullptr;
    check(get && SUCCEEDED(get(clsid, IID_IClassFactory, reinterpret_cast<void**>(&f))) && f, "its class factory");
    if (!f) return 1;
    IExplorerCommand* c = nullptr;
    check(SUCCEEDED(f->CreateInstance(nullptr, IID_IExplorerCommand, reinterpret_cast<void**>(&c))) && c, "the command");
    if (!c) return 1;
    // files to select
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    const std::wstring t(tmp);
    std::vector<std::wstring> pdfs = {t + L"fp test a.pdf", t + L"fp test b.pdf", t + L"fp test c.PDF"};
    const std::wstring other = t + L"fp test d.exe";
    for (const auto& p : pdfs) { HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr); CloseHandle(h); }
    { HANDLE h = CreateFileW(other.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr); CloseHandle(h); }

    LPWSTR title = nullptr;
    check(SUCCEEDED(c->GetTitle(nullptr, &title)) && title && std::wstring(title) == L"Combine into one PDF", "title: Combine into one PDF");
    LPWSTR icon = nullptr;
    check(SUCCEEDED(c->GetIcon(nullptr, &icon)) && icon && std::wstring(icon).find(L"FeatherPDF.exe") != std::wstring::npos, "icon: FeatherPDF.exe's");
    EXPCMDSTATE st = ECS_ENABLED;
    IShellItemArray* three = itemsOf(pdfs);
    check(three && SUCCEEDED(c->GetState(three, TRUE, &st)) && st == ECS_ENABLED, "shown for three PDFs");
    IShellItemArray* one = itemsOf({pdfs[0]});
    check(one && SUCCEEDED(c->GetState(one, TRUE, &st)) && st == ECS_HIDDEN, "hidden for one PDF");
    IShellItemArray* mixed = itemsOf({pdfs[0], other});
    check(mixed && SUCCEEDED(c->GetState(mixed, TRUE, &st)) && st == ECS_HIDDEN, "hidden when a file is not one it combines");
    // Invoke: the stand-in FeatherPDF.exe copies the list it gets
    const std::wstring got = dir + L"\\combine-received.txt";
    DeleteFileW(got.c_str());
    check(SUCCEEDED(c->Invoke(three, nullptr)), "invoked");
    for (int i = 0; i < 100 && GetFileAttributesW(got.c_str()) == INVALID_FILE_ATTRIBUTES; ++i) Sleep(100);
    std::wstring text;
    if (FILE* fp = _wfopen(got.c_str(), L"rb")) {
        wchar_t buf[2048]; size_t n;
        while ((n = fread(buf, sizeof(wchar_t), 2048, fp)) > 0) text.append(buf, n);
        fclose(fp);
    }
    bool all = !text.empty();
    for (const auto& p : pdfs) if (text.find(p) == std::wstring::npos) all = false;
    check(all, "FeatherPDF.exe --combine-list got all three files");
    std::printf(fails ? "SOME FAILED\n" : "ALL PASSED\n");
    return fails ? 1 : 0;
}
