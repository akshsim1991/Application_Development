#include "sensitive.hpp"
#include "pdfium_api.hpp"
#include <algorithm>
#include <cwctype>

namespace sensitive {

namespace {

bool digit(wchar_t c) { return c >= L'0' && c <= L'9'; }
bool upper(wchar_t c) { return c >= L'A' && c <= L'Z'; }
bool asciiAlnum(wchar_t c) { return digit(c) || upper(c) || (c >= L'a' && c <= L'z'); }
bool wordChar(wchar_t c) { return c == L'_' || std::iswalnum(static_cast<wint_t>(c)); }
bool at(const std::wstring& s, size_t i, bool (*f)(wchar_t)) { return i < s.size() && f(s[i]); }

std::wstring lower(std::wstring s) { for (auto& c : s) c = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c))); return s; }

struct Finder {
    const std::wstring& t;
    unsigned kinds;
    std::vector<bool> taken;
    std::vector<Hit> hits;
    Finder(const std::wstring& text, unsigned k) : t(text), kinds(k), taken(text.size(), false) {}

    bool free(size_t a, size_t b) const { for (size_t i = a; i < b; ++i) if (taken[i]) return false; return true; }
    void add(Kind k, size_t a, size_t b) {
        if (!(kinds & k) || a >= b || !free(a, b)) return;
        for (size_t i = a; i < b; ++i) taken[i] = true;
        Hit h;
        h.kind = k;
        h.start = static_cast<int>(a);
        h.count = static_cast<int>(b - a);
        h.text = t.substr(a, b - a);
        hits.push_back(h);
    }
    bool boundaryBefore(size_t a) const { return a == 0 || !wordChar(t[a - 1]); }
    bool boundaryAfter(size_t b) const { return b >= t.size() || !wordChar(t[b]); }

    // name.surname+tag@mail.example.co.in
    void emails() {
        auto local = [](wchar_t c) { return asciiAlnum(c) || c == L'.' || c == L'_' || c == L'%' || c == L'+' || c == L'-'; };
        auto domain = [](wchar_t c) { return asciiAlnum(c) || c == L'.' || c == L'-'; };
        for (size_t at_ = t.find(L'@'); at_ != std::wstring::npos; at_ = t.find(L'@', at_ + 1)) {
            size_t a = at_, b = at_ + 1;
            while (a > 0 && local(t[a - 1])) --a;
            while (b < t.size() && domain(t[b])) ++b;
            while (a < at_ && (t[a] == L'.' || t[a] == L'-')) ++a;
            while (b > at_ + 1 && (t[b - 1] == L'.' || t[b - 1] == L'-')) --b;
            if (a == at_) continue;
            const std::wstring dom = t.substr(at_ + 1, b - at_ - 1);
            const size_t dot = dom.rfind(L'.');
            if (dot == std::wstring::npos || dot == 0 || dom.size() - dot - 1 < 2) continue;
            bool tld = true;
            for (size_t i = dot + 1; i < dom.size(); ++i) tld = tld && std::iswalpha(static_cast<wint_t>(dom[i]));
            if (tld) add(Email, a, b);
        }
    }

    // ABCPE1234F; IFSC: SBIN0001234 (with bank accounts)
    void codes() {
        for (size_t i = 0; i + 10 <= t.size(); ++i) {
            if (!boundaryBefore(i) || !upper(t[i])) continue;
            if (i + 10 <= t.size() && boundaryAfter(i + 10) && panValid(t.substr(i, 10))) { add(Pan, i, i + 10); continue; }
            if (i + 11 <= t.size() && boundaryAfter(i + 11)) {
                bool ifsc = upper(t[i]) && upper(t[i + 1]) && upper(t[i + 2]) && upper(t[i + 3]) && t[i + 4] == L'0';
                for (size_t j = i + 5; ifsc && j < i + 11; ++j) ifsc = upper(t[j]) || digit(t[j]);
                if (ifsc) add(Bank, i, i + 11);
            }
        }
    }

