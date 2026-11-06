#pragma once
// Comparing the text of two documents word by word, across page breaks (a paragraph added on page 2 does not make every
// later page "different"): the changes (replaced, inserted, removed words) with their pages, a written report, and a
// marked copy of the first document (each change highlighted, with what it became in the comment).
#include <string>
#include <vector>
#include "document.hpp"

namespace textdiff {

struct Word {
    int page = 0, start = 0, count = 0;   // where it is in its document's text
    std::wstring text;
};

struct Change {
    enum Kind { Replaced, Inserted, Removed } kind = Replaced;
    int aFrom = 0, aTo = 0;   // words [aFrom, aTo) of the first document (empty for Inserted: the place before aFrom)
    int bFrom = 0, bTo = 0;   // words [bFrom, bTo) of the second (empty for Removed)
};

struct Options {
    bool ignoreCase = false;
    bool ignorePunctuation = false;   // "word," and "word" are the same
};

struct Result {
    std::vector<Word> a, b;
    std::vector<Change> changes;
    int same = 0;          // words unchanged
    bool rough = false;    // too many differences for the exact comparison: compared page by page instead
};

std::vector<Word> words(Document& d);
Result compare(Document& a, Document& b, const Options& o = {});
// The same, on plain word lists (for tests and for text that is not in a document).
Result compareWords(std::vector<Word> a, std::vector<Word> b, const Options& o = {});

// The words of a change as one string ("the quick fox").
std::wstring text(const std::vector<Word>& w, int from, int to);
// A report in Markdown (headings, a summary, each change with its pages), for textpdf::build.
std::wstring report(const Result& r, const std::wstring& nameA, const std::wstring& nameB);
// Marks the changes on `copy`, a copy of the first document: replaced and removed words highlighted (orange, red),
// insertions shown by a note where they belong (green); each with a comment saying what changed. Returns the marks added.
int mark(Document& copy, const Result& r);

} // namespace textdiff
