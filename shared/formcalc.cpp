#include "formcalc.hpp"
#include "attach.hpp"   // layers::decodeString
#include "formdata.hpp"
#include "pdfedit.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <functional>
#include <map>

namespace formcalc {
using namespace pdfedit;

const wchar_t* opName(Op o) {
    switch (o) {
        case Op::Product: return L"Product";
        case Op::Average: return L"Average";
        case Op::Min: return L"Smallest";
        case Op::Max: return L"Largest";
        default: return L"Sum";
    }
}

namespace {

const char* opCode(Op o) {
    switch (o) {
        case Op::Product: return "PRD";
        case Op::Average: return "AVG";
        case Op::Min: return "MIN";
        case Op::Max: return "MAX";
        default: return "SUM";
    }
}

struct Raw {
    std::string s;
    File f{s};
    std::map<int, std::string> dict;
    int root = -1;
    bool ok = false;
    struct F { int object; std::wstring name; };
    std::vector<F> fields;   // terminal and parent fields that have a name, with their full names

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
        root = refOf(f.trailer, "/Root");
        if (!dict.count(root)) return;
        const std::string acro = value(dict[root], "/AcroForm");
        std::function<void(const std::string&, const std::wstring&, int)> walk = [&](const std::string& arr, const std::wstring& parent, int depth) {
            if (depth > 20) return;
            for (int n : refs(arr)) {
                if (!dict.count(n)) continue;
                const std::string& o = dict[n];
                std::wstring t = layers::decodeString(trim(raw(o, "/T")));
                const std::wstring full = t.empty() ? parent : (parent.empty() ? t : parent + L"." + t);
                if (!t.empty()) fields.push_back({n, full});
                walk(value(o, "/Kids"), full, depth + 1);
            }
        };
        walk(value(acro, "/Fields"), L"", 0);
        ok = true;
    }
    static std::string trim(std::string v) {
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) v.erase(0, 1);
        while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back()))) v.pop_back();
        return v;
    }
    static std::string raw(const std::string& d0, const char* key) {
        size_t b, e;
        return valueSpan(d0, key, b, e) ? d0.substr(b, e - b) : std::string();
    }
    std::string value(const std::string& d0, const char* key) const {
        std::string v = raw(d0, key);
        if (!v.empty() && v.back() == 'R') {
            auto it = dict.find(atoi(v.c_str()));
            return it == dict.end() ? std::string() : it->second;
        }
        return v;
    }
    static std::vector<int> refs(const std::string& arr) {
        std::vector<int> out;
        size_t p = 0;
        while ((p = arr.find(" R", p)) != std::string::npos) {
            size_t q = p;
            while (q > 0 && std::isdigit(static_cast<unsigned char>(arr[q - 1]))) --q;
            size_t r = q;
            while (r > 0 && arr[r - 1] == ' ') --r;
            size_t n = r;
            while (n > 0 && std::isdigit(static_cast<unsigned char>(arr[n - 1]))) --n;
            if (n < r) out.push_back(atoi(arr.c_str() + n));
            p += 2;
        }
        return out;
    }
    // The script of one action of the field's /AA (C, K, F or V): text, or empty.
    std::wstring script(int field, const char* which) const {
        auto it = dict.find(field);
        if (it == dict.end()) return {};
        const std::string aa = value(it->second, "/AA");
        if (aa.empty()) return {};
        const std::string act = value(aa, which);
        if (act.empty()) return {};
        const std::string js = raw(act, "/JS");
        if (!js.empty() && js.back() == 'R') {
            auto jt = dict.find(atoi(js.c_str()));
            return jt == dict.end() ? std::wstring() : layers::decodeString(trim(jt->second));
        }
        return layers::decodeString(trim(js));
    }
    int objectOf(const std::wstring& name) const {
        for (const auto& fl : fields) if (fl.name == name) return fl.object;
        return -1;
    }
};