    bool bankWordsBefore(size_t a) const {
        const size_t from = a > 60 ? a - 60 : 0;
        const std::wstring before = lower(t.substr(from, a - from));
        for (const wchar_t* w : {L"account", L"a/c", L"acct", L"ac no", L"a/c no", L"bank"})
            if (before.find(w) != std::wstring::npos) return true;
        return false;
    }

    // Runs of digits, joined when one or two separators (space, hyphen, dot, brackets) stand between them:
    // 98450 12345, +91-98450-12345, (080) 2345 6789, 2345 6789 0123.
    void numbers() {
        struct Group { size_t a, b; };
        std::vector<Group> g;
        for (size_t i = 0; i < t.size();) {
            if (!digit(t[i])) { ++i; continue; }
            size_t j = i;
            while (j < t.size() && digit(t[j])) ++j;
            g.push_back({i, j});
            i = j;
        }
        auto joinable = [&](size_t endPrev, size_t startNext) {
            const size_t n = startNext - endPrev;
            if (n < 1 || n > 2) return false;
            for (size_t i = endPrev; i < startNext; ++i)
                if (t[i] != L' ' && t[i] != L'-' && t[i] != L'.' && t[i] != L'(' && t[i] != L')') return false;
            return !(n == 2 && t[endPrev] == t[endPrev + 1] && t[endPrev] != L')');   // "1 - 2" or "1..2" is not one number
        };
        for (size_t s = 0; s < g.size();) {
            size_t found = 0;
            for (size_t k = std::min<size_t>(5, g.size() - s); k >= 1 && !found; --k) {
                bool ok = true;
                for (size_t i = s + 1; i < s + k && ok; ++i) ok = joinable(g[i - 1].b, g[i].a);
                if (!ok) continue;
                size_t a = g[s].a, b = g[s + k - 1].b;
                bool plus = false;
                if (a > 0 && t[a - 1] == L'+') { --a; plus = true; }
                else if (a > 0 && t[a - 1] == L'(') --a;
                if (plus && a > 0 && t[a - 1] == L'(') --a;
                // not part of a word, a decimal number or a longer list of digits
                if (!boundaryBefore(a) || !boundaryAfter(b)) continue;
                if (a >= 2 && (t[a - 1] == L'.' || t[a - 1] == L',' || t[a - 1] == L'/') && digit(t[a - 2])) continue;
                if (b + 1 < t.size() && (t[b] == L'.' || t[b] == L',' || t[b] == L'/') && digit(t[b + 1])) continue;
                std::wstring d;
                std::vector<size_t> sizes;
                bool dashesOnly = true;
                for (size_t i = s; i < s + k; ++i) {
                    d += t.substr(g[i].a, g[i].b - g[i].a);
                    sizes.push_back(g[i].b - g[i].a);
                    if (i > s) for (size_t c = g[i - 1].b; c < g[i].a; ++c) dashesOnly = dashesOnly && t[c] == L'-';
                }
                Kind kind = Kind(0);
                const bool aadhaarShape = sizes == std::vector<size_t>{12} || sizes == std::vector<size_t>{4, 4, 4};
                if (!plus && d.size() == 12 && d[0] >= L'2' && aadhaarShape && verhoeffValid(d)) kind = Aadhaar;
                else if (!plus && d.size() >= 9 && d.size() <= 18 && (k == 1 || dashesOnly) && bankWordsBefore(a)) kind = Bank;
                else if ((plus && d.size() >= 10 && d.size() <= 13) ||
                         (d.size() == 10 && d[0] >= L'6') ||
                         (d.size() == 12 && d[0] == L'9' && d[1] == L'1' && d[2] >= L'6') ||
                         (d.size() == 11 && d[0] == L'0' && d[1] != L'0'))
                    kind = Phone;
                if (kind && (kinds & kind)) {
                    add(kind, a, b);
                    found = k;
                }
            }
            s += found ? found : 1;
        }
    }
};

} // namespace

const wchar_t* name(Kind k) {
    switch (k) {
        case Email: return L"E-mail address";
        case Phone: return L"Phone number";
        case Aadhaar: return L"Aadhaar number";
        case Pan: return L"PAN";
        case Bank: return L"Bank account or IFSC";
        default: return L"Sensitive information";
    }
}

const std::vector<Kind>& kinds() {
    static const std::vector<Kind> k{Email, Phone, Aadhaar, Pan, Bank};
    return k;
}

