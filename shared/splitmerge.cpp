#include "splitmerge.hpp"
#include "outline.hpp"
#include <algorithm>
#include <map>

namespace splitmerge {

namespace {

// The tree with each page moved by `map` (old page -> new page); entries whose page is not in the map are left out,
// their children kept in their place.
void remapInto(const std::vector<Bookmark>& v, const std::map<int, int>& map, std::vector<Bookmark>& out) {
    for (const auto& b : v) {
        auto it = map.find(b.page);
        if (it != map.end()) {
            Bookmark c;
            c.title = b.title;
            c.page = it->second;
            remapInto(b.children, map, c.children);
            out.push_back(std::move(c));
        } else remapInto(b.children, map, out);
    }
}

std::vector<Bookmark> remap(const std::vector<Bookmark>& v, const std::map<int, int>& map) {
    std::vector<Bookmark> out;
    remapInto(v, map, out);
    return out;
}

bool withBookmarks(std::vector<unsigned char>& bytes, const std::vector<Bookmark>& tree) {
    if (tree.empty()) return true;
    std::vector<unsigned char> marked;
    if (!outline::write(bytes, tree, marked)) return true;   // the pages are right; the bookmarks could not be written
    bytes.swap(marked);
    return true;
}

void atLevel(const std::vector<Bookmark>& v, int level, std::vector<const Bookmark*>& out) {
    for (const auto& b : v) {
        if (level <= 1) out.push_back(&b);
        else atLevel(b.children, level - 1, out);
    }
}

} // namespace

std::vector<Part> everyN(int pageCount, int n) {
    std::vector<Part> parts;
    if (n < 1) return parts;
    for (int i = 0; i < pageCount; i += n) {
        Part p;
        for (int k = i; k < std::min(pageCount, i + n); ++k) p.pages.push_back(k);
        parts.push_back(p);
    }
    return parts;
}

std::vector<Part> byBookmarks(Document& d, int level) {
    std::vector<Part> parts;
    const int n = d.pageCount();
    std::vector<const Bookmark*> marks;
    const std::vector<Bookmark> tree = d.bookmarks();
    atLevel(tree, level, marks);
    // where each part starts: the first bookmark on a page wins; bookmarks without a page are skipped
    std::map<int, std::wstring> starts;
    for (const Bookmark* b : marks)
        if (b->page >= 0 && b->page < n && !starts.count(b->page)) starts[b->page] = b->title;
    if (starts.empty()) return parts;
    if (starts.begin()->first > 0) starts[0] = L"";   // the pages before the first bookmark (a cover, contents)
    for (auto it = starts.begin(); it != starts.end(); ++it) {
        auto nx = std::next(it);
        const int end = nx == starts.end() ? n : nx->first;
        Part p;
        p.name = it->second;
        for (int k = it->first; k < end; ++k) p.pages.push_back(k);
        parts.push_back(p);
    }
    return parts;
}

std::vector<Part> bySize(Document& d, double maxBytes) {
    std::vector<Part> parts;
    const int n = d.pageCount();
    if (n <= 0 || maxBytes <= 0) return parts;
    // each page's size on its own (its fonts and pictures counted with it), then whole parts checked and halved if needed
    std::vector<double> size(static_cast<size_t>(n), 0);
    std::vector<unsigned char> b;
    for (int i = 0; i < n; ++i) if (d.pagesToBytes({i}, b)) size[static_cast<size_t>(i)] = static_cast<double>(b.size());
    Part cur;
    double sum = 0;
    for (int i = 0; i < n; ++i) {
        if (!cur.pages.empty() && sum + size[static_cast<size_t>(i)] > maxBytes) { parts.push_back(cur); cur = Part(); sum = 0; }
        cur.pages.push_back(i);
        sum += size[static_cast<size_t>(i)];
    }
    if (!cur.pages.empty()) parts.push_back(cur);
    std::vector<Part> checked;
    std::vector<Part> todo(parts.rbegin(), parts.rend());
    while (!todo.empty()) {
        Part p = todo.back();
        todo.pop_back();
        if (p.pages.size() > 1 && d.pagesToBytes(p.pages, b) && static_cast<double>(b.size()) > maxBytes) {
            const size_t half = p.pages.size() / 2;
            Part a, c;
            a.pages.assign(p.pages.begin(), p.pages.begin() + static_cast<long>(half));
            c.pages.assign(p.pages.begin() + static_cast<long>(half), p.pages.end());
            todo.push_back(c);
            todo.push_back(a);
            continue;
        }
        checked.push_back(p);
    }
    return checked;
}

bool partBytes(Document& d, const std::vector<int>& pages, std::vector<unsigned char>& out) {
    if (!d.pagesToBytes(pages, out)) return false;
    std::map<int, int> map;
    for (size_t i = 0; i < pages.size(); ++i) if (!map.count(pages[i])) map[pages[i]] = static_cast<int>(i);
    return withBookmarks(out, remap(d.bookmarks(), map));
}

std::wstring fileSafe(const std::wstring& title) {
    std::wstring s;
    for (wchar_t c : title) {
        if (c < 32 || std::wstring(L"\\/:*?\"<>|").find(c) != std::wstring::npos) c = L' ';
        if (c == L' ' && (s.empty() || s.back() == L' ')) continue;
        s += c;
        if (s.size() >= 60) break;
    }
    while (!s.empty() && (s.back() == L' ' || s.back() == L'.')) s.pop_back();
    return s;
}

bool merge(const std::vector<Source>& docs, Marks marks, std::vector<unsigned char>& out) {
    if (docs.empty() || !docs.front().doc) return false;
    std::vector<unsigned char> first;
    if (!docs.front().doc->serialize(first)) return false;
    Document all;
    if (all.load(std::move(first), "") != OpenResult::Ok) return false;
    std::vector<Bookmark> tree;
    int offset = 0;
    for (size_t i = 0; i < docs.size(); ++i) {
        Document* src = docs[i].doc;
        if (!src) return false;
        const int n = src->pageCount();
        if (i > 0) {
            std::vector<int> every(static_cast<size_t>(n));
            for (int k = 0; k < n; ++k) every[static_cast<size_t>(k)] = k;
            if (!all.importPages(*src, every, all.pageCount())) return false;
        }
        if (marks != Marks::None) {
            std::map<int, int> map;
            for (int k = 0; k < n; ++k) map[k] = offset + k;
            std::vector<Bookmark> own = remap(src->bookmarks(), map);
            if (marks == Marks::UnderFileNames) {
                Bookmark file;
                file.title = docs[i].name.empty() ? src->title() : docs[i].name;
                file.page = offset;
                file.children = std::move(own);
                tree.push_back(std::move(file));
            } else for (auto& b : own) tree.push_back(std::move(b));
        }
        offset += n;
    }
    if (!all.serialize(out)) return false;
    // the first document's own outline is in `out` already; write the combined one (or none) in its place
    std::vector<unsigned char> marked;
    if (outline::write(out, tree, marked)) out.swap(marked);
    return true;
}

std::vector<int> interleaveOrder(int fronts, int backs, bool backsReversed) {
    std::vector<int> order;
    for (int i = 0; i < std::max(fronts, backs); ++i) {
        if (i < fronts) order.push_back(i);
        if (i < backs) order.push_back(fronts + (backsReversed ? backs - 1 - i : i));
    }
    return order;
}

bool interleave(Document& d, Document* backs, bool backsReversed, std::vector<unsigned char>& out) {
    if (!backs) {
        const int n = d.pageCount();
        if (n < 2) return false;
        const int fronts = (n + 1) / 2;
        return d.pagesToBytes(interleaveOrder(fronts, n - fronts, backsReversed), out);
    }
    std::vector<unsigned char> both;
    std::vector<Source> two{{&d, L""}, {backs, L""}};
    if (!merge(two, Marks::None, both)) return false;
    Document all;
    if (all.load(std::move(both), "") != OpenResult::Ok) return false;
    return all.pagesToBytes(interleaveOrder(d.pageCount(), backs->pageCount(), backsReversed), out);
}

} // namespace splitmerge
