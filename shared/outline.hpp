#pragma once
// Adds bookmarks (outline entries) to a PDF written by PDFium. PDFium itself cannot create them, so the entry is
// appended as an incremental update that only touches the outline objects.
#include <string>
#include <vector>
#include "document.hpp"

namespace outline {

// `pdf` is a complete file as written by PDFium (classic cross-reference table). Adds a bookmark `title` that goes to
// page `pageIndex` (0-based) at the end of the top level. Returns false when the structure is not supported.
bool addBookmark(const std::vector<unsigned char>& pdf, int pageIndex, const std::wstring& title, std::vector<unsigned char>& out);

// Replaces the whole outline of `pdf` (a file written by PDFium) with `tree`: titles, target pages (0-based; -1 for none)
// and nested entries (closed). An empty tree removes the bookmarks. Returns false when the structure is not supported.
bool write(const std::vector<unsigned char>& pdf, const std::vector<Bookmark>& tree, std::vector<unsigned char>& out);

} // namespace outline
