#include "textdiff.hpp"
#include "annot.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <cwctype>
#include <unordered_map>

namespace textdiff {

namespace {

std::wstring key(const std::wstring& w, const Options& o) {
    std::wstring k;
    for (wchar_t c : w) {
        switch (c) {   // the same letter typed or typeset differently
            case L'‘': case L'’': c = L'\''; break;
            case L'“': case L'”': c = L'"'; break;
            case L'–': case L'—': case L'−': c = L'-'; break;
            case L'ﬁ': k += L"f"; c = L'i'; break;
            case L'ﬂ': k += L"f"; c = L'l'; break;
            default: break;
        }
        if (o.ignorePunctuation && std::iswpunct(static_cast<wint_t>(c))) continue;
        k += o.ignoreCase ? static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c))) : c;
    }
    return k;
}

// Myers' difference: the pairs (i, j) of equal words, in order, or false when more than `maxD` edits are needed.
bool myers(const std::vector<int>& A, const std::vector<int>& B, int maxD, std::vector<std::pair<int, int>>& matches) {
    const int N = static_cast<int>(A.size()), M = static_cast<int>(B.size());
    const int MAX = N + M;
    std::vector<int> V(static_cast<size_t>(2 * MAX + 3), 0);
    const int off = MAX + 1;
    std::vector<std::vector<int>> trace;   // V before each step, entries -d-1 .. d+1
    int D = -1;
    for (int d = 0; d <= std::min(MAX, maxD); ++d) {
        trace.emplace_back(V.begin() + (off - d - 1), V.begin() + (off + d + 2));
        for (int k = -d; k <= d; k += 2) {
            int x = (k == -d || (k != d && V[static_cast<size_t>(off + k - 1)] < V[static_cast<size_t>(off + k + 1)]))
                        ? V[static_cast<size_t>(off + k + 1)] : V[static_cast<size_t>(off + k - 1)] + 1;
            int y = x - k;
            while (x < N && y < M && A[static_cast<size_t>(x)] == B[static_cast<size_t>(y)]) { ++x; ++y; }
            V[static_cast<size_t>(off + k)] = x;
            if (x >= N && y >= M) { D = d; break; }
        }
        if (D >= 0) break;
    }
    if (D < 0) return false;
    int x = N, y = M;
    std::vector<std::pair<int, int>> rev;
    for (int d = D; d >= 0; --d) {
        const std::vector<int>& v = trace[static_cast<size_t>(d)];
        auto at = [&](int k) { return v[static_cast<size_t>(k + d + 1)]; };
        const int k = x - y;
        const int pk = (k == -d || (k != d && at(k - 1) < at(k + 1))) ? k + 1 : k - 1;
        const int px = at(pk), py = px - pk;
        while (x > px && y > py && x > 0 && y > 0) { rev.push_back({x - 1, y - 1}); --x; --y; }
        if (d > 0) { x = px; y = py; }
    }
    matches.assign(rev.rbegin(), rev.rend());
    return true;
}

// Equal words of a and b (index pairs), the common start and end taken first.
bool align(const std::vector<int>& A, const std::vector<int>& B, int maxD, std::vector<std::pair<int, int>>& out) {
    int pre = 0;
    const int n = static_cast<int>(A.size()), m = static_cast<int>(B.size());
    while (pre < n && pre < m && A[static_cast<size_t>(pre)] == B[static_cast<size_t>(pre)]) ++pre;
    int suf = 0;
    while (suf < n - pre && suf < m - pre && A[static_cast<size_t>(n - 1 - suf)] == B[static_cast<size_t>(m - 1 - suf)]) ++suf;
    std::vector<int> a(A.begin() + pre, A.end() - suf), b(B.begin() + pre, B.end() - suf);
    std::vector<std::pair<int, int>> mid;
    if (!myers(a, b, maxD, mid)) return false;
    out.clear();
    for (int i = 0; i < pre; ++i) out.push_back({i, i});
    for (const auto& p : mid) out.push_back({p.first + pre, p.second + pre});
    for (int i = suf; i > 0; --i) out.push_back({n - i, m - i});
    return true;
}

void changesFrom(const std::vector<std::pair<int, int>>& matches, int n, int m, Result& r) {
    int i = 0, j = 0;
    auto gap = [&](int ai, int bj) {
        if (ai == i && bj == j) return;
        Change c;
        c.aFrom = i; c.aTo = ai; c.bFrom = j; c.bTo = bj;
        c.kind = ai == i ? Change::Inserted : bj == j ? Change::Removed : Change::Replaced;
        r.changes.push_back(c);
    };
    for (const auto& p : matches) {
        gap(p.first, p.second);
        i = p.first + 1;
        j = p.second + 1;
        ++r.same;
    }
    gap(n, m);
}

