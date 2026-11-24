#pragma once
// "Combine into one PDF" in File Explorer's first right-click menu on Windows 11: the sparse package FeatherPDF.msix
// (Windows/shellext/make-package.py), its command FeatherPDFShell.dll, and FeatherPDF-package.cer, the certificate it
// is signed with (made for each build, its key thrown away). Header only: the setup program and the app both use it.
//   - The setup program (as administrator) puts the certificate in the PC's Trusted People and takes out the older ones
//     of the same name (trust()); the uninstaller takes it out (untrust()).
//   - Packages are registered per Windows user: the app registers it for whoever uses it, once for each version
//     (registerForUser(), in the background), and takes it out when the entry is turned off in Settings.
// On Windows 10 nothing of this is done: its menu shows the classic entries (Windows/src/explorermenu.hpp), which stay
// on Windows 11 too, under "Show more options".
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>

namespace explorerpackage {

inline const wchar_t* const kPackage = L"FeatherPDF.msix";
inline const wchar_t* const kCertificate = L"FeatherPDF-package.cer";
inline const wchar_t* const kPackageName = L"FeatherPDF.ExplorerMenu";   // its Identity Name in the manifest
inline const wchar_t* const kSubject = L"Feather PDF";                   // CN of the certificate (the Publisher)

// Windows 11 (build 22000 or later): the only Windows whose first menu needs the package.
inline bool windows11() {
    using Fn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    auto get = nt ? reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(nt, "RtlGetVersion"))) : nullptr;
    OSVERSIONINFOW v{sizeof(v)};
    return get && get(&v) == 0 && v.dwMajorVersion >= 10 && v.dwBuildNumber >= 22000;
}

namespace detail {
inline HCERTSTORE trustedPeople() {
    return CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE | CERT_STORE_OPEN_EXISTING_FLAG, L"TrustedPeople");
}
// Takes the certificates named "Feather PDF" out of the store (all of them: each build has its own).
inline void removeOurs(HCERTSTORE store) {
    for (;;) {
        PCCERT_CONTEXT c = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR_W, kSubject, nullptr);
        if (!c) break;
        wchar_t name[256] = L"";
        CertGetNameStringW(c, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, 256);
        if (wcscmp(name, kSubject) != 0) { CertFreeCertificateContext(c); break; }   // (only exactly ours)
        CertDeleteCertificateFromStore(c);   // (frees c)
    }
}
// Runs PowerShell without a window and waits for it; true when it ended well.
inline bool powershell(const std::wstring& command, DWORD waitMs = 120000) {
    wchar_t sys[MAX_PATH];
    if (!GetSystemDirectoryW(sys, MAX_PATH)) return false;
    const std::wstring exe = std::wstring(sys) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring cmd = L"\"" + exe + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"" + command + L"\"";
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    CloseHandle(pi.hThread);
    DWORD code = 1;
    if (WaitForSingleObject(pi.hProcess, waitMs) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    return code == 0;
}
// 'text' for PowerShell, in single quotes
inline std::wstring quoted(const std::wstring& s) {
    std::wstring out = L"'";
    for (wchar_t c : s) { if (c == L'\'') out += L"''"; else out += c; }
    return out + L"'";
}
} // namespace detail

// The setup program, as administrator: this build's certificate trusted, the older ones of the same name taken out.
inline bool trust(const std::wstring& programFolder) {
    HANDLE f = CreateFileW((programFolder + L"\\" + kCertificate).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    std::vector<BYTE> der(GetFileSize(f, nullptr));
    DWORD got = 0;
    const BOOL read = !der.empty() && ReadFile(f, der.data(), static_cast<DWORD>(der.size()), &got, nullptr) && got == der.size();
    CloseHandle(f);
    if (!read) return false;
    HCERTSTORE store = detail::trustedPeople();
    if (!store) return false;
    detail::removeOurs(store);
    const BOOL ok = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING, der.data(), static_cast<DWORD>(der.size()), CERT_STORE_ADD_REPLACE_EXISTING, nullptr);
    CertCloseStore(store, 0);
    return ok != 0;
}

// The uninstaller: the certificates taken out, and the package for the user running it.
inline void untrust() {
    detail::powershell(L"Get-AppxPackage -Name " + std::wstring(kPackageName) + L" | Remove-AppxPackage", 60000);
    if (HCERTSTORE store = detail::trustedPeople()) { detail::removeOurs(store); CertCloseStore(store, 0); }
}

// The package registered for the current user, its files in `programFolder`. Slow (PowerShell): call it off the window's thread.
inline bool registerForUser(const std::wstring& programFolder) {
    return detail::powershell(L"Add-AppxPackage -Path " + detail::quoted(programFolder + L"\\" + kPackage) + L" -ExternalLocation " +
                              detail::quoted(programFolder) + L" -ForceUpdateFromAnyVersion");
}
inline bool unregisterForUser() {
    return detail::powershell(L"Get-AppxPackage -Name " + std::wstring(kPackageName) + L" | Remove-AppxPackage");
}

} // namespace explorerpackage
