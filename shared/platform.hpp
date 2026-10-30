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

namespace platform {

// UTF-8 <-> wide text (UTF-16 on Windows, UTF-32 elsewhere).
std::string utf8(const std::wstring& s);
std::wstring wide(const std::string& utf8);

} // namespace platform