std::wstring quoted(const std::wstring& s) {
    std::wstring t = s.size() > 300 ? s.substr(0, 300) + L"…" : s;
    for (auto& c : t) if (c == L'*' || c == L'`' || c == L'_') c = L' ';   // no Markdown inside the quotes
    return L"“" + t + L"”";
}

} // namespace

std::vector<Word> words(Document& d) {
    std::vector<Word> out;
    for (int p = 0; p < d.pageCount(); ++p) {
        const int n = d.charCount(p);
        if (n <= 0) continue;
        const std::wstring t = d.textRange(p, 0, n);
        size_t i = 0;
        while (i < t.size()) {
            while (i < t.size() && (std::iswspace(static_cast<wint_t>(t[i])) || t[i] < 32)) ++i;
            const size_t s = i;
            while (i < t.size() && !std::iswspace(static_cast<wint_t>(t[i])) && t[i] >= 32) ++i;
            if (i > s) out.push_back({p, static_cast<int>(s), static_cast<int>(i - s), t.substr(s, i - s)});
        }
    }
    return out;
}

Result compareWords(std::vector<Word> a, std::vector<Word> b, const Options& o) {
    Result r;
    r.a = std::move(a);
    r.b = std::move(b);
    std::unordered_map<std::wstring, int> ids;
    auto id = [&](const Word& w) { return ids.emplace(key(w.text, o), static_cast<int>(ids.size())).first->second; };
    std::vector<int> A, B;
    for (const auto& w : r.a) A.push_back(id(w));
    for (const auto& w : r.b) B.push_back(id(w));
    std::vector<std::pair<int, int>> matches;
    if (align(A, B, 3000, matches)) {
        changesFrom(matches, static_cast<int>(A.size()), static_cast<int>(B.size()), r);
        return r;
    }
    // very different texts: page against page (the same page numbers), each page exactly
    r.rough = true;
    matches.clear();
    const int pages = std::max(r.a.empty() ? 0 : r.a.back().page, r.b.empty() ? 0 : r.b.back().page) + 1;
    size_t ia = 0, ib = 0;
    for (int p = 0; p < pages; ++p) {
        const size_t sa = ia, sb = ib;
        while (ia < r.a.size() && r.a[ia].page == p) ++ia;
        while (ib < r.b.size() && r.b[ib].page == p) ++ib;
        std::vector<int> pa(A.begin() + static_cast<long>(sa), A.begin() + static_cast<long>(ia));
        std::vector<int> pb(B.begin() + static_cast<long>(sb), B.begin() + static_cast<long>(ib));
        std::vector<std::pair<int, int>> pm;
        if (!align(pa, pb, 2000, pm)) continue;   // nothing alike on this page
        for (const auto& q : pm) matches.push_back({q.first + static_cast<int>(sa), q.second + static_cast<int>(sb)});
    }
    changesFrom(matches, static_cast<int>(A.size()), static_cast<int>(B.size()), r);
    return r;
}

Result compare(Document& a, Document& b, const Options& o) { return compareWords(words(a), words(b), o); }

std::wstring text(const std::vector<Word>& w, int from, int to) {
    std::wstring s;
    for (int i = std::max(0, from); i < to && i < static_cast<int>(w.size()); ++i) {
        if (!s.empty()) s += L' ';
        s += w[static_cast<size_t>(i)].text;
    }
    return s;
}

