#include "review.hpp"
#include "attach.hpp"   // layers::decodeString
#include "pdfedit.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <ctime>
#include <functional>
#include <map>

namespace review {
using namespace pdfedit;

const wchar_t* stateName(State s) {
    switch (s) {
        case State::Accepted: return L"Accepted";
        case State::Rejected: return L"Rejected";
        case State::Cancelled: return L"Cancelled";
        case State::Completed: return L"Completed";
        default: return L"None";
    }
}

namespace {

struct Raw {
    std::string s;
    File f{s};
    std::map<int, std::string> dict;
    std::vector<int> pages;
    std::vector<std::vector<int>> annots;   // per page, the annotation objects in /Annots order
    bool ok = false;

    explicit Raw(Document& d) {
        std::vector<unsigned char> b;
        if (!d.serialize(b, FPDF_NO_INCREMENTAL)) return;
        s.assign(b.begin(), b.end());
        if (!f.parseXref()) return;
        for (const auto& kv : f.offsets) {
            std::string body;
            if (!f.object(kv.first, body)) continue;
            const size_t st = body.find("stream");
            const size_t gt = body.rfind(">>", st == std::string::npos ? std::string::npos : st);
            dict[kv.first] = (st != std::string::npos && gt != std::string::npos && st > gt) ? body.substr(0, gt + 2) : body;
        }
        const int root = refOf(f.trailer, "/Root");
        if (!dict.count(root)) return;
        std::function<void(int, int)> walk = [&](int n, int depth) {
            if (depth > 64 || !dict.count(n)) return;
            const std::string& o = dict[n];
            if (o.find("/Kids") != std::string::npos) { for (int k : kidsOf(o)) walk(k, depth + 1); return; }
            pages.push_back(n);
        };
        walk(refOf(dict[root], "/Pages"), 0);
        for (int p : pages) annots.push_back(refsIn(value(dict[p], "/Annots")));
        ok = !pages.empty();
    }
    std::string value(const std::string& d0, const char* key) const {
        size_t b, e;
        if (!valueSpan(d0, key, b, e)) return {};
        std::string v = d0.substr(b, e - b);
        if (!v.empty() && v.back() == 'R') {
            auto it = dict.find(atoi(v.c_str()));
            return it == dict.end() ? std::string() : it->second;
        }
        return v;
    }
    static std::vector<int> refsIn(const std::string& arr) {
        std::vector<int> out;
        size_t p = 0;
        while ((p = arr.find(" R", p)) != std::string::npos) {
            // "N G R": walk back over the generation and the number
            size_t q = p;
            while (q > 0 && std::isdigit(static_cast<unsigned char>(arr[q - 1]))) --q;   // generation
            size_t r = q;
            while (r > 0 && arr[r - 1] == ' ') --r;
            size_t n = r;
            while (n > 0 && std::isdigit(static_cast<unsigned char>(arr[n - 1]))) --n;
            if (n < r) out.push_back(atoi(arr.c_str() + n));
            p += 2;
        }
        return out;
    }
};

std::wstring str(const std::string& t) {
    std::string v = t;
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) v.erase(0, 1);
    while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back()))) v.pop_back();
    return layers::decodeString(v);
}

std::string name(const std::string& d0, const char* key) {
    size_t b, e;
    return valueSpan(d0, key, b, e) ? d0.substr(b, e - b) : std::string();
}

// "D:20261106032400+05'30'" -> "2026-11-06 03:24"
std::wstring prettyDate(const std::wstring& d) {
    std::wstring t = d.rfind(L"D:", 0) == 0 ? d.substr(2) : d;
    if (t.size() < 12) return t;
    return t.substr(0, 4) + L"-" + t.substr(4, 2) + L"-" + t.substr(6, 2) + L" " + t.substr(8, 2) + L":" + t.substr(10, 2);
}

std::string pdfNow() {
    const time_t now = time(nullptr);
    tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &now);
#else
    localtime_r(&now, &lt);
#endif
    char b[32];
    strftime(b, sizeof(b), "(D:%Y%m%d%H%M%S)", &lt);
    return b;
}

