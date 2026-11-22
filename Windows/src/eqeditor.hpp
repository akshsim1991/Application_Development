#pragma once
// The equation editor: the equation written in one line (s^8, x_1, \frac{a}{b}, \sqrt{x} ...), with buttons for powers,
// indices, fractions, roots, brackets, Greek letters and signs, and a live picture of the equation as it will be set.
#include <windows.h>
#include <string>

namespace eqeditor {

struct Result {
    std::wstring source;
    double size = 12;
    bool remove = false;   // "Remove equation" was chosen
};

// Shows the editor. `source` and `size` are what it starts with (an empty source: a new equation); `existing` offers
// "Remove equation". False when it was cancelled.
bool run(HWND parent, HINSTANCE inst, int dpi, HFONT font, const wchar_t* title, const std::wstring& source, double size, bool existing, Result& out);

} // namespace eqeditor
