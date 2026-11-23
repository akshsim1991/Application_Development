#pragma once
// Equations, as an equation editor makes them. An equation is written as one line in a small notation (the way LaTeX
// writes mathematics) and set on the page with its powers raised and smaller, its fractions over a bar and its roots under
// a radical sign:
//     s^8 + 9s^6 + 24s^5 = 0       x_1   x_{i+1}   e^{-st}   (s^10 and s^-1 take the whole number)
//     G(s) = \frac{2K}{s(s + 2)(s + 4)}       \sqrt{x}   \sqrt[3]{x}
//     \alpha ... \omega  \Delta ... \Omega   \pm \times \cdot \div \le \ge \ne \approx \infty \to \sum \prod \int \partial
//     \text{for all t}    sin cos tan log ln exp lim max min are upright on their own
// Unicode letters and signs can be typed as they are (α, ≤, ², ₁ ...). Spaces only separate: the signs get their space.
#include "document.hpp"
#include <string>
#include <utility>
#include <vector>

namespace equation {

struct Glyph {
    std::wstring text;
    int font = 0;                 // 0 Times-Roman, 1 Times-Italic, 2 Symbol, 3 Times-Bold; 4 to 7 the same in Latin Modern
    double x = 0, y = 0;          // where it starts, on its baseline
    double size = 12;
    double stretch = 1;           // drawn this much taller (a bracket around a fraction)
    double advance = 0;           // its width as set
    bool slant = false;           // drawn leaning (Greek letters in an equation are italic; the Symbol font's are upright)
};
struct Stroke {                   // a line drawn through the points (a fraction's bar, a radical sign)
    std::vector<std::pair<double, double>> points;
    double width = 0.5;
};
struct Layout {
    std::vector<Glyph> glyphs;
    std::vector<Stroke> strokes;
    double width = 0, ascent = 0, descent = 0;   // from its start on the baseline
    bool tex = false;                            // set in Latin Modern (fonts 4 to 7), not Times
};

// Sets `source` at `size` points, starting at (0, 0) on the baseline. False when the notation is wrong; `error` says what.
// With `tex`, in Latin Modern (TeX's look: for documents made with LaTeX); else in Times and Symbol.
bool layout(const std::wstring& source, double size, Layout& out, std::wstring* error = nullptr, bool tex = false);

// An equation found on a page.
struct Found {
    bool found = false;
    bool ours = false;                // made by this editor (its notation is kept with it)
    int page = 0;
    std::vector<int> objects;         // its text objects and lines (fraction bars, radical signs)
    std::wstring source;              // in the notation
    double left = 0, right = 0, bottom = 0, top = 0;
    double baseline = 0;              // of its main line
    double size = 12;                 // of its main line
    COLORREF color = 0;
    bool centred = false;             // in the middle of the page: stays centred
    bool tex = false;                 // set in TeX's fonts: set again in Latin Modern
    bool wrapped = false;             // it runs onto a second line (a formula wrapped in a table's cell): that line
    double wrapX = 0, wrapBaseline = 0;   // starts here, and a longer formula is broken there again
    double startX = 0;                // where its first line starts
    bool uncertain = false;           // its text runs over two lines (a formula wrapped in a table's cell): it may be read
                                      // wrongly, and the editor says so
};
// The equation under a page point: text in a mathematics font (Cambria Math, STIX Math, LaTeX's) and what goes with it
// (powers, the other side of a fraction, its bar), or one made by this editor. With `anyText`, any line of text there is
// read as an equation (the editor asked for on ordinary text).
Found at(Document& d, int page, double x, double y, bool anyText = false);
// Sets `source` in place of the equation (one undo step). An empty `source` removes it.
bool replace(Document& d, const Found& f, const std::wstring& source, double size, std::wstring* why);
// Puts a new equation on the page, starting at `x` on the baseline `baseline` (in Latin Modern on a page set by TeX).
bool insert(Document& d, int page, double x, double baseline, const std::wstring& source, double size, COLORREF color, std::wstring* why);
// Whether the page's text is mostly in TeX's fonts: a new equation there is set in Latin Modern.
bool pageIsTex(Document& d, int page);
// A picture of the equation for a preview: BGRA rows, `scale` pixels per point, on white. False when it does not parse.
bool preview(const std::wstring& source, double size, double scale, std::vector<unsigned char>& bgra, int& w, int& h, std::wstring* error,
             bool tex = false);

// Superscript (kind 1) or subscript (-1) characters for `text` where they exist (², ₁, ⁿ ...): what an editor's x² and x₂
// buttons make of the selected text. Characters that have none stay as they are; `all` is false then.
std::wstring toScript(const std::wstring& text, int kind, bool* all = nullptr);
// The plain characters back (x² and x₂ pressed again).
std::wstring fromScript(const std::wstring& text);
// 1 when every character of `text` is a superscript, -1 a subscript, else 0.
int scriptKind(const std::wstring& text);

} // namespace equation