std::vector<std::wstring> quoted(const std::wstring& s, size_t from) {
    std::vector<std::wstring> out;
    for (size_t i = from; i < s.size(); ++i) {
        if (s[i] != L'"' && s[i] != L'\'') continue;
        const wchar_t q = s[i];
        std::wstring v;
        size_t j = i + 1;
        for (; j < s.size() && s[j] != q; ++j) { if (s[j] == L'\\' && j + 1 < s.size()) ++j; v += s[j]; }
        out.push_back(v);
        i = j;
    }
    return out;
}

std::vector<double> numbersIn(const std::wstring& s, size_t from) {
    std::vector<double> out;
    size_t i = s.find(L'(', from);
    if (i == std::wstring::npos) return out;
    const size_t e = s.find(L')', i);
    std::wstring args = s.substr(i + 1, e == std::wstring::npos ? std::wstring::npos : e - i - 1);
    size_t p = 0;
    while (p <= args.size()) {
        size_t c = args.find(L',', p);
        std::wstring a = args.substr(p, c == std::wstring::npos ? std::wstring::npos : c - p);
        while (!a.empty() && std::iswspace(static_cast<wint_t>(a.front()))) a.erase(0, 1);
        while (!a.empty() && std::iswspace(static_cast<wint_t>(a.back()))) a.pop_back();
        if (a == L"true") out.push_back(1);
        else if (a == L"false") out.push_back(0);
        else out.push_back(wcstod(a.c_str(), nullptr));
        if (c == std::wstring::npos) break;
        p = c + 1;
    }
    return out;
}

// "1,234.50", "₹ 99", "(12)" -> a number; false when it is not one.
bool number(const std::wstring& v, double& out) {
    std::wstring t;
    bool neg = false;
    for (wchar_t c : v) {
        if (std::iswdigit(static_cast<wint_t>(c)) || c == L'.' || c == L'-' || c == L'+' || c == L'e' || c == L'E') t += c;
        else if (c == L'(') neg = true;
        else if (c == L',' || c == L' ' || c == L')' || c == L'₹' || c == L'$' || c == L'€' || c == L'£' || c == L'%') continue;
        else if ((c == L'R' || c == L's') && t.empty()) continue;   // "Rs"
        else return false;
    }
    if (t.empty()) { out = 0; return true; }
    wchar_t* e = nullptr;
    out = wcstod(t.c_str(), &e);
    if (e == t.c_str() || *e) return false;
    if (neg) out = -out;
    return true;
}

std::wstring format(double v, int decimals) {
    wchar_t b[64];
    if (decimals >= 0) { swprintf(b, 64, L"%.*f", decimals, v); return b; }
    swprintf(b, 64, L"%.2f", v);
    std::wstring s = b;
    while (!s.empty() && s.back() == L'0') s.pop_back();
    if (!s.empty() && s.back() == L'.') s.pop_back();
    return s == L"-0" ? L"0" : s;
}

std::string action(const std::wstring& js) { return "<< /S /JavaScript /JS " + pdfString(js) + " >>"; }

// Rewrites the field's /AA with `set` (key -> action text, or "null" to remove) and the AcroForm's /CO; reloads.
bool write(Document& d, Raw& r, int field, const std::map<std::string, std::string>& set, int coAdd, int coRemove, const wchar_t* what) {
    std::map<int, std::string> out;
    std::string fd = r.dict[field];
    std::string aaRaw = Raw::raw(fd, "/AA");
    int aaObj = -1;
    std::string aa = "<< >>";
    if (!aaRaw.empty() && aaRaw.back() == 'R') { aaObj = atoi(aaRaw.c_str()); aa = r.dict[aaObj]; }
    else if (!aaRaw.empty()) aa = aaRaw;
    for (const auto& kv : set) aa = setKey(aa, kv.first.c_str(), kv.second);
    if (aaObj >= 0) out[aaObj] = aa;
    else out[field] = setKey(fd, "/AA", aa);
    if (coAdd >= 0 || coRemove >= 0) {
        std::string cat = r.dict[r.root];
        std::string acroRaw = Raw::raw(cat, "/AcroForm");
        int acroObj = -1;
        std::string acro = acroRaw;
        if (!acroRaw.empty() && acroRaw.back() == 'R') { acroObj = atoi(acroRaw.c_str()); acro = r.dict[acroObj]; }
        std::vector<int> co = Raw::refs(r.value(acro, "/CO"));
        co.erase(std::remove(co.begin(), co.end(), coRemove), co.end());
        if (coAdd >= 0 && std::find(co.begin(), co.end(), coAdd) == co.end()) co.push_back(coAdd);
        std::string arr = "[";
        for (int n : co) arr += std::to_string(n) + " 0 R ";
        arr += "]";
        acro = setKey(acro, "/CO", arr);
        if (acroObj >= 0) out[acroObj] = acro;
        else out[r.root] = setKey(cat, "/AcroForm", acro);
    }
    std::vector<unsigned char> bytes = appendIncremental(r.s, r.f, out, r.f.size);
    d.beginChange(what);
    if (d.reload(std::move(bytes)) != OpenResult::Ok) { d.rollbackChange(); return false; }
    return true;
}

} // namespace

