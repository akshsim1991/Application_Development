#pragma once
// The fonts equations are set in when the document was made with TeX: Latin Modern Math (GUST Font License) as three
// TrueType fonts, upright, mathematical italic and bold, each at the plain code points (see scripts/make-math-fonts.py).
#include <cstddef>

namespace mathfonts {
struct Font { const unsigned char* data; size_t size; };
extern const Font kFonts[3];   // 0 upright, 1 italic, 2 bold
} // namespace mathfonts
