#pragma once
#include <vector>
#include "document.hpp"

namespace redact {

// Permanently removes everything under the marked areas. Every page that has marks is redrawn as a picture
// with the marked areas blacked out; its text, drawings, pictures and annotations are removed from the file.
// Returns the number of pages changed (0 on failure). One undo step.
int apply(Document& d, int dpi = 200);

// Marks covering every occurrence of `query`. Returns how many were added.
int markText(Document& d, const std::wstring& query, bool matchCase);

// The text under the pending marks (call this before apply()), cleaned up, without duplicates.
std::vector<std::wstring> collect(Document& d);

struct Finding { std::wstring phrase, where; };
struct Report {
    int phrases = 0;                 // phrases that were looked for
    int places = 0;                  // text layers, annotations, bookmarks, ... that were examined
    std::vector<Finding> found;      // phrases that are still in the file
    int textPagesLeft = 0;           // pages that still have selectable text (informational)
};
// Looks for the phrases everywhere the file could still hold them: the text of every page, comments and other annotations,
// bookmarks, the document information (title, author, subject, keywords) and the raw file data (plain and UTF-16 forms).
Report verify(Document& d, const std::vector<std::wstring>& phrases);

} // namespace redact
