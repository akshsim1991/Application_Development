#include "formcreate.hpp"
#include "pdfedit.hpp"
#include <algorithm>

namespace formcreate {
using namespace pdfedit;

namespace {

std::string num(double v) { char b[32]; snprintf(b, sizeof(b), "%.2f", v); return b; }

// the elements (as text) of an array value: "[ 1 0 R 2 0 R ]" -> "1 0 R 2 0 R"
std::string arrayInner(const std::string& v) {
    size_t a = v.find('['), z = v.rfind(']');
    if (a == std::string::npos || z == std::string::npos || z < a) return {};
    return v.substr(a + 1, z - a - 1);
}

std::string formStream(int w, int h, const std::string& content) {
    return "<< /Type /XObject /Subtype /Form /BBox [0 0 " + std::to_string(w) + " " + std::to_string(h) + "] /Length " + std::to_string(content.size()) + " >>\nstream\n" + content + "\nendstream";
}

} // namespace

bool add(const std::vector<unsigned char>& pdf, const std::vector<Spec>& specs, std::vector<unsigned char>& out, std::wstring& error) {
    if (specs.empty()) { error = L"Nothing to add."; return false; }
    const std::string s(pdf.begin(), pdf.end());
    File f(s);
    if (!f.parseXref()) { error = L"The structure of this document is not supported."; return false; }
    const int root = refOf(f.trailer, "/Root");
    std::string cat;
    if (root < 0 || !f.object(root, cat)) { error = L"The document catalog could not be read."; return false; }
    const int pagesNode = refOf(cat, "/Pages");
    if (pagesNode < 0) { error = L"The page tree could not be read."; return false; }

    int next = f.size;
    std::map<int, std::string> write;
    std::map<int, std::vector<int>> widgetsByPage;   // page object -> new widgets
    std::vector<int> allWidgets;
    // Helvetica for the field text
    const int fontObj = next++;
    write[fontObj] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

    for (const Spec& sp : specs) {
        int idx = sp.page;
        std::set<int> seen;
        const int page = pageRef(f, pagesNode, idx, seen);
        if (page < 0) { error = L"A page could not be found."; return false; }
        const double l = std::min(sp.left, sp.right), r = std::max(sp.left, sp.right), b = std::min(sp.bottom, sp.top), t = std::max(sp.bottom, sp.top);
        const int w = std::max(2, static_cast<int>(r - l)), h = std::max(2, static_cast<int>(t - b));
        const int obj = next++;
        std::string d = "<< /Type /Annot /Subtype /Widget /T " + pdfString(sp.name) + " /Rect [" + num(l) + " " + num(b) + " " + num(r) + " " + num(t) + "] /F 4 /P " + std::to_string(page) + " 0 R";
        if (sp.kind == Spec::Text) {
            d += " /FT /Tx /Ff " + std::string(sp.multiline ? "4096" : "0") + " /DA (/Helv 0 Tf 0 g) /MK << /BC [0.3 0.3 0.3] /BG [0.93 0.96 1] >> /V ()";
        } else if (sp.kind == Spec::Combo) {
            d += " /FT /Ch /Ff 131072 /DA (/Helv 0 Tf 0 g) /MK << /BC [0.3 0.3 0.3] /BG [0.93 0.96 1] >> /Opt [";
            for (const auto& o : sp.options) d += pdfString(o) + " ";
            d += "]";
        } else {
            const int on = next++, off = next++;
            const std::string box = "0.3 G 1 w 0.5 0.5 " + std::to_string(w - 1) + " " + std::to_string(h - 1) + " re S\n";
            write[off] = formStream(w, h, "q 0.93 0.96 1 rg 0 0 " + std::to_string(w) + " " + std::to_string(h) + " re f Q\n" + box);
            write[on] = formStream(w, h, "q 0.93 0.96 1 rg 0 0 " + std::to_string(w) + " " + std::to_string(h) + " re f Q\n" + box +
                                   "0 G 2 w 1 J " + num(w * 0.2) + " " + num(h * 0.5) + " m " + num(w * 0.42) + " " + num(h * 0.22) + " l " + num(w * 0.8) + " " + num(h * 0.8) + " l S\n");
            d += " /FT /Btn /Ff 0 /AS /Off /V /Off /AP << /N << /Yes " + std::to_string(on) + " 0 R /Off " + std::to_string(off) + " 0 R >> >> /MK << /BC [0.3 0.3 0.3] >>";
        }
        d += " >>";
        write[obj] = d;
        widgetsByPage[page].push_back(obj);
        allWidgets.push_back(obj);
    }

    // the pages: append the widgets to /Annots
    for (auto& pg : widgetsByPage) {
        std::string body;
        if (!f.object(pg.first, body)) { error = L"A page could not be read."; return false; }
        std::string refs;
        for (int o : pg.second) refs += std::to_string(o) + " 0 R ";
        size_t vb, ve;
        if (!valueSpan(body, "/Annots", vb, ve)) {
            body = withEntry(body, "/Annots [" + refs + "]");
        } else if (body[vb] == '[') {
            size_t close = body.rfind(']', ve);
            body.insert(close, " " + refs);
        } else {   // an indirect array object
            const int arr = refOf(body, "/Annots");
            std::string ab;
            if (arr < 0 || !f.object(arr, ab)) { error = L"The annotation list of a page could not be read."; return false; }
            size_t close = ab.rfind(']');
            if (close == std::string::npos) { error = L"The annotation list of a page could not be read."; return false; }
            ab.insert(close, " " + refs);
            write[arr] = ab;
        }
        write[pg.first] = body;
    }

    // the interactive form of the document
    std::string acro;
    int acroObj = -1;
    size_t cb, ce;
    bool inlineAcro = false;
    if (valueSpan(cat, "/AcroForm", cb, ce)) {
        if (cat[cb] == '<') { acro = cat.substr(cb, ce - cb); inlineAcro = true; }
        else { acroObj = refOf(cat, "/AcroForm"); if (acroObj < 0 || !f.object(acroObj, acro)) { error = L"The form of this document could not be read."; return false; } }
    }
    std::string fields;
    if (!acro.empty()) {
        size_t fb, fe;
        if (valueSpan(acro, "/Fields", fb, fe)) {
            if (acro[fb] == '[') fields = arrayInner(acro.substr(fb, fe - fb));
            else {
                const int arr = refOf(acro, "/Fields");
                std::string ab;
                if (arr >= 0 && f.object(arr, ab)) fields = arrayInner(ab);
            }
        }
    } else acro = "<< >>";
    for (int o : allWidgets) fields += " " + std::to_string(o) + " 0 R";
    acro = setKey(acro, "/Fields", "[" + fields + "]");
    acro = setKey(acro, "/NeedAppearances", "true");
    size_t db, de;
    if (!valueSpan(acro, "/DA", db, de)) acro = setKey(acro, "/DA", "(/Helv 0 Tf 0 g)");
    if (!valueSpan(acro, "/DR", db, de)) acro = setKey(acro, "/DR", "<< /Font << /Helv " + std::to_string(fontObj) + " 0 R >> >>");
    else {   // keep the document's own resources and make sure /Helv exists
        std::string dr = acro.substr(db, de - db);
        size_t hb, he;
        if (dr[0] == '<') {
            if (!valueSpan(dr, "/Font", hb, he)) dr = setKey(dr, "/Font", "<< /Helv " + std::to_string(fontObj) + " 0 R >>");
            else if (dr[hb] == '<' && dr.find("/Helv", hb) == std::string::npos) { std::string fnt = dr.substr(hb, he - hb); fnt.insert(fnt.rfind(">>"), " /Helv " + std::to_string(fontObj) + " 0 R "); dr = dr.substr(0, hb) + fnt + dr.substr(he); }
            acro = acro.substr(0, db) + dr + acro.substr(de);
        }
    }
    if (acroObj >= 0) write[acroObj] = acro;
    else {
        acroObj = next++;
        write[acroObj] = acro;
        std::string newCat = cat;
        if (inlineAcro) newCat = cat.substr(0, cb) + std::to_string(acroObj) + " 0 R" + cat.substr(ce);
        else newCat = withEntry(cat, "/AcroForm " + std::to_string(acroObj) + " 0 R");
        write[root] = newCat;
    }
    out = appendIncremental(s, f, write, next);
    return true;
}

} // namespace formcreate
