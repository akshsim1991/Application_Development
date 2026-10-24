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

} // namespace redact
