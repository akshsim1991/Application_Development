#pragma once
#include <cwchar>
#include <string>

namespace featherscript {
// Superscripts and subscripts (m³, x², H₂O, eⁿ): kept in the text as the Unicode characters for them, which the editor
// shows and can be typed; set as the plain character, smaller, raised or lowered (so any font draws them).
namespace script {
const wchar_t kSupBase[] = L"0123456789+-=()niabcdefghjklmoprstuvwxyz";
const wchar_t kSup[] = L"⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿⁱ"
                       L"ᵃᵇᶜᵈᵉᶠᵍʰʲᵏˡᵐᵒᵖʳˢᵗᵘᵛʷˣʸᶻ";
const wchar_t kSubBase[] = L"0123456789+-=()aeoxhklmnpst";
const wchar_t kSub[] = L"₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎ₐₑₒₓₕₖₗₘₙₚₛₜ";
// 1 superscript, -1 subscript, 0 neither; `base` the plain character
inline int of(wchar_t c, wchar_t& base) {
    base = c;
    if (c < 0xB0) return 0;
    if (const wchar_t* p = std::wcschr(kSup, c)) { base = kSupBase[p - kSup]; return 1; }
    if (const wchar_t* p = std::wcschr(kSub, c)) { base = kSubBase[p - kSub]; return -1; }
    return 0;
}
inline wchar_t make(wchar_t c, int kind) {
    const wchar_t* from = kind > 0 ? kSupBase : kSubBase;
    const wchar_t* to = kind > 0 ? kSup : kSub;
    if (const wchar_t* p = std::wcschr(from, c)) return to[p - from];
    return c;
}
inline bool any(const std::wstring& t) { wchar_t b; for (wchar_t c : t) if (of(c, b)) return true; return false; }
inline std::wstring plain(std::wstring t) { for (wchar_t& c : t) { wchar_t b; of(c, b); c = b; } return t; }
} // namespace script
} // namespace featherscript
