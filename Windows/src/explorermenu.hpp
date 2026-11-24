#pragma once
// File Explorer's right-click menu entries: "Convert to PDF" and "Convert to PDF and combine all the selected file(s)".
// Written by the installer, and by the app (Settings > File Explorer, through an elevated copy of itself: they are
// machine-wide, in HKLM). Which entries are wanted is kept in HKLM\SOFTWARE\FeatherPDF\ContextMenu, so an update keeps
// the choice. Header only: the setup program and the app both include it.
#include <windows.h>
#include <shlobj.h>
#include <string>

namespace explorermenu {

// The files Feather PDF can make into PDFs (shared/topdf.cpp: extensions(), Windows part; and Windows/src/officepdf.cpp,
// made by the office program on the PC). Under SystemFileAssociations, so the entries are offered whichever program
// opens those files. Combining also takes PDFs.
inline const wchar_t* const kTypes[] = {L".png", L".jpg", L".jpeg", L".bmp", L".gif", L".tif", L".tiff", L".txt", L".md", L".markdown",
                                        L".ppt", L".pptx", L".pps", L".ppsx", L".odp", L".doc", L".docx", L".rtf", L".odt", L".xls", L".xlsx", L".ods"};
inline const wchar_t* const kConvertVerb = L"\\shell\\FeatherPDF.ConvertToPDF";
inline const wchar_t* const kCombineVerb = L"\\shell\\FeatherPDF.ConvertCombine";
inline const wchar_t* const kClasses = L"SOFTWARE\\Classes\\SystemFileAssociations\\";
inline const wchar_t* const kPrefs = L"SOFTWARE\\FeatherPDF\\ContextMenu";
inline const wchar_t* const kConvertLabel = L"Convert to PDF";
inline const wchar_t* const kCombineLabel = L"Convert to PDF and combine all the selected file(s)";
inline const wchar_t* const kCombinePdfLabel = L"Combine into one PDF (Feather PDF)";   // the same, on PDFs

namespace detail {
inline bool set(const std::wstring& key, const wchar_t* name, const std::wstring& value) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    const LONG r = RegSetValueExW(k, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
inline bool setDword(const std::wstring& key, const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    const LONG r = RegSetValueExW(k, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
inline void removeTree(const std::wstring& key) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) {
        RegDeleteTreeW(k, nullptr);
        RegCloseKey(k);
    }
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), KEY_WOW64_64KEY, 0);
}
inline bool exists(const std::wstring& key) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return false;
    RegCloseKey(k);
    return true;
}
inline bool verbOn(const wchar_t* verb, const wchar_t* type) { return exists(std::wstring(kClasses) + type + verb + L"\\command"); }
} // namespace detail

// Whether each entry is in the menu now.
inline bool convertOn() { return detail::verbOn(kConvertVerb, L".png"); }
inline bool combineOn() { return detail::verbOn(kCombineVerb, L".png"); }

// The choice kept for updates (both on when nothing was chosen yet).
inline void wanted(bool& convert, bool& combine) {
    convert = combine = true;
    HKEY k;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kPrefs, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return;
    DWORD v = 1, sz = sizeof(v);
    if (RegQueryValueExW(k, L"Convert", nullptr, nullptr, reinterpret_cast<BYTE*>(&v), &sz) == ERROR_SUCCESS) convert = v != 0;
    v = 1; sz = sizeof(v);
    if (RegQueryValueExW(k, L"Combine", nullptr, nullptr, reinterpret_cast<BYTE*>(&v), &sz) == ERROR_SUCCESS) combine = v != 0;
    RegCloseKey(k);
}

// Puts the entries in (or takes them out of) the menu for `exe`, and keeps the choice. Needs the right to change HKLM.
inline bool apply(const std::wstring& exe, bool convert, bool combine) {
    bool ok = detail::setDword(kPrefs, L"Convert", convert ? 1 : 0) && detail::setDword(kPrefs, L"Combine", combine ? 1 : 0);
    const std::wstring icon = exe + L",0";
    for (const wchar_t* t : kTypes) {
        const std::wstring v = std::wstring(kClasses) + t + kConvertVerb;
        if (convert) {
            ok = detail::set(v, nullptr, kConvertLabel) && ok;
            detail::set(v, L"Icon", icon);
            ok = detail::set(v + L"\\command", nullptr, L"\"" + exe + L"\" --convert-open \"%1\"") && ok;
        } else detail::removeTree(v);
    }
    auto combineOn = [&](const std::wstring& type) {
        const std::wstring v = std::wstring(kClasses) + type + kCombineVerb;
        if (combine) {
            ok = detail::set(v, nullptr, type == L".pdf" ? kCombinePdfLabel : kCombineLabel) && ok;
            detail::set(v, L"Icon", icon);
            // Player: offered for any number of selected files (Explorer hides other entries above 15). Each file starts
            // the program; the first one gathers the others' (Windows/src/main.cpp, convertCombine).
            detail::set(v, L"MultiSelectModel", L"Player");
            ok = detail::set(v + L"\\command", nullptr, L"\"" + exe + L"\" --convert-combine \"%1\"") && ok;
        } else detail::removeTree(v);
    };
    for (const wchar_t* t : kTypes) combineOn(t);
    combineOn(L".pdf");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

// Takes every entry out (uninstalling); the choice goes with SOFTWARE\FeatherPDF.
inline void removeAll() {
    for (const wchar_t* t : kTypes) {
        detail::removeTree(std::wstring(kClasses) + t + kConvertVerb);
        detail::removeTree(std::wstring(kClasses) + t + kCombineVerb);
    }
    detail::removeTree(std::wstring(kClasses) + L".pdf" + kCombineVerb);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

} // namespace explorermenu