bool verhoeffValid(const std::wstring& digits) {
    static const int dTab[10][10] = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 2, 3, 4, 0, 6, 7, 8, 9, 5}, {2, 3, 4, 0, 1, 7, 8, 9, 5, 6},
                                     {3, 4, 0, 1, 2, 8, 9, 5, 6, 7}, {4, 0, 1, 2, 3, 9, 5, 6, 7, 8}, {5, 9, 8, 7, 6, 0, 4, 3, 2, 1},
                                     {6, 5, 9, 8, 7, 1, 0, 4, 3, 2}, {7, 6, 5, 9, 8, 2, 1, 0, 4, 3}, {8, 7, 6, 5, 9, 3, 2, 1, 0, 4},
                                     {9, 8, 7, 6, 5, 4, 3, 2, 1, 0}};
    static const int pTab[8][10] = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, {1, 5, 7, 6, 2, 8, 3, 0, 9, 4}, {5, 8, 0, 3, 7, 9, 6, 1, 4, 2},
                                    {8, 9, 1, 6, 0, 4, 3, 5, 2, 7}, {9, 4, 5, 3, 1, 2, 6, 8, 7, 0}, {4, 2, 8, 6, 5, 7, 3, 9, 0, 1},
                                    {2, 7, 9, 3, 8, 0, 6, 4, 1, 5}, {7, 0, 4, 6, 9, 1, 3, 2, 5, 8}};
    if (digits.empty()) return false;
    int c = 0;
    const size_t n = digits.size();
    for (size_t i = 0; i < n; ++i) {
        const wchar_t ch = digits[n - 1 - i];
        if (!digit(ch)) return false;
        c = dTab[c][pTab[i % 8][ch - L'0']];
    }
    return c == 0;
}

bool panValid(const std::wstring& s) {
    if (s.size() != 10) return false;
    for (int i = 0; i < 5; ++i) if (!upper(s[static_cast<size_t>(i)])) return false;
    for (int i = 5; i < 9; ++i) if (!digit(s[static_cast<size_t>(i)])) return false;
    if (!upper(s[9])) return false;
    // the fourth letter is the holder's type: Person, Company, HUF, Firm, AOP, Trust, BOI, Local authority, Juridical, Government
    return std::wstring(L"PCHFATBLJG").find(s[3]) != std::wstring::npos;
}

std::vector<Hit> scanText(const std::wstring& text, unsigned kinds) {
    Finder f(text, kinds);
    f.emails();      // first: an address can hold digits
    f.codes();
    f.numbers();
    std::sort(f.hits.begin(), f.hits.end(), [](const Hit& a, const Hit& b) { return a.start < b.start; });
    return f.hits;
}

std::vector<Hit> scan(Document& d, unsigned kinds) {
    std::vector<Hit> out;
    for (int p = 0; p < d.pageCount(); ++p) {
        const int n = d.charCount(p);
        if (n <= 0) continue;
        for (Hit h : scanText(d.textRange(p, 0, n), kinds)) {
            h.page = p;
            out.push_back(h);
        }
    }
    return out;
}

int mark(Document& d, const std::vector<Hit>& hits) {
    int added = 0;
    const auto& api = pdfium::api();
    for (const Hit& h : hits) {
        FPDF_TEXTPAGE tp = d.text(h.page);
        if (!tp) continue;
        const int n = api.FPDFText_CountRects(tp, h.start, h.count);
        for (int i = 0; i < n; ++i) {
            double l, t, r, b;
            if (api.FPDFText_GetRect(tp, i, &l, &t, &r, &b)) { d.redactions.push_back({h.page, l, b, r, t}); ++added; }
        }
    }
    return added;
}

std::wstring masked(const std::wstring& s) {
    std::wstring out = s;
    std::vector<size_t> idx;
    for (size_t i = 0; i < s.size(); ++i) if (asciiAlnum(s[i])) idx.push_back(i);
    if (idx.size() <= 4) return out;
    for (size_t k = 2; k + 2 < idx.size(); ++k) out[idx[k]] = L'•';
    return out;
}

} // namespace sensitive
