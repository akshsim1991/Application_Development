#include "selftest.hpp"
#include "headfoot.hpp"
#include "platform.hpp"
#include "tables.hpp"
#include <cstdio>
#include <cwchar>
#include <string>

namespace selftest {
namespace {
double wnum(const wchar_t* s) { return std::wcstod(s, nullptr); }
int wint(const wchar_t* s) { return static_cast<int>(std::wcstol(s, nullptr, 10)); }
} // namespace

int run(const std::vector<std::wstring>& files) {
    if (!files.empty() && files[0] == L"--hfremove") {
        // --hfremove in.pdf out.pdf report.txt header|footer|both|numbers all|odd|even|<list> [area l t r b] : the
        // headers and footers found (report), and with out.pdf other than "-" what is left after deleting them
        if (files.size() < 6) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        headfoot::Detection det = headfoot::detect(doc);
        const std::wstring& what = files[4];
        headfoot::selectFor(det, what == L"header" ? 0 : what == L"footer" ? 1 : what == L"numbers" ? 3 : 2);
        if (files.size() >= 12 && files[6] == L"area") {
            headfoot::Element a;
            a.kind = headfoot::Kind::Area;
            a.area = {wnum(files[7].c_str()), wnum(files[8].c_str()), wnum(files[9].c_str()), wnum(files[10].c_str())};
            a.fromBottom = files[11] == L"bottom";
            a.refHeight = doc.pageCount() ? pdfium::api().FPDF_GetPageHeightF(doc.page(0)) : 0;
            a.label = L"Area";
            det.elements.push_back(a);
        }
        const std::wstring& pg = files[5];
        std::wstring why;
        const std::vector<int> pages = headfoot::choosePages(doc, pg == L"all" ? 0 : pg == L"odd" ? 2 : pg == L"even" ? 3 : 4, 0, pg, &why);
        std::wstring rep;
        for (const auto& e : det.elements) {
            wchar_t head[64];
            swprintf(head, 64, L"%ls %ls pages %d items %d: ", e.band == headfoot::Band::Header ? L"header" : L"footer", e.selected ? L"[x]" : L"[ ]", e.pageCount(), static_cast<int>(e.items.size()));
            rep += head + e.label + L"\n";
        }
        for (size_t p = 0; p < det.scannedPage.size(); ++p) if (det.scannedPage[p]) rep += L"scanned page " + std::to_wstring(p + 1) + (det.scannedNoText[p] ? L" (no text)" : L"") + L"\n";
        const headfoot::Plan plan = headfoot::plan(doc, det, pages);
        rep += L"plan pages " + std::to_wstring(plan.pages.size()) + L" objects " + std::to_wstring(plan.objects) + L" areas " + std::to_wstring(plan.areas) +
               L" crossing " + std::to_wstring(plan.crossing) + L" sharedKept " + std::to_wstring(plan.sharedKept) + L" body " + std::to_wstring(plan.bodyTaken) + L"\n";
        int rc = 0;
        if (files[2] != L"-") {
            if (!headfoot::apply(doc, plan, &why)) { rep += L"failed: " + why + L"\n"; rc = 6; }
            else if (doc.saveAs(files[2]) != SaveResult::Ok) rc = 4;
        }
        const std::string u = platform::utf8(rep);
        Document::writeFile(files[3], std::vector<unsigned char>(u.begin(), u.end()));
        return rc;
    }
    if (!files.empty() && files[0] == L"--tables") {   // --tables in.pdf report.txt : the tables found on every page
        if (files.size() < 3) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        std::wstring rep;
        for (int p = 0; p < doc.pageCount(); ++p)
            for (const tables::Table& t : tables::detect(doc, p)) {
                wchar_t head[256];
                swprintf(head, 256, L"page %d table %.1f %.1f %.1f %.1f rows %d cols %d editable %d scanned %d objects %d line %.2f\n", p + 1, t.left(), t.bottom(),
                         t.right(), t.top(), t.rows(), t.cols(), t.cells.empty() ? 0 : 1, t.scanned ? 1 : 0, static_cast<int>(t.objects.size()), t.lineWidth);
                rep += head;
                std::wstring h = L"  h", v = L"  v";
                for (char c : t.hRule) h += c ? L'1' : L'0';
                for (char c : t.vRule) v += c ? L'1' : L'0';
                rep += h + L"\n" + v + L"\n";
                for (int r = 0; r < t.rows() && !t.cells.empty(); ++r) {
                    rep += L"  |";
                    for (int c = 0; c < t.cols(); ++c) {
                        const tables::Cell& cell = t.at(r, c);
                        std::wstring x = cell.covered ? L"^" : cell.text;
                        for (auto& ch : x) if (ch == L'\n') ch = L'/';
                        if (!cell.covered && (cell.rowSpan > 1 || cell.colSpan > 1)) x += L"{" + std::to_wstring(cell.rowSpan) + L"x" + std::to_wstring(cell.colSpan) + L"}";
                        if (!cell.covered && cell.align) x += cell.align == 1 ? L"<c>" : L"<r>";
                        if (!cell.covered && cell.bold) x += L"<b>";
                        if (!cell.covered && cell.fill != tables::kNoFill) x += L"<f>";
                        if (!cell.covered && !cell.text.empty()) { wchar_t sz[16]; swprintf(sz, 16, L"<%.1f>", cell.size); x += sz; }
                        rep += x + L"|";
                    }
                    rep += L"\n";
                }
            }
        const std::string u = platform::utf8(rep);
        return Document::writeFile(files[2], std::vector<unsigned char>(u.begin(), u.end())) == SaveResult::Ok ? 0 : 4;
    }
    if (!files.empty() && files[0] == L"--tableedit") {   // --tableedit in.pdf out.pdf page x y op args... : changes the table there
        if (files.size() < 6) return 2;
        Document doc;
        if (doc.open(files[1]) != OpenResult::Ok) return 3;
        const int page = wint(files[3].c_str()) - 1;
        tables::Table t;
        std::wstring why;
        if (!tables::tableAt(doc, page, wnum(files[4].c_str()), wnum(files[5].c_str()), t, &why)) {
            Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(why.begin(), why.end()));
            return 5;
        }
        bool del = false;
        for (size_t i = 6; i < files.size();) {
            const std::wstring& op = files[i];
            auto num = [&](size_t k) { return i + k < files.size() ? wnum(files[i + k].c_str()) : 0.0; };
            auto in = [&](size_t k) { return static_cast<int>(num(k)); };
            if (op == L"cell") { t.at(in(1), in(2)).text = i + 3 < files.size() ? files[i + 3] : L""; i += 4; }
            else if (op == L"insrow") { tables::insertRow(t, in(1)); i += 2; }
            else if (op == L"delrow") { tables::deleteRow(t, in(1)); i += 2; }
            else if (op == L"inscol") { tables::insertColumn(t, in(1)); i += 2; }
            else if (op == L"delcol") { tables::deleteColumn(t, in(1)); i += 2; }
            else if (op == L"merge") { if (!tables::merge(t, in(1), in(2), in(3), in(4))) return 6; i += 5; }
            else if (op == L"split") { tables::split(t, in(1), in(2)); i += 3; }
            else if (op == L"width") { tables::setColumnWidth(t, in(1), num(2)); i += 3; }
            else if (op == L"height") { tables::setRowHeight(t, in(1), num(2)); i += 3; }
            else if (op == L"move") { tables::moveTo(t, num(1), num(2)); i += 3; }
            else if (op == L"resize") { tables::resize(t, num(1), num(2)); i += 3; }
            else if (op == L"align") { t.at(in(1), in(2)).align = in(3); i += 4; }
            else if (op == L"bold") { t.at(in(1), in(2)).bold = in(3) != 0; i += 4; }
            else if (op == L"fill") { t.at(in(1), in(2)).fill = static_cast<COLORREF>(std::wcstoul(files[i + 3].c_str(), nullptr, 16)); i += 4; }
            else if (op == L"borders") { const std::wstring& b = files[i + 1]; tables::setBorders(t, b == L"none" ? tables::Borders::None : b == L"outside" ? tables::Borders::Outside : b == L"rows" ? tables::Borders::Rows : tables::Borders::All); i += 2; }
            else if (op == L"delete") { del = true; i += 1; }
            else return 2;
        }
        bool shrunk = false;
        double grew = 0;
        if (del ? !tables::remove(doc, t) : !tables::write(doc, t, &why, &grew, &shrunk)) {
            Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(why.begin(), why.end()));
            return 7;
        }
        {   // what happened, for the tests: "grew 12.5" / "shrunk"
            std::string note = shrunk ? "shrunk\n" : grew > 0.01 ? "grew " + std::to_string(grew) + "\n" : "same height\n";
            Document::writeFile(files[2] + L".txt", std::vector<unsigned char>(note.begin(), note.end()));
        }
        return doc.saveAs(files[2]) == SaveResult::Ok ? 0 : 4;
    }
    return -1;
}

} // namespace selftest
