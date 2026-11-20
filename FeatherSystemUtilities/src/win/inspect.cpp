// File evidence: version resource + offline Authenticode (embedded or catalog). Read-only.
#include "app.h"
#include "../core/text.h"
#include <tlhelp32.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <mscat.h>
#include <mutex>
#include <shlobj.h>
#include <fstream>
#include <filesystem>

namespace fsu {
bool InSafeMode(int* kind) { int m = GetSystemMetrics(SM_CLEANBOOT); if (kind) *kind = m; return m != 0; }

UINT DpiForWindow(HWND h) {
    static auto fn = (UINT(WINAPI*)(HWND))GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (fn && h) return fn(h);
    HDC dc = GetDC(h); int d = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96; if (dc) ReleaseDC(h, dc); return d > 0 ? (UINT)d : 96;
}
DWORD WindowsBuild() {
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*); static DWORD b = [] { auto f = (RtlGetVersionFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"); OSVERSIONINFOW v{}; v.dwOSVersionInfoSize = sizeof v; if (f && f(&v) == 0) return (DWORD)v.dwBuildNumber; return (DWORD)0; }(); return b;
}
bool IsProcessRunning(const wchar_t* exe) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (s == INVALID_HANDLE_VALUE) return false; PROCESSENTRY32W pe{}; pe.dwSize = sizeof pe; bool found = false;
    if (Process32FirstW(s, &pe)) do { if (_wcsicmp(pe.szExeFile, exe) == 0) { found = true; break; } } while (Process32NextW(s, &pe));
    CloseHandle(s); return found;
}

std::wstring ExpandEnv(const std::wstring& s) {
    DWORD n = ExpandEnvironmentStringsW(s.c_str(), nullptr, 0);
    if (!n) return s;
    std::wstring o(n, L'\0');
    ExpandEnvironmentStringsW(s.c_str(), &o[0], n);
    o.resize(n - 1);
    return o;
}
bool FileExistsExpanded(const std::wstring& p) {
    if (p.empty()) return false;
    if (p.size() > 1 && p[0] == L'\\' && p[1] == L'\\') return true;   // strict local mode: never touch network paths
    return GetFileAttributesW(ExpandEnv(p).c_str()) != INVALID_FILE_ATTRIBUTES;
}
std::wstring Win32Err(DWORD e) {
    wchar_t* buf = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e, 0, (LPWSTR)&buf, 0, nullptr);
    std::wstring s = buf ? buf : L"";
    if (buf) LocalFree(buf);
    while (!s.empty() && (s.back() == L'\r' || s.back() == L'\n' || s.back() == L' ')) s.pop_back();
    return s + L" (error " + std::to_wstring(e) + L")";
}
std::wstring NowIsoUtc() {
    SYSTEMTIME t; GetSystemTime(&t);
    wchar_t b[40]; swprintf(b, 40, L"%04d-%02d-%02dT%02d:%02d:%02dZ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}
bool IsElevated() {
    HANDLE t; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return false;
    TOKEN_ELEVATION el{}; DWORD n = 0;
    bool r = GetTokenInformation(t, TokenElevation, &el, sizeof el, &n) && el.TokenIsElevated;
    CloseHandle(t); return r;
}
std::wstring DataDir() {
    PWSTR p = nullptr; std::wstring d;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &p))) { d = std::wstring(p) + L"\\FeatherSystemUtilities"; CoTaskMemFree(p); }
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

std::wstring SubDir(const wchar_t* name) { std::wstring d = DataDir() + L"\\" + name; CreateDirectoryW(d.c_str(), nullptr); return d; }

static std::wstring SignerFromState(HANDLE state) {
    CRYPT_PROVIDER_DATA* pd = WTHelperProvDataFromStateData(state);
    if (!pd) return L"";
    CRYPT_PROVIDER_SGNR* sg = WTHelperGetProvSignerFromChain(pd, 0, FALSE, 0);
    if (!sg || !sg->pChainContext || sg->pChainContext->cChain == 0) return L"";
    auto* ch = sg->pChainContext->rgpChain[0];
    if (!ch || ch->cElement == 0) return L"";
    PCCERT_CONTEXT cert = ch->rgpElement[0]->pCertContext;
    wchar_t name[256] = L"";
    if (cert) CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, 256);
    return name;
}