std::wstring report(const Result& r, const std::wstring& nameA, const std::wstring& nameB) {
    int rep = 0, ins = 0, rem = 0;
    for (const auto& c : r.changes) (c.kind == Change::Replaced ? rep : c.kind == Change::Inserted ? ins : rem)++;
    const int total = static_cast<int>(std::max(r.a.size(), r.b.size()));
    const int pct = total ? static_cast<int>(100.0 * r.same / total + 0.5) : 100;
    std::wstring s = L"# Comparison of the text\n\n";
    s += L"**First:** " + nameA + L" (" + std::to_wstring(r.a.size()) + L" words)\n\n";
    s += L"**Second:** " + nameB + L" (" + std::to_wstring(r.b.size()) + L" words)\n\n";
    s += L"**Unchanged:** " + std::to_wstring(r.same) + L" words (" + std::to_wstring(pct) + L" %)\n\n";
    s += L"**Changes:** " + std::to_wstring(r.changes.size()) + L" (" + std::to_wstring(rep) + L" replaced, " + std::to_wstring(ins) +
         L" inserted, " + std::to_wstring(rem) + L" removed)\n\n";
    if (r.rough) s += L"The documents differ a lot, so they were compared page by page.\n\n";
    if (r.changes.empty()) { s += L"The words of the two documents are the same.\n"; return s; }
    s += L"## The changes\n\n";
    int n = 0;
    for (const auto& c : r.changes) {
        ++n;
        auto pageA = [&]() {   // an insertion is where the word before it is
            const int i = std::min(c.kind == Change::Inserted && c.aFrom > 0 ? c.aFrom - 1 : c.aFrom, static_cast<int>(r.a.size()) - 1);
            return i >= 0 ? r.a[static_cast<size_t>(i)].page + 1 : 1;
        };
        auto pageB = [&]() { const int i = std::min(c.bFrom, static_cast<int>(r.b.size()) - 1); return i >= 0 ? r.b[static_cast<size_t>(i)].page + 1 : 1; };
        const wchar_t* kind = c.kind == Change::Replaced ? L"Replaced" : c.kind == Change::Inserted ? L"Inserted" : L"Removed";
        s += L"**" + std::to_wstring(n) + L". " + kind + L"** — page " + std::to_wstring(pageA()) + L" of the first, page " +
             std::to_wstring(pageB()) + L" of the second\n\n";
        if (c.kind != Change::Inserted) s += L"- Was: " + quoted(text(r.a, c.aFrom, c.aTo)) + L"\n";
        if (c.kind != Change::Removed) s += L"- Now: " + quoted(text(r.b, c.bFrom, c.bTo)) + L"\n";
        s += L"\n";
    }
    return s;
}

int mark(Document& copy, const Result& r) {
    const auto& api = pdfium::api();
    int added = 0;
    // the rectangles of words [from, to) on one page
    auto rects = [&](int from, int to, int page) {
        std::vector<annot::RectD> out;
        FPDF_TEXTPAGE tp = copy.text(page);
        if (!tp) return out;
        for (int i = from; i < to; ++i) {
            const Word& w = r.a[static_cast<size_t>(i)];
            if (w.page != page) continue;
            const int n = api.FPDFText_CountRects(tp, w.start, w.count);
            for (int k = 0; k < n; ++k) {
                double l, t, rt, b;
                if (api.FPDFText_GetRect(tp, k, &l, &t, &rt, &b)) out.push_back({l, b, rt, t});
            }
        }
        return out;
    };
    for (const auto& c : r.changes) {
        if (c.kind == Change::Inserted) {
            // a note just after the word the insertion follows (before the first word when it comes first)
            if (r.a.empty()) continue;
            const bool after = c.aFrom > 0;
            const int wi = after ? std::min(c.aFrom - 1, static_cast<int>(r.a.size()) - 1) : 0;
            const auto rs = rects(wi, wi + 1, r.a[static_cast<size_t>(wi)].page);
            if (rs.empty()) continue;
            const annot::PointD at{after ? rs.back().right + 2 : std::max(0.0, rs.front().left - 22), rs.front().top + 2};
            if (annot::addNote(copy, r.a[static_cast<size_t>(wi)].page, at, L"Inserted: " + text(r.b, c.bFrom, c.bTo), RGB(0x20, 0xA0, 0x40))) ++added;
            continue;
        }
        const std::wstring now = c.kind == Change::Replaced ? L"Now: " + text(r.b, c.bFrom, c.bTo) : L"Removed";
        const int firstPage = r.a[static_cast<size_t>(c.aFrom)].page, lastPage = r.a[static_cast<size_t>(c.aTo - 1)].page;
        for (int p = firstPage; p <= lastPage; ++p) {
            const auto rs = rects(c.aFrom, c.aTo, p);
            if (rs.empty()) continue;
            const bool ok = c.kind == Change::Replaced
                                ? annot::addMarkup(copy, p, annot::Markup::Highlight, rs, RGB(0xFF, 0xA0, 0x30), now)
                                : annot::addMarkup(copy, p, annot::Markup::Strikethrough, rs, RGB(0xE0, 0x20, 0x20), now);
            if (ok) ++added;
        }
    }
    return added;
}

} // namespace textdiff
