#pragma once
// Turns a plain text or Markdown file into a PDF (A4, standard fonts).
#include <string>
#include <vector>

namespace textpdf {

struct Result {
    bool ok = false;
    int pages = 0;
    int replaced = 0;   // characters the standard fonts cannot show, written as "?"
};

Result build(const std::wstring& text, bool markdown, std::vector<unsigned char>& out);

} // namespace textpdf
