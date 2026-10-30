#include "formdata.hpp"
#include "contentgen.hpp"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <set>

namespace formdata {
namespace {

std::wstring wide(const std::vector<unsigned short>& buf, unsigned long bytes) {
    if (bytes < 2) return {};
    return std::wstring(buf.begin(), buf.begin() + (bytes / 2 - 1));
}

template <typename Fn>
std::wstring getString(Fn fn) {
    unsigned long n = fn(nullptr, 0);
    if (n < 2) return {};
    std::vector<unsigned short> buf(n / 2 + 1);
    fn(buf.data(), n);
    return wide(buf, n);
}

bool truthy(const std::wstring& v) {
    std::wstring l = v;
    for (auto& c : l) c = static_cast<wchar_t>(towlower(c));
    return l == L"1" || l == L"yes" || l == L"y" || l == L"true" || l == L"x" || l == L"on" || l == L"checked" || l == L"ja" || l == L"oui" || l == L"si";
}

} // namespace

std::vector<Field> list(Document& d) {
    std::vector<Field> out;
    FPDF_FORMHANDLE form = d.form();
    if (!form) return out;
    const auto& api = pdfium::api();
    for (int p = 0; p < d.pageCount(); ++p) {
        FPDF_PAGE pg = d.page(p);
        if (!pg) continue;
        const int n = api.FPDFPage_GetAnnotCount(pg);
        for (int i = 0; i < n; ++i) {
            FPDF_ANNOTATION a = api.FPDFPage_GetAnnot(pg, i);
            if (!a) continue;
            if (api.FPDFAnnot_GetSubtype(a) == FPDF_ANNOT_WIDGET) {
                Field f;
                f.page = p;
                f.type = api.FPDFAnnot_GetFormFieldType(form, a);
                f.name = getString([&](unsigned short* b, unsigned long l) { return api.FPDFAnnot_GetFormFieldName(form, a, b, l); });
                f.value = getString([&](unsigned short* b, unsigned long l) { return api.FPDFAnnot_GetFormFieldValue(form, a, b, l); });
                f.exportValue = getString([&](unsigned short* b, unsigned long l) { return api.FPDFAnnot_GetFormFieldExportValue(form, a, b, l); });
                f.checked = (f.type == FPDF_FORMFIELD_CHECKBOX || f.type == FPDF_FORMFIELD_RADIOBUTTON) && api.FPDFAnnot_IsChecked(form, a);
                if (f.type != FPDF_FORMFIELD_SIGNATURE && f.type != FPDF_FORMFIELD_PUSHBUTTON) out.push_back(f);
            }
            api.FPDFPage_CloseAnnot(a);
        }
    }
    return out;
}

std::vector<std::vector<std::wstring>> answers(Document& d) {
    std::vector<std::wstring> names, values;
    for (const Field& f : list(d)) {
        std::wstring v = f.value;
        if (f.type == FPDF_FORMFIELD_CHECKBOX || f.type == FPDF_FORMFIELD_RADIOBUTTON) {
            auto it = std::find(names.begin(), names.end(), f.name);
            const std::wstring on = f.exportValue.empty() ? L"Yes" : f.exportValue;
            if (it != names.end()) { if (f.checked) values[it - names.begin()] = on; continue; }
            v = f.checked ? on : L"";
        } else if (std::find(names.begin(), names.end(), f.name) != names.end()) continue;
        names.push_back(f.name);
        values.push_back(v);
    }
    if (names.empty()) return {};
    return {names, values};
}

int setValues(Document& d, const std::map<std::wstring, std::wstring>& values) {
    FPDF_FORMHANDLE form = d.form();
    if (!form) return 0;
    const auto& api = pdfium::api();
    int changed = 0;
    for (int p = 0; p < d.pageCount(); ++p) {
        FPDF_PAGE pg = d.page(p);
        if (!pg) continue;
        d.currentPageForForm = p;
        const int n = api.FPDFPage_GetAnnotCount(pg);
        for (int i = 0; i < n; ++i) {
            FPDF_ANNOTATION a = api.FPDFPage_GetAnnot(pg, i);
            if (!a) continue;
            if (api.FPDFAnnot_GetSubtype(a) == FPDF_ANNOT_WIDGET) {
                const std::wstring name = getString([&](unsigned short* b, unsigned long l) { return api.FPDFAnnot_GetFormFieldName(form, a, b, l); });
                auto it = values.find(name);
                const int type = api.FPDFAnnot_GetFormFieldType(form, a);
                FS_RECTF rc;
                if (it != values.end() && api.FPDFAnnot_GetRect(a, &rc)) {
                    const double cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
                    auto click = [&] { api.FORM_ForceToKillFocus(form); api.FORM_OnLButtonDown(form, pg, 0, cx, cy); api.FORM_OnLButtonUp(form, pg, 0, cx, cy); };
                    if (type == FPDF_FORMFIELD_TEXTFIELD || type == FPDF_FORMFIELD_COMBOBOX) {
                        click();
                        api.FORM_SelectAllText(form, pg);
                        std::vector<unsigned short> w(it->second.begin(), it->second.end());
                        w.push_back(0);
                        api.FORM_ReplaceSelection(form, pg, w.data());
                        api.FORM_ForceToKillFocus(form);
                        ++changed;
                    } else if (type == FPDF_FORMFIELD_LISTBOX) {
                        // pick the entry whose label matches; an empty value clears the choice
                        const int count = api.FPDFAnnot_GetOptionCount(form, a);
                        click();
                        for (int k = 0; k < count; ++k) {
                            const std::wstring label = getString([&](unsigned short* b, unsigned long l) { return api.FPDFAnnot_GetOptionLabel(form, a, k, b, l); });
                            const bool want = !it->second.empty() && label == it->second;
                            if (want != static_cast<bool>(api.FORM_IsIndexSelected(form, pg, k))) api.FORM_SetIndexSelected(form, pg, k, want);
                        }
                        api.FORM_ForceToKillFocus(form);
                        ++changed;
                    } else if (type == FPDF_FORMFIELD_CHECKBOX || type == FPDF_FORMFIELD_RADIOBUTTON) {
                        const bool checked = api.FPDFAnnot_IsChecked(form, a);
                        bool want;
                        if (type == FPDF_FORMFIELD_RADIOBUTTON) {
                            const std::wstring ex = getString([&](unsigned short* b, unsigned long l) { return api.FPDFAnnot_GetFormFieldExportValue(form, a, b, l); });
                            want = it->second == ex;
                        } else want = truthy(it->second);
                        if (want != checked && (want || type == FPDF_FORMFIELD_CHECKBOX)) { click(); ++changed; }
                    }
                }
            }
            api.FPDFPage_CloseAnnot(a);
        }
    }
    return changed;
}

void drawRadioLabels(Document& d, const std::vector<formcreate::Spec>& specs) {
    const auto& api = pdfium::api();
    std::set<int> changed;
    for (const auto& sp : specs)
        for (const auto& lb : formcreate::radioLabels(sp)) {
            FPDF_PAGE pg = d.page(lb.page);
            FPDF_PAGEOBJECT o = pg ? api.FPDFPageObj_NewTextObj(d.handle(), "Helvetica", static_cast<float>(lb.sizePt)) : nullptr;
            if (!o) continue;
            std::vector<unsigned short> w(lb.text.begin(), lb.text.end());
            w.push_back(0);
            if (!api.FPDFText_SetText(o, w.data())) { api.FPDFPageObj_Destroy(o); continue; }
            api.FPDFPageObj_SetFillColor(o, 0, 0, 0, 255);
            api.FPDFPageObj_Transform(o, 1, 0, 0, 1, lb.x, lb.y);
            api.FPDFPage_InsertObject(pg, o);
            changed.insert(lb.page);
        }
    for (int p : changed) { contentgen::generate(d.handle(), d.page(p)); d.resetText(p); }
}

int resetAll(Document& d) {
    std::map<std::wstring, std::wstring> empty;
    for (const Field& f : list(d)) if (f.type != FPDF_FORMFIELD_RADIOBUTTON) empty[f.name] = L"";
    return setValues(d, empty);
}

std::vector<std::vector<std::wstring>> parseCsv(const std::wstring& text) {
    std::vector<std::vector<std::wstring>> rows;
    // the separator is a comma or, when the first line has no comma, a semicolon or a tab
    size_t eol = text.find_first_of(L"\r\n");
    const std::wstring first = text.substr(0, eol);
    wchar_t sep = L',';
    if (first.find(L',') == std::wstring::npos) sep = first.find(L';') != std::wstring::npos ? L';' : (first.find(L'\t') != std::wstring::npos ? L'\t' : L',');
    std::vector<std::wstring> row;
    std::wstring cell;
    bool quoted = false, any = false;
    auto endRow = [&] { row.push_back(cell); cell.clear(); if (!(row.size() == 1 && row[0].empty())) rows.push_back(row); row.clear(); any = false; };
    for (size_t i = 0; i < text.size(); ++i) {
        wchar_t c = text[i];
        if (i == 0 && c == 0xFEFF) continue;
        if (quoted) {
            if (c == L'"') { if (i + 1 < text.size() && text[i + 1] == L'"') { cell += L'"'; ++i; } else quoted = false; }
            else cell += c;
        } else if (c == L'"') { quoted = true; any = true; }
        else if (c == sep) { row.push_back(cell); cell.clear(); any = true; }
        else if (c == L'\r' || c == L'\n') { if (c == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i; endRow(); }
        else { cell += c; any = true; }
    }
    if (any || !cell.empty() || !row.empty()) endRow();
    return rows;
}

std::vector<formcreate::Spec> detect(Document& d) {
    std::vector<formcreate::Spec> out;
    const auto& api = pdfium::api();
    std::set<std::wstring> used;
    for (const Field& f : list(d)) used.insert(f.name);
    auto unique = [&](std::wstring base) {
        for (auto& c : base) if (c == L'.' ) c = L' ';
        size_t a = base.find_first_not_of(L" :\t"), z = base.find_last_not_of(L" :\t");
        base = a == std::wstring::npos ? L"Field" : base.substr(a, z - a + 1);
        if (base.size() > 40) base = base.substr(base.size() - 40);
        std::wstring name = base;
        for (int k = 2; used.count(name); ++k) name = base + L" " + std::to_wstring(k);
        used.insert(name);
        return name;
    };
    for (int p = 0; p < d.pageCount(); ++p) {
        FPDF_PAGE pg = d.page(p);
        FPDF_TEXTPAGE tp = d.text(p);
        if (!pg || !tp) continue;
        const int n = api.FPDFText_CountChars(tp);
        std::vector<formcreate::Spec> pageSpecs;
        // 1. runs of underscores
        for (int i = 0; i < n;) {
            if (api.FPDFText_GetUnicode(tp, i) != '_') { ++i; continue; }
            int j = i;
            double l = 1e9, b = 1e9, r = -1e9, t = -1e9;
            while (j < n && api.FPDFText_GetUnicode(tp, j) == '_') {
                double cl, cr, cb, ct;
                if (api.FPDFText_GetCharBox(tp, j, &cl, &cr, &cb, &ct)) { l = std::min(l, cl); r = std::max(r, cr); b = std::min(b, cb); t = std::max(t, ct); }
                ++j;
            }
            if (j - i >= 5 && r > l) {
                // the label: the text before the run on the same line
                std::wstring label;
                for (int k = i - 1; k >= 0 && label.size() < 60; --k) {
                    unsigned c = api.FPDFText_GetUnicode(tp, k);
                    if (c == '\r' || c == '\n' || c == '_') break;
                    label.insert(label.begin(), static_cast<wchar_t>(c));
                }
                formcreate::Spec sp;
                sp.kind = formcreate::Spec::Text; sp.page = p;
                sp.left = l; sp.right = r; sp.bottom = b - 1; sp.top = std::max(t + 3, b + 14);
                sp.name = label;
                pageSpecs.push_back(sp);
            }
            i = j;
        }
        // 2. rules and small squares drawn as shapes
        const int objs = api.FPDFPage_CountObjects(pg);
        for (int o = 0; o < objs; ++o) {
            FPDF_PAGEOBJECT ob = api.FPDFPage_GetObject(pg, o);
            if (!ob || api.FPDFPageObj_GetType(ob) != FPDF_PAGEOBJ_PATH) continue;
            float l, b, r, t;
            if (!api.FPDFPageObj_GetBounds(ob, &l, &b, &r, &t)) {
                // a straight rule has no area, so PDFium gives no bounds: take them from the path itself
                const int segs = api.FPDFPath_CountSegments(ob);
                FS_MATRIX m{1, 0, 0, 1, 0, 0};
                api.FPDFPageObj_GetMatrix(ob, &m);
                float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
                for (int sIdx = 0; sIdx < segs; ++sIdx) {
                    FPDF_PATHSEGMENT seg = api.FPDFPath_GetPathSegment(ob, sIdx);
                    float px, py;
                    if (!seg || !api.FPDFPathSegment_GetPoint(seg, &px, &py)) continue;
                    const float qx = m.a * px + m.c * py + m.e, qy = m.b * px + m.d * py + m.f;
                    x0 = std::min(x0, qx); x1 = std::max(x1, qx); y0 = std::min(y0, qy); y1 = std::max(y1, qy);
                }
                if (x1 < x0) continue;
                l = x0; r = x1; b = y0; t = y1;
            }
            const double w = r - l, h = t - b;
            // text just left of / right of this shape
            auto textBeside = [&](bool left) {
                std::wstring s;
                const double x0 = left ? l - 150 : r, x1 = left ? l : r + 150;
                const int len = api.FPDFText_GetBoundedText(tp, x0, t + 14, x1, b - 3, nullptr, 0);
                if (len > 0) {
                    std::vector<unsigned short> buf(len + 1);
                    api.FPDFText_GetBoundedText(tp, x0, t + 14, x1, b - 3, buf.data(), len);
                    s.assign(buf.begin(), buf.begin() + std::min<size_t>(len, buf.size()));
                    s.resize(s.find(L'\0') == std::wstring::npos ? s.size() : s.find(L'\0'));
                    for (auto& c : s) if (c == L'\r' || c == L'\n') c = L' ';
                }
                return s;
            };
            if (h <= 3.2 && w >= 60 && w <= 400) {   // a rule: needs a label ending in a colon just before it
                std::wstring lab = textBeside(true);
                size_t z = lab.find_last_not_of(L' ');
                if (z != std::wstring::npos && lab[z] == L':') {
                    formcreate::Spec sp;
                    sp.kind = formcreate::Spec::Text; sp.page = p;
                    sp.left = l; sp.right = r; sp.bottom = b + 1; sp.top = b + 15;
                    sp.name = lab;
                    pageSpecs.push_back(sp);
                }
            } else if (w >= 6 && w <= 16 && h >= 6 && h <= 16 && std::fabs(w - h) <= 2) {   // a small square with text after it
                std::wstring lab = textBeside(false);
                if (lab.find_first_not_of(L' ') != std::wstring::npos) {
                    formcreate::Spec sp;
                    sp.kind = formcreate::Spec::Check; sp.page = p;
                    sp.left = l; sp.right = r; sp.bottom = b; sp.top = t;
                    sp.name = lab.substr(0, 40);
                    pageSpecs.push_back(sp);
                }
            }
        }
        // drop blanks that sit on top of an existing field or of each other
        for (auto& sp : pageSpecs) {
            bool overlap = false;
            for (const auto& o : out) {
                if (o.page != sp.page) continue;
                if (sp.left < o.right && sp.right > o.left && sp.bottom < o.top && sp.top > o.bottom) { overlap = true; break; }
            }
            if (overlap) continue;
            sp.name = unique(sp.name);
            out.push_back(sp);
        }
    }
    return out;
}

} // namespace formdata
