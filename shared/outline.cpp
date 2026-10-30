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

} // namespace outline