static void VerifyFile(const std::wstring& path, FileInfo& fi) {
    WINTRUST_FILE_INFO file{}; file.cbStruct = sizeof file; file.pcwszFilePath = path.c_str();
    WINTRUST_DATA wd{}; wd.cbStruct = sizeof wd; wd.dwUIChoice = WTD_UI_NONE; wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE; wd.pFile = &file; wd.dwStateAction = WTD_STATEACTION_VERIFY;
    wd.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_REVOCATION_CHECK_NONE;   // offline: no network
    GUID act = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG r = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &act, &wd);
    if (r == 0) { fi.sig = Sig::Valid; fi.signer = SignerFromState(wd.hWVTStateData); }
    wd.dwStateAction = WTD_STATEACTION_CLOSE; WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &act, &wd);
    if (r == 0) return;
    if ((DWORD)r != (DWORD)TRUST_E_NOSIGNATURE && (DWORD)r != (DWORD)TRUST_E_PROVIDER_UNKNOWN && (DWORD)r != (DWORD)TRUST_E_SUBJECT_FORM_UNKNOWN) {
        fi.sig = Sig::Invalid; return;   // signed but untrusted/tampered/expired chain offline
    }
    // Catalog-signed (most Windows files)
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { fi.sig = Sig::Unknown; return; }
    HCATADMIN ca = nullptr; bool done = false;
    if (CryptCATAdminAcquireContext(&ca, nullptr, 0)) {
        BYTE hash[64]; DWORD hl = sizeof hash;
        if (CryptCATAdminCalcHashFromFileHandle(h, &hl, hash, 0)) {
            HCATINFO ci = CryptCATAdminEnumCatalogFromHash(ca, hash, hl, 0, nullptr);
            if (ci) {
                CATALOG_INFO cinfo{}; cinfo.cbStruct = sizeof cinfo;
                if (CryptCATCatalogInfoFromContext(ci, &cinfo, 0)) {
                    std::wstring tag; for (DWORD i = 0; i < hl; ++i) { wchar_t b[3]; swprintf(b, 3, L"%02X", hash[i]); tag += b; }
                    WINTRUST_CATALOG_INFO cat{}; cat.cbStruct = sizeof cat; cat.pcwszCatalogFilePath = cinfo.wszCatalogFile;
                    cat.pcwszMemberFilePath = path.c_str(); cat.hMemberFile = h; cat.pbCalculatedFileHash = hash; cat.cbCalculatedFileHash = hl;
                    cat.pcwszMemberTag = tag.c_str();
                    WINTRUST_DATA w2{}; w2.cbStruct = sizeof w2; w2.dwUIChoice = WTD_UI_NONE; w2.fdwRevocationChecks = WTD_REVOKE_NONE;
                    w2.dwUnionChoice = WTD_CHOICE_CATALOG; w2.pCatalog = &cat; w2.dwStateAction = WTD_STATEACTION_VERIFY;
                    w2.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL | WTD_REVOCATION_CHECK_NONE;
                    GUID a2 = WINTRUST_ACTION_GENERIC_VERIFY_V2;
                    LONG r2 = WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &a2, &w2);
                    if (r2 == 0) { fi.sig = Sig::Valid; fi.catalogSigned = true; fi.signer = SignerFromState(w2.hWVTStateData); }
                    else fi.sig = Sig::Invalid;
                    w2.dwStateAction = WTD_STATEACTION_CLOSE; WinVerifyTrust((HWND)INVALID_HANDLE_VALUE, &a2, &w2);
                    done = true;
                }
                CryptCATAdminReleaseCatalogContext(ca, ci, 0);
            }
        }
        CryptCATAdminReleaseContext(ca, 0);
    }
    CloseHandle(h);
    if (!done) fi.sig = Sig::Unsigned;
}

static void ReadVersion(const std::wstring& path, FileInfo& fi) {
    DWORD dummy; DWORD sz = GetFileVersionInfoSizeW(path.c_str(), &dummy);
    if (!sz) return;
    std::vector<BYTE> buf(sz);
    if (!GetFileVersionInfoW(path.c_str(), 0, sz, buf.data())) return;
    struct LT { WORD l, c; } *tr = nullptr; UINT n = 0;
    std::wstring sub = L"\\StringFileInfo\\040904B0\\";
    if (VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation", (LPVOID*)&tr, &n) && n >= sizeof(LT)) {
        wchar_t b[64]; swprintf(b, 64, L"\\StringFileInfo\\%04x%04x\\", tr->l, tr->c); sub = b;
    }
    auto get = [&](const wchar_t* k) { wchar_t* v = nullptr; UINT l = 0;
        if (VerQueryValueW(buf.data(), (sub + k).c_str(), (LPVOID*)&v, &l) && v) return std::wstring(v); return std::wstring(); };
    fi.publisher = get(L"CompanyName"); fi.version = get(L"FileVersion");
}

FileInfo InspectFile(const std::wstring& path, bool checkSig) {
    static std::mutex mu; static std::map<std::wstring, FileInfo> cache;   // identity includes size+mtime
    FileInfo fi;
    if (path.empty() || (path.size() > 1 && path[0] == L'\\' && path[1] == L'\\')) return fi;
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a) || (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return fi;
    std::wstring key = Lower(path) + L"|" + std::to_wstring(a.nFileSizeLow) + L"|" + std::to_wstring(a.ftLastWriteTime.dwLowDateTime) + (checkSig ? L"|s" : L"");
    { std::lock_guard<std::mutex> g(mu); auto it = cache.find(key); if (it != cache.end()) return it->second; }
    fi.exists = true;
    ReadVersion(path, fi);
    if (checkSig) VerifyFile(path, fi);
    std::lock_guard<std::mutex> g(mu); cache[key] = fi;
    return fi;
}

} // namespace fsu