std::wstring kindOf(const std::string& sub) {
    if (sub == "/Text") return L"Note";
    if (sub == "/Highlight") return L"Highlight";
    if (sub == "/Underline") return L"Underline";
    if (sub == "/StrikeOut") return L"Strike-out";
    if (sub == "/Squiggly") return L"Squiggly underline";
    if (sub == "/FreeText") return L"Text box";
    if (sub == "/Stamp") return L"Stamp";
    if (sub == "/Ink") return L"Drawing";
    if (sub == "/Square") return L"Rectangle";
    if (sub == "/Circle") return L"Ellipse";
    if (sub == "/Line") return L"Line";
    if (sub == "/Polygon" || sub == "/PolyLine") return L"Shape";
    if (sub == "/Caret") return L"Insertion mark";
    if (sub == "/FileAttachment") return L"Attached file";
    if (sub == "/Sound") return L"Sound";
    return L"Comment";
}

State stateFrom(const std::wstring& s) {
    if (s == L"Accepted") return State::Accepted;
    if (s == L"Rejected") return State::Rejected;
    if (s == L"Cancelled") return State::Cancelled;
    if (s == L"Completed") return State::Completed;
    return State::None;
}

// Adds a new annotation `body` to the page of `parent` and reloads the document (one undo step).
bool addToPage(Document& d, Raw& r, int parent, const std::string& bodyWithoutIrt, const std::wstring& what) {
    int pageIdx = -1;
    for (size_t i = 0; i < r.annots.size(); ++i)
        if (std::find(r.annots[i].begin(), r.annots[i].end(), parent) != r.annots[i].end()) pageIdx = static_cast<int>(i);
    if (pageIdx < 0) return false;
    const int page = r.pages[static_cast<size_t>(pageIdx)];
    const std::string& pd = r.dict[page];
    std::string rect = name(r.dict[parent], "/Rect");
    if (rect.empty()) rect = "[0 0 0 0]";
    std::map<int, std::string> write;
    const int n = r.f.size;
    write[n] = bodyWithoutIrt.substr(0, bodyWithoutIrt.size() - 2) + " /IRT " + std::to_string(parent) + " 0 R /P " + std::to_string(page) +
               " 0 R /Rect " + rect + " >>";
    size_t b, e;
    if (valueSpan(pd, "/Annots", b, e)) {
        const std::string v = pd.substr(b, e - b);
        if (!v.empty() && v.back() == 'R') {   // the array is an object of its own
            const int an = atoi(v.c_str());
            std::string arr = r.dict[an];
            const size_t close = arr.rfind(']');
            if (close == std::string::npos) return false;
            arr.insert(close, " " + std::to_string(n) + " 0 R");
            write[an] = arr;
        } else {
            std::string arr = v;
            const size_t close = arr.rfind(']');
            if (close == std::string::npos) return false;
            arr.insert(close, " " + std::to_string(n) + " 0 R");
            write[page] = setKey(pd, "/Annots", arr);
        }
    } else return false;
    std::vector<unsigned char> out = appendIncremental(r.s, r.f, write, n + 1);
    d.beginChange(what);
    if (d.reload(std::move(out)) != OpenResult::Ok) { d.rollbackChange(); return false; }
    return true;
}

} // namespace

std::vector<Thread> threads(Document& d) {
    std::vector<Thread> out;
    Raw r(d);
    if (!r.ok) return out;
    std::map<int, size_t> byObject;   // comment object -> index in out
    struct Pending { int parent; std::string body; };
    std::vector<Pending> children;
    const auto& api = pdfium::api();
    for (size_t p = 0; p < r.annots.size(); ++p) {
        for (int a : r.annots[p]) {
            auto it = r.dict.find(a);
            if (it == r.dict.end()) continue;
            const std::string& o = it->second;
            const std::string sub = name(o, "/Subtype");
            if (sub == "/Popup" || sub == "/Link" || sub == "/Widget" || sub.empty()) continue;
            const int irt = refOf(o, "/IRT");
            if (irt >= 0) { children.push_back({irt, o}); continue; }
            Thread t;
            t.page = static_cast<int>(p);
            t.object = a;
            t.kind = kindOf(sub);
            t.author = str(name(o, "/T"));
            t.text = str(name(o, "/Contents"));
            t.date = prettyDate(str(name(o, "/M")));
            if (sub == "/Highlight" || sub == "/Underline" || sub == "/StrikeOut" || sub == "/Squiggly") {
                std::string rect = name(o, "/Rect");
                double l = 0, b = 0, rr = 0, tp = 0;
                if (sscanf(rect.c_str(), " [ %lf %lf %lf %lf", &l, &b, &rr, &tp) == 4 || sscanf(rect.c_str(), "[%lf %lf %lf %lf", &l, &b, &rr, &tp) == 4) {
                    FPDF_TEXTPAGE tpg = d.text(static_cast<int>(p));
                    const int n = tpg ? api.FPDFText_GetBoundedText(tpg, l, tp, rr, b, nullptr, 0) : 0;
                    if (n > 0) {
                        std::vector<unsigned short> buf(static_cast<size_t>(n) + 1);
                        api.FPDFText_GetBoundedText(tpg, l, tp, rr, b, buf.data(), n);
                        std::wstring q(buf.begin(), buf.begin() + n);
                        for (auto& c : q) if (c == L'\r' || c == L'\n') c = L' ';
                        while (!q.empty() && (q.back() == L' ' || q.back() == 0)) q.pop_back();
                        t.quoted = q;
                    }
                }
            }
            byObject[a] = out.size();
            out.push_back(t);
        }
    }
    // replies and statuses, in the order they were added
    for (const auto& c : children) {
        auto it = byObject.find(c.parent);
        if (it == byObject.end()) continue;
        Thread& t = out[it->second];
        const std::wstring st = str(name(c.body, "/State"));
        const std::wstring model = str(name(c.body, "/StateModel"));
        if (!st.empty() && (model.empty() || model == L"Review")) {
            t.state = stateFrom(st);
            t.stateBy = str(name(c.body, "/T"));
            continue;
        }
        if (!st.empty()) continue;   // marked / unmarked (the other state model)
        t.replies.push_back({str(name(c.body, "/T")), str(name(c.body, "/Contents")), prettyDate(str(name(c.body, "/M")))});
    }
    return out;
}