std::vector<Calc> calculations(Document& d) {
    std::vector<Calc> out;
    Raw r(d);
    if (!r.ok) return out;
    // the calculation order first, then any other field with a calculation
    std::vector<int> order = Raw::refs(r.value(r.value(r.dict[r.root], "/AcroForm"), "/CO"));
    for (const auto& fl : r.fields) if (std::find(order.begin(), order.end(), fl.object) == order.end()) order.push_back(fl.object);
    for (int n : order) {
        const std::wstring js = r.script(n, "/C");
        const size_t at = js.find(L"AFSimple_Calculate");
        if (at == std::wstring::npos) continue;
        const auto q = quoted(js, at);
        if (q.empty()) continue;
        Calc c;
        for (const auto& fl : r.fields) if (fl.object == n) c.field = fl.name;
        if (c.field.empty()) continue;
        c.op = q[0] == L"PRD" ? Op::Product : q[0] == L"AVG" ? Op::Average : q[0] == L"MIN" ? Op::Min : q[0] == L"MAX" ? Op::Max : Op::Sum;
        c.sources.assign(q.begin() + 1, q.end());
        out.push_back(c);
    }
    return out;
}

std::vector<Rule> rules(Document& d) {
    std::vector<Rule> out;
    Raw r(d);
    if (!r.ok) return out;
    for (const auto& fl : r.fields) {
        const std::wstring k = r.script(fl.object, "/K"), v = r.script(fl.object, "/V");
        Rule rule;
        rule.field = fl.name;
        if (k.find(L"AFNumber_Keystroke") != std::wstring::npos) {
            rule.kind = Kind::Number;
            const auto n = numbersIn(k, k.find(L"AFNumber_Keystroke"));
            if (!n.empty()) rule.decimals = static_cast<int>(n[0]);
        }
        const size_t rg = v.find(L"AFRange_Validate");
        if (rg != std::wstring::npos) {
            const auto n = numbersIn(v, rg);
            if (n.size() >= 4) {
                rule.kind = Kind::Range;
                rule.hasMin = n[0] != 0; rule.min = n[1];
                rule.hasMax = n[2] != 0; rule.max = n[3];
            }
        }
        const size_t dt = k.find(L"AFDate_KeystrokeEx");
        if (dt != std::wstring::npos) {
            rule.kind = Kind::Date;
            const auto q = quoted(k, dt);
            if (!q.empty()) rule.dateFormat = q[0];
        }
        if (v.find(L"feather:email") != std::wstring::npos) rule.kind = Kind::Email;
        if (v.find(L"feather:phone") != std::wstring::npos) rule.kind = Kind::Phone;
        if (rule.kind != Kind::None) out.push_back(rule);
    }
    return out;
}

