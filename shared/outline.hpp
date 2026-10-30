#pragma once
// Adds bookmarks (outline entries) to a PDF written by PDFium. PDFium itself cannot create them, so the entry is
// appended as an incremental update that only touches the outline objects.
#include <string>
#include <vector>

namespace outline {

// `pdf` is a complete file as written by PDFium (classic cross-reference table). Adds a bookmark `title` that goes to
// page `pageIndex` (0-based) at the end of the top level. Returns false when the structure is not supported.
bool addBookmark(const std::vector<unsigned char>& pdf, int pageIndex, const std::wstring& title, std::vector<unsigned char>& out);

} // namespace outline
