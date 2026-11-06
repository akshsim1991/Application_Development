#include "readaloud.hpp"
#include <algorithm>
#include <cwctype>

namespace readaloud {

Script build(Document& d, int fromPage, int fromChar, size_t maxChars) {
    Script s;
    for (int p = std::max(0, fromPage); p < d.pageCount() && s.text.size() < maxChars; ++p) {
        const int n = d.charCount(p);
        if (n <= 0) continue;
        const std::wstring t = d.textRange(p, 0, n);
        const int first = p == fromPage ? std::clamp(fromChar, 0, n) : 0;
        for (int i = first; i < static_cast<int>(t.size()); ++i) {
            wchar_t c = t[static_cast<size_t>(i)];
            if (c == L'\r') continue;
            if (c == L'\n') c = L' ';
            if (c == 0x02 || c == 0xFFFE) continue;   // a soft hyphen at a line end: the word goes on
            s.text += c;
            s.page.push_back(p);
            s.index.push_back(i);
        }
        s.text += L"\n";   // a pause between pages
        s.page.push_back(-1);
        s.index.push_back(-1);
    }
    return s;
}

bool span(const Script& s, size_t offset, size_t length, int& page, int& start, int& count) {
    page = -1;
    for (size_t i = offset; i < offset + length && i < s.text.size(); ++i) {
        if (s.page[i] < 0) continue;
        if (page < 0) { page = s.page[i]; start = s.index[i]; }
        if (s.page[i] != page) break;
        count = s.index[i] - start + 1;
    }
    return page >= 0 && count > 0;
}

size_t sentence(const Script& s, size_t offset, int dir) {
    auto isEnd = [&](size_t i) {
        const wchar_t c = s.text[i];
        return (c == L'.' || c == L'!' || c == L'?' || c == L'\n') && (i + 1 >= s.text.size() || std::iswspace(static_cast<wint_t>(s.text[i + 1])));
    };
    auto startOf = [&](size_t i) {   // the start of the sentence holding i
        while (i > 0 && !isEnd(i - 1)) --i;
        while (i < s.text.size() && std::iswspace(static_cast<wint_t>(s.text[i]))) ++i;
        return i;
    };
    offset = std::min(offset, s.text.empty() ? 0 : s.text.size() - 1);
    size_t here = startOf(offset);
    if (dir < 0) {
        if (here < 2) return 0;
        size_t back = here - 1;
        while (back > 0 && std::iswspace(static_cast<wint_t>(s.text[back]))) --back;
        return startOf(back > 0 && isEnd(back) ? back - 1 : back);
    }
    if (dir > 0) {
        size_t i = offset;
        while (i < s.text.size() && !isEnd(i)) ++i;
        ++i;
        while (i < s.text.size() && std::iswspace(static_cast<wint_t>(s.text[i]))) ++i;
        return std::min(i, s.text.size());
    }
    return here;
}

size_t wordLength(const Script& s, size_t offset) {
    size_t i = offset;
    while (i < s.text.size() && !std::iswspace(static_cast<wint_t>(s.text[i]))) ++i;
    return i - offset;
}

} // namespace readaloud