bool setCalculation(Document& d, const Calc& c) {
    Raw r(d);
    const int n = r.ok ? r.objectOf(c.field) : -1;
    if (n < 0) return false;
    if (c.sources.empty()) return write(d, r, n, {{"/C", "null"}}, -1, n, L"Remove a calculation");
    std::wstring js = L"AFSimple_Calculate(\"" + std::wstring(opCode(c.op), opCode(c.op) + 3) + L"\", new Array(";
    for (size_t i = 0; i < c.sources.size(); ++i) js += (i ? L", \"" : L"\"") + c.sources[i] + L"\"";
    js += L"));";
    return write(d, r, n, {{"/C", action(js)}}, n, -1, L"Calculate a field");
}

bool setRule(Document& d, const Rule& rule) {
    Raw r(d);
    const int n = r.ok ? r.objectOf(rule.field) : -1;
    if (n < 0) return false;
    std::map<std::string, std::string> set{{"/K", "null"}, {"/F", "null"}, {"/V", "null"}};
    const std::wstring dec = std::to_wstring(std::clamp(rule.decimals, 0, 6));
    switch (rule.kind) {
        case Kind::Number: case Kind::Range: {
            set["/K"] = action(L"AFNumber_Keystroke(" + dec + L", 0, 0, 0, \"\", true);");
            set["/F"] = action(L"AFNumber_Format(" + dec + L", 0, 0, 0, \"\", true);");
            if (rule.kind == Kind::Range)
                set["/V"] = action(L"AFRange_Validate(" + std::wstring(rule.hasMin ? L"true" : L"false") + L", " + format(rule.min, -1) + L", " +
                                   (rule.hasMax ? L"true" : L"false") + L", " + format(rule.max, -1) + L");");
            break;
        }
        case Kind::Date:
            set["/K"] = action(L"AFDate_KeystrokeEx(\"" + rule.dateFormat + L"\");");
            set["/F"] = action(L"AFDate_FormatEx(\"" + rule.dateFormat + L"\");");
            break;
        case Kind::Email:
            set["/V"] = action(L"/* feather:email */ if (event.value && !/^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$/.test(event.value)) { app.alert(\"Type an e-mail address, such as name@example.com.\"); event.rc = false; }");
            break;
        case Kind::Phone:
            set["/V"] = action(L"/* feather:phone */ if (event.value && !/^\\+?[0-9 ()-]{8,18}$/.test(event.value)) { app.alert(\"Type a phone number.\"); event.rc = false; }");
            break;
        default: break;
    }
    return write(d, r, n, set, -1, -1, rule.kind == Kind::None ? L"Remove a field check" : L"Check what a field accepts");
}

