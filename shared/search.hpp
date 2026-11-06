#pragma once
// Searching a document with options: match case, whole words only, wildcards (* any letters, ? one letter, \* a real
// star) and the text of comments (notes, highlights' comments, text boxes, callouts). Hits in the page text are
// character ranges; hits in a comment carry the comment's index and rectangle (TextHit::annot, ::rect).
#include <string>
#include <utility>
#include <vector>
#include "document.hpp"

namespace search {

struct Options {
    bool matchCase = false;
    bool wholeWord = false;
    bool wildcards = false;
    bool comments = false;   // also look in the comments' text
};

std::vector<TextHit> find(Document& d, const std::wstring& query, const Options& o);

// Matches of `query` in `text`: (start, length) pairs, not overlapping, in order. Spaces in the query match any run
// of spaces and line breaks; * stays within a line (at the end of the query it runs to the end of the word).
std::vector<std::pair<int, int>> matchText(const std::wstring& text, const std::wstring& query, const Options& o);

// The comment's text for a hit in a comment (for showing where the match is); empty for a page-text hit.
std::wstring commentText(Document& d, const TextHit& h);

} // namespace search
