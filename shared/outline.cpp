#include "outline.hpp"
#include "pdfedit.hpp"

namespace outline {
using namespace pdfedit;

bool addBookmark(const std::vector<unsigned char>& pdf, int pageIndex, const std::wstring& title, std::vector<unsigned char>& out) {
    const std::string s(pdf.begin(), pdf.end());
    File f(s);
    if (!f.parseXref()) return false;
    const int root = refOf(f.trailer, "/Root");
    if (root < 0) return false;
    std::string cat;
    if (!f.object(root, cat)) return false;
    const int pagesNode = refOf(cat, "/Pages");
    if (pagesNode < 0) return false;
    int idx = pageIndex;
    std::set<int> seen;
    const int page = pageRef(f, pagesNode, idx, seen);
    if (page < 0) return false;

    int next = f.size;
    const int item = next++;
    std::map<int, std::string> write;   // object number -> full object text (without "N 0 obj")
    int outlines = refOf(cat, "/Outlines");
    std::string itemText = "<< /Title " + titleHex(title) + " /Parent ";
    if (outlines < 0) {
        outlines = next++;
        itemText += std::to_string(outlines) + " 0 R /Dest [" + std::to_string(page) + " 0 R /Fit] >>";
        write[outlines] = "<< /Type /Outlines /First " + std::to_string(item) + " 0 R /Last " + std::to_string(item) + " 0 R /Count 1 >>";
        write[root] = withEntry(cat, "/Outlines " + std::to_string(outlines) + " 0 R");
        if (write[root].empty()) return false;
    } else {
        std::string ob;
        if (!f.object(outlines, ob)) return false;
        const int last = refOf(ob, "/Last");
        itemText += std::to_string(outlines) + " 0 R ";
        if (last >= 0) {
            std::string lb;
            if (!f.object(last, lb)) return false;
            itemText += "/Prev " + std::to_string(last) + " 0 R ";
            std::string nl = withEntry(lb, "/Next " + std::to_string(item) + " 0 R");
            if (nl.empty()) return false;
            write[last] = nl;
        }
        itemText += "/Dest [" + std::to_string(page) + " 0 R /Fit] >>";
        std::string nb = ob;
        if (last >= 0) nb = replaceKey(nb, "/Last", std::to_string(item) + " 0 R");
        else { nb = withEntry(nb, "/First " + std::to_string(item) + " 0 R"); nb = withEntry(nb, "/Last " + std::to_string(item) + " 0 R"); }
        size_t cp = nb.find("/Count");
        int count = cp == std::string::npos ? 0 : atoi(nb.c_str() + cp + 6);
        nb = replaceKey(nb, "/Count", std::to_string(count < 0 ? count - 1 : count + 1));
        write[outlines] = nb;
    }
    write[item] = itemText;

    out = appendIncremental(s, f, write, next);
    return true;
}

namespace {

void collectPages(const File& f, int node, std::vector<int>& out, std::set<int>& seen, int depth = 0) {
    if (depth > 64 || !seen.insert(node).second) return;
    std::string body;
    if (!f.object(node, body)) return;
    if (body.find("/Kids") == std::string::npos) { out.push_back(node); return; }
    for (int kid : kidsOf(body)) collectPages(f, kid, out, seen, depth + 1);
}

int countAll(const std::vector<Bookmark>& v) {
    int n = 0;
    for (const auto& b : v) n += 1 + countAll(b.children);
    return n;
}

// Gives each entry an object number (depth first) and writes them.
void emit(const std::vector<Bookmark>& v, int parent, int& next, const std::vector<int>& pages, std::map<int, std::string>& write,
          int& first, int& last) {
    std::vector<int> nums;
    for (size_t i = 0; i < v.size(); ++i) nums.push_back(next++);
    first = nums.empty() ? -1 : nums.front();
    last = nums.empty() ? -1 : nums.back();
    for (size_t i = 0; i < v.size(); ++i) {
        const Bookmark& b = v[i];
        std::string t = "<< /Title " + titleHex(b.title) + " /Parent " + std::to_string(parent) + " 0 R";
        if (i > 0) t += " /Prev " + std::to_string(nums[i - 1]) + " 0 R";
        if (i + 1 < v.size()) t += " /Next " + std::to_string(nums[i + 1]) + " 0 R";
        if (b.page >= 0 && b.page < static_cast<int>(pages.size())) t += " /Dest [" + std::to_string(pages[static_cast<size_t>(b.page)]) + " 0 R /Fit]";
        if (!b.children.empty()) {
            int cf = -1, cl = -1;
            emit(b.children, nums[i], next, pages, write, cf, cl);
            t += " /First " + std::to_string(cf) + " 0 R /Last " + std::to_string(cl) + " 0 R /Count -" + std::to_string(countAll(b.children));
        }
        write[nums[i]] = t + " >>";
    }
}

} // namespace

bool write(const std::vector<unsigned char>& pdf, const std::vector<Bookmark>& tree, std::vector<unsigned char>& out) {
    const std::string s(pdf.begin(), pdf.end());
    File f(s);
    if (!f.parseXref()) return false;
    const int root = refOf(f.trailer, "/Root");
    if (root < 0) return false;
    std::string cat;
    if (!f.object(root, cat)) return false;
    const int pagesNode = refOf(cat, "/Pages");
    if (pagesNode < 0) return false;
    std::vector<int> pages;
    std::set<int> seen;
    collectPages(f, pagesNode, pages, seen);
    if (pages.empty()) return false;

    int next = f.size;
    std::map<int, std::string> write;
    const int outlines = next++;
    int first = -1, last = -1;
    emit(tree, outlines, next, pages, write, first, last);
    std::string ol = "<< /Type /Outlines";
    if (first >= 0) ol += " /First " + std::to_string(first) + " 0 R /Last " + std::to_string(last) + " 0 R /Count " + std::to_string(tree.size());
    write[outlines] = ol + " >>";
    std::string nc = setKey(cat, "/Outlines", std::to_string(outlines) + " 0 R");
    if (nc.empty()) return false;
    write[root] = nc;
    out = appendIncremental(s, f, write, next);
    return true;
}

} // namespace outline
