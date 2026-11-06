#pragma once
// Splitting a document (every N pages, at its bookmarks, or into files no larger than a size), merging documents
// with their bookmarks, and putting the pages of a two-sided scan in order (fronts and backs interleaved).
#include <string>
#include <vector>
#include "document.hpp"

namespace splitmerge {

struct Part {
    std::vector<int> pages;   // 0-based, in order
    std::wstring name;        // for the file name: the bookmark's title, or empty
};

std::vector<Part> everyN(int pageCount, int n);
// A new part at every bookmark of the given level (1 = top level). Pages before the first bookmark form a part of their own.
std::vector<Part> byBookmarks(Document& d, int level = 1);
// Parts that are each at most `maxBytes` when saved (a single page that is larger stays a part of its own).
std::vector<Part> bySize(Document& d, double maxBytes);

// The pages as a PDF of their own, with the bookmarks that point into them.
bool partBytes(Document& d, const std::vector<int>& pages, std::vector<unsigned char>& out);

// A title made safe for a file name (no \ / : * ? " < > |, at most 60 letters).
std::wstring fileSafe(const std::wstring& title);

struct Source {
    Document* doc = nullptr;
    std::wstring name;   // the bookmark for this document when merged with UnderFileNames
};
enum class Marks { Keep, UnderFileNames, None };
// All pages of the documents, in order, as one PDF. Keep: each document's bookmarks, moved to its pages.
// UnderFileNames: a bookmark per document (its name) with the document's own bookmarks inside.
bool merge(const std::vector<Source>& docs, Marks marks, std::vector<unsigned char>& out);

// The order of pages for a two-sided scan: the fronts (indices 0 .. fronts-1) and the backs (fronts .. fronts+backs-1)
// one after the other. `backsReversed`: the back pages were scanned last page first (the stack turned over).
std::vector<int> interleaveOrder(int fronts, int backs, bool backsReversed);
// `d` holds the fronts and, when `backs` is null, the backs as its second half; otherwise the backs are `backs`.
bool interleave(Document& d, Document* backs, bool backsReversed, std::vector<unsigned char>& out);

} // namespace splitmerge