std::wstring check(const Rule& r, const std::wstring& value) {
    std::wstring v = value;
    while (!v.empty() && std::iswspace(static_cast<wint_t>(v.back()))) v.pop_back();
    while (!v.empty() && std::iswspace(static_cast<wint_t>(v.front()))) v.erase(0, 1);
    if (v.empty()) return {};
    double x = 0;
    switch (r.kind) {
        case Kind::Number:
            return number(v, x) ? std::wstring() : L"Type a number.";
        case Kind::Range: {
            if (!number(v, x)) return L"Type a number.";
            if ((r.hasMin && x < r.min) || (r.hasMax && x > r.max)) {
                if (r.hasMin && r.hasMax) return L"Type a number from " + format(r.min, -1) + L" to " + format(r.max, -1) + L".";
                return r.hasMin ? L"Type a number of at least " + format(r.min, -1) + L"." : L"Type a number of at most " + format(r.max, -1) + L".";
            }
            return {};
        }
        case Kind::Date: {
            // dd, mm, yyyy (or yy) in the order the format gives, separated as it says
            const std::wstring& fmt = r.dateFormat;
            int day = -1, month = -1, year = -1;
            size_t i = 0, j = 0;
            while (i < fmt.size()) {
                const wchar_t c = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(fmt[i])));
                if (c == L'd' || c == L'm' || c == L'y') {
                    size_t k = i;
                    while (k < fmt.size() && std::towlower(static_cast<wint_t>(fmt[k])) == c) ++k;
                    size_t e = j;
                    while (e < v.size() && std::iswdigit(static_cast<wint_t>(v[e])) && e - j < (c == L'y' ? 4u : 2u)) ++e;
                    if (e == j) return L"Type a date as " + fmt + L".";
                    const int n = static_cast<int>(wcstol(v.substr(j, e - j).c_str(), nullptr, 10));
                    (c == L'd' ? day : c == L'm' ? month : year) = n;
                    i = k;
                    j = e;
                } else {
                    if (j >= v.size() || v[j] != fmt[i]) return L"Type a date as " + fmt + L".";
                    ++i;
                    ++j;
                }
            }
            if (j != v.size()) return L"Type a date as " + fmt + L".";
            static const int days[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            if (month != -1 && (month < 1 || month > 12)) return L"There is no month " + std::to_wstring(month) + L".";
            if (day != -1 && (day < 1 || day > (month >= 1 ? days[month - 1] : 31))) return L"That day does not exist in that month.";
            return {};
        }
        case Kind::Email: {
            const size_t at = v.find(L'@');
            const size_t dot = v.rfind(L'.');
            const bool ok = at != std::wstring::npos && at > 0 && v.find(L'@', at + 1) == std::wstring::npos && dot != std::wstring::npos && dot > at + 1 &&
                            dot + 1 < v.size() && v.find(L' ') == std::wstring::npos;
            return ok ? std::wstring() : L"Type an e-mail address, such as name@example.com.";
        }
        case Kind::Phone: {
            int digits = 0;
            for (wchar_t c : v) {
                if (std::iswdigit(static_cast<wint_t>(c))) ++digits;
                else if (!(c == L'+' || c == L' ' || c == L'-' || c == L'(' || c == L')')) return L"Type a phone number (digits, spaces, + and -).";
            }
            return digits >= 8 && digits <= 15 ? std::wstring() : L"Type a phone number (8 to 15 digits).";
        }
        default: return {};
    }
}

int recalculate(Document& d) {
    const auto calcs = calculations(d);
    if (calcs.empty()) return 0;
    std::map<std::wstring, int> decimals;
    for (const auto& r : rules(d)) if (r.kind == Kind::Number || r.kind == Kind::Range) decimals[r.field] = r.decimals;
    int changed = 0;
    for (const auto& c : calcs) {   // in order: a total can use another total worked out before it
        std::map<std::wstring, std::wstring> now;
        for (const auto& f : formdata::list(d)) now[f.name] = f.value;
        std::vector<double> xs;
        for (const auto& s : c.sources) {
            double x = 0;
            auto it = now.find(s);
            if (it != now.end() && number(it->second, x)) xs.push_back(x);
            else xs.push_back(0);
        }
        double v = 0;
        if (!xs.empty()) {
            switch (c.op) {
                case Op::Product: v = 1; for (double x : xs) v *= x; break;
                case Op::Average: for (double x : xs) v += x; v /= static_cast<double>(xs.size()); break;
                case Op::Min: v = *std::min_element(xs.begin(), xs.end()); break;
                case Op::Max: v = *std::max_element(xs.begin(), xs.end()); break;
                default: for (double x : xs) v += x; break;
            }
        }
        auto dk = decimals.find(c.field);
        const std::wstring text = format(v, dk == decimals.end() ? -1 : dk->second);
        auto cur = now.find(c.field);
        if (cur != now.end() && cur->second == text) continue;
        changed += formdata::setValues(d, {{c.field, text}});
    }
    return changed;
}

std::vector<std::pair<std::wstring, std::wstring>> problems(Document& d) {
    std::vector<std::pair<std::wstring, std::wstring>> out;
    const auto rs = rules(d);
    if (rs.empty()) return out;
    std::map<std::wstring, std::wstring> now;
    for (const auto& f : formdata::list(d)) now[f.name] = f.value;
    for (const auto& r : rs) {
        auto it = now.find(r.field);
        if (it == now.end()) continue;
        const std::wstring msg = check(r, it->second);
        if (!msg.empty()) out.push_back({r.field, msg});
    }
    return out;
}

} // namespace formcalc
