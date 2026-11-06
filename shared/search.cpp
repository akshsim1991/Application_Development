#include "search.hpp"
#include "annot.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <cwctype>

namespace search {

namespace {

struct Tok {
    enum Kind { Lit, One, Run, Space } kind;
    wchar_t c;
};

bool space(wchar_t c) { return std::iswspace(static_cast<wint_t>(c)) != 0; }
bool wordChar(wchar_t c) { return c == L'_' || std::iswalnum(static_cast<wint_t>(c)) != 0; }
bool lineBreak(wchar_t c) { return c == L'\n' || c == L'\r'; }

// Letters PDF text often holds in another form than the one typed: curly quotes, dashes, the "fi" ligature's parts.
wchar_t plain(wchar_t c) {
    switch (c) {
        case L'‘': case L'’': case L'‛': return L'\'';
        case L'“': case L'”': return L'"';
        case L'‐': case L'‑': case L'‒': case L'–': case L'—': case L'−': return L'-';
        case L' ': return L' ';
        default: return c;
    }
}

struct Matcher {
    std::vector<Tok> p;
    bool matchCase, wholeWord;
    const std::wstring* t = nullptr;

    wchar_t fold(wchar_t c) const { c = plain(c); return matchCase ? c : static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c))); }

    // The end of the shortest match of tokens [pi..] starting at ti, or -1.
    int match(size_t ti, size_t pi, int depth = 0) const {
        const std::wstring& s = *t;
        const size_t n = s.size();
        if (depth > 400) return -1;
        if (pi == p.size()) return (!wholeWord || ti >= n || !wordChar(s[ti])) ? static_cast<int>(ti) : -1;
        const Tok& k = p[pi];
        switch (k.kind) {
            case Tok::Lit:
                return ti < n && fold(s[ti]) == k.c ? match(ti + 1, pi + 1, depth + 1) : -1;
            case Tok::One:
                return ti < n && !space(s[ti]) ? match(ti + 1, pi + 1, depth + 1) : -1;
            case Tok::Space: {
                if (ti >= n || !space(plain(s[ti]))) return -1;
                size_t j = ti;
                while (j < n && space(plain(s[j]))) ++j;
                return match(j, pi + 1, depth + 1);
            }
            case Tok::Run: {
                if (pi + 1 == p.size()) {   // a star at the end: the rest of the word
                    size_t j = ti;
                    while (j < n && !space(s[j]) && (wordChar(s[j]) || !wholeWord)) ++j;
                    return match(j, pi + 1, depth + 1);
                }
                for (size_t j = ti; j <= n && j - ti <= 200; ++j) {
                    const int r = match(j, pi + 1, depth + 1);
                    if (r >= 0) return r;
                    if (j >= n || lineBreak(s[j])) break;
                }
                return -1;
            }
        }
        return -1;
    }
};

Matcher compile(const std::wstring& query, const Options& o) {
    Matcher m;
    m.matchCase = o.matchCase;
    m.wholeWord = o.wholeWord;
    for (size_t i = 0; i < query.size(); ++i) {
        const wchar_t c = query[i];
        if (space(plain(c))) {
            if (!m.p.empty() && m.p.back().kind != Tok::Space) m.p.push_back({Tok::Space, 0});
            continue;
        }
        if (o.wildcards && c == L'\\' && i + 1 < query.size()) { m.p.push_back({Tok::Lit, m.fold(query[++i])}); continue; }
        if (o.wildcards && c == L'*') { if (m.p.empty() || m.p.back().kind != Tok::Run) m.p.push_back({Tok::Run, 0}); continue; }
        if (o.wildcards && c == L'?') { m.p.push_back({Tok::One, 0}); continue; }
        m.p.push_back({Tok::Lit, m.fold(c)});
    }
    while (!m.p.empty() && m.p.back().kind == Tok::Space) m.p.pop_back();
    return m;
}

} // namespace

std::vector<std::pair<int, int>> matchText(const std::wstring& text, const std::wstring& query, const Options& o) {
    std::vector<std::pair<int, int>> out;
    Matcher m = compile(query, o);
    // nothing to anchor on (only stars and question marks would match everything): no matches
    bool anchor = false;
    for (const Tok& k : m.p) anchor = anchor || k.kind == Tok::Lit;
    if (!anchor) return out;
    m.t = &text;
    for (size_t s = 0; s < text.size();) {
        if (o.wholeWord && s > 0 && wordChar(text[s - 1]) && wordChar(text[s])) { ++s; continue; }
        if (m.p.front().kind == Tok::Lit && m.fold(text[s]) != m.p.front().c) { ++s; continue; }
        const int e = m.match(s, 0);
        if (e > static_cast<int>(s)) {
            out.push_back({static_cast<int>(s), e - static_cast<int>(s)});
            s = static_cast<size_t>(e);
        } else ++s;
    }
    return out;
}

std::vector<TextHit> find(Document& d, const std::wstring& query, const Options& o) {
    std::vector<TextHit> hits;
    if (query.empty()) return hits;
    const bool own = o.wholeWord || o.wildcards;   // otherwise PDFium's search (it also joins words broken over lines)
    const std::vector<TextHit> plainHits = own ? std::vector<TextHit>{} : d.find(query, o.matchCase);
    size_t next = 0;
    for (int p = 0; p < d.pageCount(); ++p) {
        if (own) {
            const int n = d.charCount(p);
            if (n > 0)
                for (const auto& r : matchText(d.textRange(p, 0, n), query, o)) {
                    TextHit h;
                    h.page = p;
                    h.start = r.first;
                    h.count = r.second;
                    hits.push_back(h);
                }
        } else {
            while (next < plainHits.size() && plainHits[next].page == p) hits.push_back(plainHits[next++]);
        }
        if (!o.comments) continue;
        for (const annot::Info& a : annot::list(d, p)) {
            if (a.contents.empty() || a.subtype == FPDF_ANNOT_LINK || a.subtype == FPDF_ANNOT_POPUP || a.subtype == FPDF_ANNOT_WIDGET) continue;
            for (const auto& r : matchText(a.contents, query, o)) {
                TextHit h;
                h.page = p;
                h.start = r.first;
                h.count = r.second;
                h.annot = a.index;
                h.left = a.rect.left;
                h.bottom = a.rect.bottom;
                h.right = a.rect.right;
                h.top = a.rect.top;
                hits.push_back(h);
            }
        }
    }
    return hits;
}

std::wstring commentText(Document& d, const TextHit& h) {
    if (h.annot < 0) return {};
    for (const annot::Info& a : annot::list(d, h.page))
        if (a.index == h.annot) return a.contents;
    return {};
}

} // namespace search