bool reply(Document& d, int object, const std::wstring& author, const std::wstring& text) {
    Raw r(d);
    if (!r.ok || !r.dict.count(object) || text.empty()) return false;
    const std::string body = "<< /Type /Annot /Subtype /Text /Contents " + pdfString(text) + " /T " + pdfString(author.empty() ? L"Reviewer" : author) +
                             " /M " + pdfNow() + " /RT /R /Name /Comment /F 32 /C [1 0.82 0.2] >>";
    return addToPage(d, r, object, body, L"Reply to a comment");
}

bool setState(Document& d, int object, State s, const std::wstring& author) {
    Raw r(d);
    if (!r.ok || !r.dict.count(object)) return false;
    const std::wstring who = author.empty() ? L"Reviewer" : author;
    const std::string body = "<< /Type /Annot /Subtype /Text /Contents " + pdfString(std::wstring(stateName(s)) + L" set by " + who) + " /T " +
                             pdfString(who) + " /M " + pdfNow() + " /State " + pdfString(stateName(s)) + " /StateModel (Review) /F 34 >>";
    return addToPage(d, r, object, body, std::wstring(L"Mark a comment ") + stateName(s));
}

std::wstring summary(Document& d) {
    const auto all = threads(d);
    std::map<State, int> counts;
    int replies = 0;
    for (const auto& t : all) { ++counts[t.state]; replies += static_cast<int>(t.replies.size()); }
    std::wstring s = L"# Review summary\n\n**Document:** " + d.title() + L"\n\n**Comments:** " + std::to_wstring(all.size()) + L" (" +
                     std::to_wstring(replies) + L" replies)\n\n";
    s += L"**Accepted:** " + std::to_wstring(counts[State::Accepted]) + L"  ·  **Rejected:** " + std::to_wstring(counts[State::Rejected]) +
         L"  ·  **Completed:** " + std::to_wstring(counts[State::Completed]) + L"  ·  **Cancelled:** " + std::to_wstring(counts[State::Cancelled]) +
         L"  ·  **Open:** " + std::to_wstring(counts[State::None]) + L"\n\n";
    int page = -1;
    for (const auto& t : all) {
        if (t.page != page) { page = t.page; s += L"## Page " + std::to_wstring(page + 1) + L"\n\n"; }
        s += L"**" + t.kind + L"**";
        if (!t.author.empty()) s += L" by " + t.author;
        if (!t.date.empty()) s += L", " + t.date;
        s += L" — *" + std::wstring(t.state == State::None ? L"Open" : stateName(t.state)) + (t.stateBy.empty() ? L"" : L" (" + t.stateBy + L")") + L"*\n\n";
        if (!t.quoted.empty()) s += L"> " + t.quoted + L"\n\n";
        if (!t.text.empty()) s += t.text + L"\n\n";
        for (const auto& r : t.replies) s += L"- **" + (r.author.empty() ? std::wstring(L"Reply") : r.author) + L"**" + (r.date.empty() ? L"" : L" (" + r.date + L")") + L": " + r.text + L"\n";
        if (!t.replies.empty()) s += L"\n";
    }
    if (all.empty()) s += L"There are no comments in this document.\n";
    return s;
}

} // namespace review
