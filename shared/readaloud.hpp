#pragma once
// Read aloud: the text to speak, with each spoken character's place on the page, so the word being spoken can be
// highlighted and the reading can move back or forward a sentence.
#include <string>
#include <vector>
#include "document.hpp"

namespace readaloud {

struct Script {
    std::wstring text;              // what the voice reads
    std::vector<int> page, index;   // for each character of `text`: its page and character index (-1: added between pages)
};

// From page `fromPage` (character `fromChar`) to the end, at most `maxChars` characters.
Script build(Document& d, int fromPage, int fromChar = 0, size_t maxChars = 200000);

// The page characters under text [offset, offset + length): page, first index, count. False when none.
bool span(const Script& s, size_t offset, size_t length, int& page, int& start, int& count);

// The start of the sentence that holds `offset` (dir 0), the one before it (dir -1) or after it (dir 1).
size_t sentence(const Script& s, size_t offset, int dir);

// The word starting at or after `offset` (for voices that report only where a word starts): its length.
size_t wordLength(const Script& s, size_t offset);

} // namespace readaloud
