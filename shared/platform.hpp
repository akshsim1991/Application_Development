#pragma once
// What the shared PDF engine needs from the system. On Windows that is <windows.h>; on macOS (and Linux, where the
// engine is checked) the few Windows types the engine uses are defined here, with the same meaning, so the engine code
// is the same on both.
#ifdef _WIN32
#include <windows.h>
#else
#include <cstdint>

typedef uint32_t COLORREF;   // 0x00BBGGRR, as on Windows
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned int UINT;

inline constexpr COLORREF RGB(int r, int g, int b) {
    return static_cast<COLORREF>((r & 0xFF) | ((g & 0xFF) << 8) | ((b & 0xFF) << 16));
}
inline constexpr BYTE GetRValue(COLORREF c) { return static_cast<BYTE>(c & 0xFF); }
inline constexpr BYTE GetGValue(COLORREF c) { return static_cast<BYTE>((c >> 8) & 0xFF); }
inline constexpr BYTE GetBValue(COLORREF c) { return static_cast<BYTE>((c >> 16) & 0xFF); }
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace platform {

// UTF-8 <-> wide text (UTF-16 on Windows, UTF-32 elsewhere).
std::string utf8(const std::wstring& s);
std::wstring wide(const std::string& utf8);

// The whole file; false when it cannot be read.
bool readFile(const std::wstring& path, std::vector<unsigned char>& out);
// Writes the whole file (replacing it); false when it cannot be written.
bool writeFile(const std::wstring& path, const std::vector<unsigned char>& data);
// A text file as text: UTF-16 with a byte order mark, UTF-8, or else the system's old 8-bit code page (Latin-1 off
// Windows). Empty when it cannot be read.
std::wstring readTextFile(const std::wstring& path);
// True when a file (not a folder) of that name exists.
bool fileExists(const std::wstring& path);
// The folder for the app's own files (settings, search index), made when missing: %APPDATA%\FeatherPDF on Windows,
// ~/Library/Application Support/Feather PDF on the Mac. Empty when there is none.
std::wstring appDataDir();
// Makes a folder (its parent must exist); true when it exists afterwards.
bool makeDirectory(const std::wstring& path);
// The separator of folders in a path: \ on Windows, / elsewhere.
wchar_t pathSeparator();

} // namespace platform
