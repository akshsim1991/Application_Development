#pragma once
// Repairing a damaged PDF, and Fast Web View (a linearized file: the first page can be shown while the rest is still
// downloading).
#include <string>
#include <vector>
#include "document.hpp"

namespace repair {

struct Report {
    bool opened = false;        // the file could be read at all
    bool fixed = false;         // something was wrong and the copy is put right
    int pages = 0;
    int badPages = 0;           // pages that could not be read (kept as they are)
    std::vector<std::wstring> problems;   // what was found, in words
};

// Reads the file at `path` (PDFium rebuilds a damaged index while opening it) and writes a clean copy to `out`.
Report run(const std::wstring& path, std::vector<unsigned char>& out, const std::string& password = {});

} // namespace repair

namespace webview {

// `pdf`: a complete file as written by PDFium (one classic cross-reference table, no encryption). Writes the same
// document linearized ("Fast Web View"). False when the structure is not supported.
bool linearize(const std::vector<unsigned char>& pdf, std::vector<unsigned char>& out, std::wstring* why = nullptr);
// The document as it is now, linearized.
bool linearize(Document& d, std::vector<unsigned char>& out, std::wstring* why = nullptr);
// The file says it is linearized (its first object has /Linearized).
bool isLinearized(const std::vector<unsigned char>& pdf);

} // namespace webview
