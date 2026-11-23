#include "equation.hpp"
#include "mathfonts.hpp"
#include "pdfium_api.hpp"
#include "contentgen.hpp"
#include "tables.hpp"
#include "scriptchars.hpp"
#include "platform.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <cstdlib>
#include <map>
#include <set>
#include <random>

namespace pageobj {   // (as pageobjects.cpp reads them)
namespace {
wchar_t mathLetter(unsigned cp) {
    if (cp >= 0x1D400 && cp <= 0x1D6A3) { const unsigned i = (cp - 0x1D400) % 52; return static_cast<wchar_t>(i < 26 ? L'A' + i : L'a' + (i - 26)); }
    if (cp >= 0x1D6A8 && cp <= 0x1D7C9) {
        const unsigned i = (cp - 0x1D6A8) % 58;
        if (i < 25) return static_cast<wchar_t>(0x391 + i + (i >= 17 ? 1 : 0));
        if (i >= 26 && i < 51) return static_cast<wchar_t>(0x3B1 + (i - 26));
        return 0;
    }
    if (cp >= 0x1D7CE && cp <= 0x1D7FF) return static_cast<wchar_t>(L'0' + (cp - 0x1D7CE) % 10);
    return 0;
}
std::string baseFontName(FPDF_FONT font) {
    std::string name;
    size_t len = font ? pdfium::api().FPDFFont_GetBaseFontName(font, nullptr, 0) : 0;
    if (len > 1) { name.assign(len, '\0'); pdfium::api().FPDFFont_GetBaseFontName(font, name.data(), len); name.resize(std::strlen(name.c_str())); }
    return name;
}
} // namespace
} // namespace pageobj

namespace equation {
using featherscript::script::of;
namespace {

const char* const kFontNames[4] = {"Times-Roman", "Times-Italic", "Symbol", "Times-Bold"};
enum FontId { Roman = 0, Italic = 1, Sym = 2, Bold = 3 };
// A document made with TeX gets its equations in Latin Modern (TeX's own look): fonts 4 to 7 are the Latin Modern ones
// for the roles 0 to 3 (upright, italic, signs and Greek, bold). While an equation is laid out, `gTex` says which.
constexpr int kTex = 4;
thread_local bool gTex = false;
int role(int font) { return font & 3; }
bool isTex(int font) { return font >= kTex; }
int texFile(int font) { const int r = role(font); return r == Italic ? 1 : r == Bold ? 2 : 0; }   // mathfonts::kFonts

// What a TrueType font says of a character: its advance and its outline's box, in em (read from the font's own tables).
struct TtfInfo {
    const unsigned char* p = nullptr;
    size_t n = 0;
    unsigned em = 1000, hmetrics = 0, glyphs = 0;
    size_t cmap4 = 0, hmtx = 0, loca = 0, glyf = 0;
    bool longLoca = false;
    unsigned u16(size_t o) const { return o + 2 <= n ? (p[o] << 8) | p[o + 1] : 0; }
    int s16(size_t o) const { return static_cast<int16_t>(u16(o)); }
    unsigned long u32(size_t o) const { return o + 4 <= n ? (static_cast<unsigned long>(u16(o)) << 16) | u16(o + 2) : 0; }
    explicit TtfInfo(const mathfonts::Font& f) : p(f.data), n(f.size) {
        const unsigned tables = u16(4);
        size_t head = 0, hhea = 0, maxp = 0, cmap = 0;
        for (unsigned i = 0; i < tables; ++i) {
            const size_t r = 12 + 16 * i;
            const std::string tag(reinterpret_cast<const char*>(p + r), 4);
            const size_t off = u32(r + 8);
            if (tag == "head") head = off; else if (tag == "hhea") hhea = off; else if (tag == "maxp") maxp = off;
            else if (tag == "cmap") cmap = off; else if (tag == "hmtx") hmtx = off; else if (tag == "loca") loca = off;
            else if (tag == "glyf") glyf = off;
        }
        em = u16(head + 18); longLoca = s16(head + 50) != 0;
        hmetrics = u16(hhea + 34); glyphs = u16(maxp + 4);
        for (unsigned i = 0, k = u16(cmap + 2); i < k; ++i) {
            const size_t sub = cmap + u32(cmap + 4 + 8 * i + 4);
            if (u16(sub) == 4) { cmap4 = sub; break; }
        }
    }
    unsigned glyphOf(unsigned c) const {
        if (!cmap4 || c > 0xFFFF) return 0;
        const unsigned segs = u16(cmap4 + 6) / 2;
        const size_t ends = cmap4 + 14, starts = ends + 2 * segs + 2, deltas = starts + 2 * segs, ranges = deltas + 2 * segs;
        for (unsigned i = 0; i < segs; ++i) {
            if (c > u16(ends + 2 * i)) continue;
            if (c < u16(starts + 2 * i)) return 0;
            const unsigned ro = u16(ranges + 2 * i);
            if (!ro) return (c + u16(deltas + 2 * i)) & 0xFFFF;
            const unsigned g = u16(ranges + 2 * i + ro + 2 * (c - u16(starts + 2 * i)));
            return g ? (g + u16(deltas + 2 * i)) & 0xFFFF : 0;
        }
        return 0;
    }
    bool metrics(unsigned c, double& adv, double& l, double& b, double& r, double& t) const {
        const unsigned g = glyphOf(c);
        if (!g || g >= glyphs || !em) return false;
        adv = u16(hmtx + 4 * std::min(g, hmetrics - 1)) / static_cast<double>(em);
        const size_t a = longLoca ? u32(loca + 4 * g) : 2 * static_cast<size_t>(u16(loca + 2 * g));
        const size_t e = longLoca ? u32(loca + 4 * g + 4) : 2 * static_cast<size_t>(u16(loca + 2 * g + 2));
        if (e > a) { const size_t q = glyf + a; l = s16(q + 2) / double(em); b = s16(q + 4) / double(em); r = s16(q + 6) / double(em); t = s16(q + 8) / double(em); }
        else { l = 0; b = 0; r = adv; t = 0; }   // nothing drawn (a space)
        return true;
    }
};
constexpr int kTexFontKey = 0x7E40;   // Document::embeddedFont's keys for the three
// A font of TeX's (Computer Modern, Latin Modern, or this editor's Latin Modern): by its name in the PDF.
bool texFontName(const std::string& n) {
    for (const char* k : {"CMR", "CMMI", "CMSY", "CMEX", "CMBX", "CMTI", "CMSS", "LMRoman", "LMMath", "LatinModern", "FeatherLMMath", "lmr", "lmmi"})
        if (n.find(k) != std::string::npos) return true;
    return false;
}
const TtfInfo& texInfo(int font) {
    static const TtfInfo infos[3] = {TtfInfo(mathfonts::kFonts[0]), TtfInfo(mathfonts::kFonts[1]), TtfInfo(mathfonts::kFonts[2])};
    return infos[texFile(font)];
}

// ---- measuring, in the standard fonts the equation is set in ----
FPDF_DOCUMENT scratch() {
    static FPDF_DOCUMENT doc = pdfium::api().FPDF_CreateNewDocument();
    return doc;
}
FPDF_FONT stdFont(int f) {
    static FPDF_FONT fonts[4] = {};
    if (!fonts[f]) fonts[f] = pdfium::api().FPDFText_LoadStandardFont(scratch(), kFontNames[f]);
    return fonts[f];
}
struct Metrics {
    double adv = 0.5, l = 0, b = 0, r = 0.5, t = 0.7;   // per point of size; b and t the ink below and above the baseline
    bool drawn = true;                                   // the font has the character
};
const Metrics& metrics(int font, wchar_t c) {
    static std::map<std::pair<int, wchar_t>, Metrics> cache;
    const auto key = std::make_pair(font, c);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    const auto& api = pdfium::api();
    Metrics m;
    if (isTex(font)) {
        m.drawn = texInfo(font).metrics(static_cast<unsigned>(c), m.adv, m.l, m.b, m.r, m.t);
        if (!m.drawn) { m.adv = 0.5; m.l = 0; m.b = 0; m.r = 0.5; m.t = 0; }
        if (c == L' ') { m.drawn = true; m.adv = 0.333; }
        return cache.emplace(key, m).first->second;
    }
    float w = 0;
    if (FPDF_FONT f = stdFont(font); f && api.FPDFFont_GetGlyphWidth(f, static_cast<uint32_t>(c), 1000, &w)) m.adv = w / 1000.0;
    m.drawn = false;
    if (FPDF_PAGEOBJECT o = api.FPDFPageObj_NewTextObj(scratch(), kFontNames[font], 1000)) {
        const unsigned short txt[2] = {static_cast<unsigned short>(c), 0};
        float l = 0, b = 0, r = 0, t = 0;
        if (api.FPDFText_SetText(o, txt) && api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t) && t > b) {
            m.l = l / 1000.0; m.b = b / 1000.0; m.r = r / 1000.0; m.t = t / 1000.0;
            m.drawn = true;
        } else { m.l = 0; m.b = 0; m.r = m.adv; m.t = 0; }
        api.FPDFPageObj_Destroy(o);
    }
    if (c == L' ') m.drawn = true;
    // the Times fonts are written with WinAnsi: a character outside it is drawn as another one
    static const wchar_t kWinAnsi[] = L"\x20AC\x201A\x0192\x201E\x2026\x2020\x2021\x02C6\x2030\x0160\x2039\x0152\x017D\x2018\x2019\x201C\x201D\x2022\x2013\x2014\x02DC\x2122\x0161\x203A\x0153\x017E\x0178";
    if (font != Sym && c >= 0x100 && !std::wcschr(kWinAnsi, c)) m.drawn = false;
    return cache.emplace(key, m).first->second;
}
// The character a font draws for `c` (the Symbol font has its own code points for a few letters and signs).
wchar_t drawnAs(int font, wchar_t c) {
    if (font != Sym) return c;   // (Latin Modern has the letters at their own code points)
    switch (c) {
    case 0x0394: return 0x2206;   // Delta: the Symbol font's Delta is "increment"
    case 0x03A9: return 0x2126;   // Omega: "ohm"
    case 0x03BC: return 0x00B5;   // mu: "micro"
    default: return c;
    }
}
// The font a character is set in: letters in Times (italic when `italic`), what Times lacks in Symbol.
int fontForRole(wchar_t c, int prefer) {
    if (c < 0x80) return iswalpha(c) ? prefer : (prefer == Bold ? Bold : Roman);
    if (c == 0xB1 || c == 0xD7 || c == 0xB7 || c == 0xF7 || c == 0xB0 || c == 0x2026 || c == 0x2022) return prefer == Bold ? Bold : Roman;
    if (c >= 0xC0 && c <= 0x17F) return prefer;   // accented Latin letters
    if (gTex) return prefer == Italic && metrics(Italic + kTex, c).drawn ? Italic : Sym;   // (Latin Modern's upright font has the signs)
    if (metrics(Sym, drawnAs(Sym, c)).drawn) return Sym;
    return prefer;
}
// The font (0 to 7) a character is set in, in the face being laid out. In Latin Modern, a character its italic or bold
// font lacks (a sign in bold, an accented letter in italic) is set upright.
int fontFor(wchar_t c, int prefer) {
    const int r = fontForRole(c, prefer);
    if (!gTex) return r;
    if (!metrics(r + kTex, c).drawn && metrics(Roman + kTex, c).drawn) return Roman + kTex;
    return r + kTex;
}

// ---- the notation ----
enum Cls { Ord, Bin, Rel, Open, Close, Punct, Big };

struct Node {
    enum Kind { Row, Char, Script, Frac, Root, Space, Accent } kind = Row;
    std::wstring text;          // Char: the characters
    int font = Italic;          // Char: Italic for letters, Roman upright (digits and signs are upright anyway), Bold
    Cls cls = Ord;
    bool func = false;          // an upright function name (sin, log ...)
    bool limits = false;        // a big operator with its limits under and over it (sum, prod)
    std::vector<Node> kids;     // Row: items; Script: base, power, index; Frac: top, bottom; Root: body, index
    bool sup = false, sub = false, index = false;
    double em = 0;              // Space: its width in em
};

const wchar_t* const kFunctions[] = {L"sin", L"cos", L"tan", L"cot", L"sec", L"csc", L"sinh", L"cosh", L"tanh", L"coth", L"arcsin",
                                     L"arccos", L"arctan", L"log", L"ln", L"lg", L"exp", L"lim", L"max", L"min", L"sup", L"inf",
                                     L"det", L"dim", L"ker", L"deg", L"gcd", L"arg", L"Re", L"Im", L"mod"};
bool isFunction(const std::wstring& w) {
    for (const wchar_t* f : kFunctions) if (w == f) return true;
    return false;
}

struct Named { const wchar_t* name; wchar_t ch; };
const Named kSymbols[] = {
    {L"alpha", 0x3B1}, {L"beta", 0x3B2}, {L"gamma", 0x3B3}, {L"delta", 0x3B4}, {L"epsilon", 0x3B5}, {L"varepsilon", 0x3B5},
    {L"zeta", 0x3B6}, {L"eta", 0x3B7}, {L"theta", 0x3B8}, {L"vartheta", 0x3D1}, {L"iota", 0x3B9}, {L"kappa", 0x3BA},
    {L"lambda", 0x3BB}, {L"mu", 0x3BC}, {L"nu", 0x3BD}, {L"xi", 0x3BE}, {L"omicron", 0x3BF}, {L"pi", 0x3C0}, {L"varpi", 0x3D6},
    {L"rho", 0x3C1}, {L"sigma", 0x3C3}, {L"varsigma", 0x3C2}, {L"tau", 0x3C4}, {L"upsilon", 0x3C5}, {L"phi", 0x3C6},
    {L"varphi", 0x3D5}, {L"chi", 0x3C7}, {L"psi", 0x3C8}, {L"omega", 0x3C9}, {L"Gamma", 0x393}, {L"Delta", 0x394},
    {L"Theta", 0x398}, {L"Lambda", 0x39B}, {L"Xi", 0x39E}, {L"Pi", 0x3A0}, {L"Sigma", 0x3A3}, {L"Upsilon", 0x3A5},
    {L"Phi", 0x3A6}, {L"Psi", 0x3A8}, {L"Omega", 0x3A9},
    {L"pm", 0xB1}, {L"times", 0xD7}, {L"cdot", 0xB7}, {L"div", 0xF7}, {L"ast", 0x2217}, {L"circ", 0x2218}, {L"bullet", 0x2022},
    {L"cup", 0x222A}, {L"cap", 0x2229}, {L"oplus", 0x2295}, {L"otimes", 0x2297}, {L"wedge", 0x2227}, {L"land", 0x2227},
    {L"vee", 0x2228}, {L"lor", 0x2228},
    {L"le", 0x2264}, {L"leq", 0x2264}, {L"ge", 0x2265}, {L"geq", 0x2265}, {L"ne", 0x2260}, {L"neq", 0x2260},
    {L"approx", 0x2248}, {L"equiv", 0x2261}, {L"sim", 0x223C}, {L"cong", 0x2245}, {L"propto", 0x221D},
    {L"to", 0x2192}, {L"rightarrow", 0x2192}, {L"leftarrow", 0x2190}, {L"gets", 0x2190}, {L"leftrightarrow", 0x2194},
    {L"Rightarrow", 0x21D2}, {L"implies", 0x21D2}, {L"Leftarrow", 0x21D0}, {L"Leftrightarrow", 0x21D4}, {L"iff", 0x21D4},
    {L"uparrow", 0x2191}, {L"downarrow", 0x2193},
    {L"in", 0x2208}, {L"notin", 0x2209}, {L"subset", 0x2282}, {L"supset", 0x2283}, {L"subseteq", 0x2286},
    {L"supseteq", 0x2287}, {L"perp", 0x22A5}, {L"mid", L'|'},
    {L"infty", 0x221E}, {L"partial", 0x2202}, {L"nabla", 0x2207}, {L"forall", 0x2200}, {L"exists", 0x2203},
    {L"emptyset", 0x2205}, {L"varnothing", 0x2205}, {L"angle", 0x2220}, {L"degree", 0xB0}, {L"prime", 0x2032},
    {L"ldots", 0x2026}, {L"dots", 0x2026}, {L"neg", 0xAC}, {L"lnot", 0xAC}, {L"aleph", 0x2135}, {L"wp", 0x2118},
    {L"langle", 0x2329}, {L"rangle", 0x232A}, {L"lfloor", 0x23A3}, {L"rfloor", 0x23A6}, {L"lceil", 0x23A1}, {L"rceil", 0x23A4},
    {L"therefore", 0x2234}, {L"surd", 0x221A}, {L"vert", L'|'},
    {L"sum", 0x2211}, {L"prod", 0x220F}, {L"int", 0x222B}, {L"oint", 0x222E},
};

Cls classOf(wchar_t c) {
    switch (c) {
    case L'+': case 0x2212: case 0xB1: case 0x2213: case 0xD7: case 0xB7: case 0x22C5: case 0xF7: case 0x2217: case 0x2218:
    case 0x2022: case 0x222A: case 0x2229: case 0x2295: case 0x2297: case 0x2227: case 0x2228:
        return Bin;
    case L'=': case L'<': case L'>': case 0x2264: case 0x2265: case 0x2260: case 0x2248: case 0x2261: case 0x223C: case 0x2245:
    case 0x221D: case 0x2192: case 0x2190: case 0x2194: case 0x21D2: case 0x21D0: case 0x21D4: case 0x2191: case 0x2193:
    case 0x2208: case 0x2209: case 0x2282: case 0x2283: case 0x2286: case 0x2287: case 0x22A5: case L':':
        return Rel;
    case L'(': case L'[': case L'{': case 0x2329: case 0x23A1: case 0x23A3:
        return Open;
    case L')': case L']': case L'}': case 0x232A: case 0x23A4: case 0x23A6: case L'!':
        return Close;
    case L',': case L';':
        return Punct;
    case 0x2211: case 0x220F: case 0x222B: case 0x222E:
        return Big;
    default:
        return Ord;
    }
}

Node charNode(wchar_t c) {
    Node n;
    n.kind = Node::Char;
    if (c == L'-') c = 0x2212;            // a hyphen is a minus sign
    else if (c == L'*') c = 0x2217;
    else if (c == L'\'') c = 0x2032;      // f' : a prime
    else if (c == 0x22C5) c = 0xB7;       // the dot operator: Times has the middle dot
    n.text = std::wstring(1, c);
    n.cls = classOf(c);
    n.limits = c == 0x2211 || c == 0x220F;
    n.font = (c < 0x80 && iswalpha(c)) || (c >= 0x3B1 && c <= 0x3C9) || c == 0x3D1 || c == 0x3D5 || c == 0x3D6 ? Italic : Roman;
    if (c >= 0xC0 && c <= 0x17F && iswalpha(c)) n.font = Italic;
    return n;
}

struct Parser {
    const std::wstring& s;
    size_t i = 0;
    std::wstring err;
    int depth = 0;
    explicit Parser(const std::wstring& src) : s(src) {}

    bool fail(const std::wstring& m) { if (err.empty()) err = m; return false; }
    void skip() { while (i < s.size() && iswspace(s[i])) ++i; }
    static bool scriptChar(wchar_t c) { wchar_t b; return of(c, b) != 0; }

    bool row(Node& out, wchar_t until) {
        if (++depth > 60) return fail(L"The equation is nested too deeply.");
        out.kind = Node::Row;
        for (;;) {
            skip();
            if (i >= s.size()) {
                --depth;
                if (until == L'}') return fail(L"A { has no } to close it.");
                if (until == L']') return fail(L"A [ has no ] to close it.");
                return true;
            }
            const wchar_t c = s[i];
            if (until && c == until) { ++i; --depth; return true; }
            if (c == L'}') return fail(L"There is a } with no { before it.");
            if (c == L'^' || c == L'_' || scriptChar(c)) {   // a power or index with nothing before it
                if (out.kids.empty()) { Node e; e.kind = Node::Char; out.kids.push_back(e); }
                if (!attach(out.kids.back())) return false;
                continue;
            }
            Node n;
            if (!atom(n)) return false;
            out.kids.push_back(std::move(n));
            for (;;) {   // its powers and indices
                const size_t save = i;
                skip();
                if (i < s.size() && (s[i] == L'^' || s[i] == L'_' || scriptChar(s[i]))) { if (!attach(out.kids.back())) return false; }
                else { i = save; break; }
            }
        }
    }

    void makeScript(Node& base) {
        if (base.kind == Node::Script) return;
        Node b = std::move(base);
        base = Node{};
        base.kind = Node::Script;
        base.kids.resize(3);
        base.kids[0] = std::move(b);
        base.kids[1].kind = Node::Row;
        base.kids[2].kind = Node::Row;
    }

    bool attach(Node& base) {
        const wchar_t c = s[i];
        makeScript(base);
        if (c == L'^' || c == L'_') {
            ++i;
            Node arg;
            if (!argument(arg, true)) return false;
            const int k = c == L'^' ? 1 : 2;
            bool& has = k == 1 ? base.sup : base.sub;
            if (has) return fail(k == 1 ? L"Two powers in a row (x^2^3): put the second one in braces, x^{2^3}." : L"Two indices in a row (x_1_2): put them in braces, x_{12}.");
            base.kids[static_cast<size_t>(k)] = std::move(arg);
            has = true;
            return true;
        }
        // superscript or subscript characters typed as they are (s⁷, x₁)
        wchar_t b;
        const int kind = of(c, b);
        Node& into = base.kids[kind > 0 ? 1 : 2];
        (kind > 0 ? base.sup : base.sub) = true;
        while (i < s.size()) {
            wchar_t p;
            if (of(s[i], p) != kind) break;
            into.kids.push_back(charNode(p));
            ++i;
        }
        return true;
    }

    // What follows ^ or _ (or a command's argument): {...}, a command, a number (with its sign), or one character.
    bool argument(Node& out, bool number) {
        skip();
        if (i >= s.size()) return fail(L"Something must follow ^, _ or a command such as \\frac.");
        const wchar_t c = s[i];
        if (c == L'{') { ++i; return row(out, L'}'); }
        out.kind = Node::Row;
        if (c == L'\\') { Node n; if (!command(n)) return false; out.kids.push_back(std::move(n)); return true; }
        if (number) {
            size_t j = i;
            if (s[j] == L'-' || s[j] == L'+' || s[j] == 0x2212) ++j;
            if (j < s.size() && iswdigit(s[j])) {
                if (j > i) out.kids.push_back(charNode(s[i]));
                i = j;
                while (i < s.size() && (iswdigit(s[i]) || (s[i] == L'.' && i + 1 < s.size() && iswdigit(s[i + 1])))) out.kids.push_back(charNode(s[i++]));
                return true;
            }
        }
        if (c == L'}' || c == L'^' || c == L'_') return fail(L"Something must follow ^, _ or a command such as \\frac.");
        Node n;
        if (!atom(n)) return false;
        out.kids.push_back(std::move(n));
        return true;
    }

    bool atom(Node& out) {
        const wchar_t c = s[i];
        if (c == L'{') { ++i; return row(out, L'}'); }
        if (c == L'\\') return command(out);
        if (c < 0x80 && iswalpha(c)) {   // a function's name on its own (sin x) is upright
            size_t j = i;
            while (j < s.size() && s[j] < 0x80 && iswalpha(s[j])) ++j;
            const std::wstring run = s.substr(i, j - i);
            if (run.size() > 1 && isFunction(run)) {
                out = Node{};
                out.kind = Node::Char; out.text = run; out.font = Roman; out.func = true;
                out.limits = run == L"lim" || run == L"max" || run == L"min";
                i = j;
                return true;
            }
        }
        out = charNode(c);
        ++i;
        return true;
    }

    bool command(Node& out) {
        ++i;   // the backslash
        if (i >= s.size()) return fail(L"A \\ must be followed by a command, such as \\frac or \\alpha.");
        const wchar_t c = s[i];
        if (!iswalpha(c) || c >= 0x80) {
            ++i;
            out = Node{};
            switch (c) {
            case L',': out.kind = Node::Space; out.em = 0.17; return true;
            case L':': case L'>': out.kind = Node::Space; out.em = 0.22; return true;
            case L';': out.kind = Node::Space; out.em = 0.28; return true;
            case L'!': out.kind = Node::Space; out.em = -0.17; return true;
            case L' ': out.kind = Node::Space; out.em = 0.25; return true;
            case L'|': out = charNode(0x2225); return true;
            default: out = charNode(c); return true;   // \{ \} \\ \^ \_ \% \$ \# \&: the character
            }
        }
        size_t j = i;
        while (j < s.size() && s[j] < 0x80 && iswalpha(s[j])) ++j;
        const std::wstring name = s.substr(i, j - i);
        i = j;
        if (name == L"frac" || name == L"dfrac" || name == L"tfrac" || name == L"over") {
            out = Node{};
            out.kind = Node::Frac;
            out.kids.resize(2);
            return argument(out.kids[0], false) && argument(out.kids[1], false);
        }
        if (name == L"bar" || name == L"overline" || name == L"hat" || name == L"widehat" || name == L"tilde" || name == L"widetilde" ||
            name == L"dot" || name == L"ddot" || name == L"vec") {
            out = Node{};
            out.kind = Node::Accent;
            out.text = name == L"bar" || name == L"overline" ? L"\x00AF" : name == L"hat" || name == L"widehat" ? L"\x02C6"
                     : name == L"tilde" || name == L"widetilde" ? L"\x02DC" : name == L"dot" ? L"\x02D9" : name == L"ddot" ? L"\x00A8" : L"\x2192";
            out.kids.resize(1);
            return argument(out.kids[0], false);
        }
        if (name == L"sqrt") {
            out = Node{};
            out.kind = Node::Root;
            out.kids.resize(2);
            skip();
            if (i < s.size() && s[i] == L'[') { ++i; if (!row(out.kids[1], L']')) return false; out.index = true; }
            return argument(out.kids[0], false);
        }
        if (name == L"text" || name == L"mathrm" || name == L"textrm" || name == L"operatorname" || name == L"mbox" || name == L"textbf" || name == L"mathbf") {
            const bool bold = name == L"textbf" || name == L"mathbf";
            if (name == L"mathbf") {   // bold mathematics: the letters and digits of its argument in bold
                if (!argument(out, false)) return false;
                bolden(out);
                return true;
            }
            skip();
            if (i >= s.size() || s[i] != L'{') return fail(L"\\" + name + L" needs its text in braces: \\" + name + L"{...}.");
            ++i;
            std::wstring t;
            int level = 1;
            while (i < s.size()) {
                if (s[i] == L'\\' && i + 1 < s.size()) { t += s[i + 1]; i += 2; continue; }
                if (s[i] == L'{') ++level;
                if (s[i] == L'}' && --level == 0) break;
                t += s[i++];
            }
            if (i >= s.size()) return fail(L"A { has no } to close it.");
            ++i;
            out = Node{};
            out.kind = Node::Char; out.text = t; out.font = bold ? Bold : Roman;
            out.func = name == L"operatorname";
            return true;
        }
        if (name == L"left" || name == L"right" || name == L"bigl" || name == L"bigr" || name == L"Bigl" || name == L"Bigr" ||
            name == L"big" || name == L"Big" || name == L"bigg" || name == L"Bigg" || name == L"displaystyle" || name == L"limits") {
            // brackets are sized to what they hold anyway
            skip();
            if ((name == L"left" || name == L"right") && i < s.size() && s[i] == L'.') { ++i; out = Node{}; out.kind = Node::Space; return true; }
            if (i < s.size() && (name == L"displaystyle" || name == L"limits")) { out = Node{}; out.kind = Node::Space; return true; }
            if (i >= s.size()) return fail(L"\\" + name + L" must be followed by a bracket.");
            if (s[i] == L'\\') return command(out);
            out = charNode(s[i++]);
            return true;
        }
        if (name == L"quad") { out = Node{}; out.kind = Node::Space; out.em = 1; return true; }
        if (name == L"qquad") { out = Node{}; out.kind = Node::Space; out.em = 2; return true; }
        if (name == L"cdots") { out = Node{}; out.kind = Node::Char; out.text = L"\x00B7\x00B7\x00B7"; out.font = Roman; return true; }
        if (isFunction(name)) {
            out = Node{};
            out.kind = Node::Char; out.text = name; out.font = Roman; out.func = true;
            out.limits = name == L"lim" || name == L"max" || name == L"min";
            return true;
        }
        for (const Named& n : kSymbols)
            if (name == n.name) { out = charNode(n.ch); return true; }
        return fail(L"\\" + name + L" is not a command this editor knows. Use the buttons, or type the symbol itself.");
    }

    static void bolden(Node& n) {
        if (n.kind == Node::Char && n.font != Roman) n.font = Bold;
        else if (n.kind == Node::Char && !n.text.empty() && n.text[0] < 0x80 && iswalnum(n.text[0])) n.font = Bold;
        for (Node& k : n.kids) bolden(k);
    }
};

// Letters of the mathematical alphabets (𝑠, 𝐺), Planck's ℎ and surrogate pairs (16-bit wchar_t) read as plain letters.
std::wstring normalise(const std::wstring& in) {
    std::wstring out;
    for (size_t k = 0; k < in.size(); ++k) {
        unsigned cp = static_cast<unsigned>(in[k]);
        if (cp >= 0xD800 && cp <= 0xDBFF && k + 1 < in.size()) {
            const unsigned lo = static_cast<unsigned>(in[k + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); ++k; }
        }
        if (cp == 0x210E) { out += L'h'; continue; }
        if (cp == 0x25E6) { out += static_cast<wchar_t>(0xB0); continue; }   // TeX's degree (^\circ)
        if (cp > 0xFFFF) { if (const wchar_t m = pageobj::mathLetter(cp)) out += m; continue; }
        if (cp == 0x2061 || cp == 0x2062 || cp == 0x2063 || cp == 0x200B) continue;   // invisible function application, times, comma
        out += static_cast<wchar_t>(cp);
    }
    return out;
}

// ---- setting it ----
struct Box {
    double w = 0, asc = 0, desc = 0;
    std::vector<Glyph> glyphs;
    std::vector<Stroke> strokes;
    Cls first = Ord, last = Ord;
    bool italicEnd = false;
    bool func = false, big = false, limits = false;
    void put(const Box& b, double dx, double dy) {
        for (Glyph g : b.glyphs) { g.x += dx; g.y += dy; glyphs.push_back(g); }
        for (Stroke s : b.strokes) { for (auto& p : s.points) { p.first += dx; p.second += dy; } strokes.push_back(s); }
        asc = std::max(asc, b.asc + dy);
        desc = std::max(desc, b.desc - dy);
    }
};

double axisOf(double size) {
    const Metrics& m = metrics(gTex ? Roman + kTex : Roman, L'+');
    return (m.b + m.t) / 2 * size;
}
double rule(double size) { return std::max(0.045 * size, 0.4); }

Box setNode(const Node& n, double size, int level);

Box setChar(const Node& n, double size) {
    Box b;
    if (n.text.empty()) return b;
    double scale = 1;
    if (n.cls == Big) scale = n.text[0] == 0x222B || n.text[0] == 0x222E ? 1.6 : 1.35;
    const double sz = size * scale;
    double x = 0;
    double top = -1e9, bottom = 1e9;
    int lastFont = -1;
    std::vector<double> rings;   // ∮: an integral sign with a ring round its middle (the Symbol font has no ∮)
    for (wchar_t c0 : n.text) {
        wchar_t c = c0;
        if (c == 0x222E) { c = 0x222B; rings.push_back(x); }
        const int f = fontFor(c, n.font);
        const wchar_t d = drawnAs(f, c);
        const Metrics& m = metrics(f, d);
        // lower-case Greek is italic in mathematics (as TeX sets it); the Symbol font's is upright, so it leans (Latin
        // Modern's italic font has italic Greek)
        const bool slant = f == Sym && n.font == Italic && ((c >= 0x3B1 && c <= 0x3C9) || c == 0x3D1 || c == 0x3D5 || c == 0x3D6 || c == 0x3F5);
        if (lastFont == f && !b.glyphs.empty() && b.glyphs.back().slant == slant) { b.glyphs.back().text += d; b.glyphs.back().advance += m.adv * sz; }
        else { Glyph g; g.text = std::wstring(1, d); g.font = f; g.x = x; g.y = 0; g.size = sz; g.advance = m.adv * sz; g.slant = slant; b.glyphs.push_back(g); }
        lastFont = f;
        if (m.t > m.b) { top = std::max(top, m.t * sz); bottom = std::min(bottom, m.b * sz); }
        x += m.adv * sz;
    }
    b.w = x;
    if (top < bottom) { top = 0; bottom = 0; }
    if (n.cls == Big) {   // centred on the axis, as a big operator is
        const double dy = axisOf(size) - (top + bottom) / 2;
        for (Glyph& g : b.glyphs) g.y += dy;
        top += dy; bottom += dy;
    }
    for (double rx : rings) {
        const Metrics& m = metrics(gTex ? Sym + kTex : Sym, 0x222B);
        const double cx = rx + (m.l + m.r) / 2 * sz, cy = n.cls == Big ? axisOf(size) : (m.b + m.t) / 2 * sz, rr = 0.16 * sz;
        Stroke ring;
        for (int k = 0; k <= 24; ++k) { const double a = k * 3.14159265358979 / 12; ring.points.push_back({cx + rr * std::cos(a), cy + rr * 1.15 * std::sin(a)}); }
        ring.width = rule(size) * 0.8;
        b.strokes.push_back(ring);
    }
    b.asc = std::max(0.0, top);
    b.desc = std::max(0.0, -bottom);
    b.first = b.last = n.cls;
    b.italicEnd = lastFont >= 0 && role(lastFont) == Italic;
    b.func = n.func;
    b.big = n.cls == Big;
    b.limits = n.limits;
    return b;
}

Box setScript(const Node& n, double size, int level, const Box* madeBase = nullptr) {
    Box base = madeBase ? *madeBase : setNode(n.kids[0], size, level);
    const double ss = std::max(size * 0.71, 4.0);
    Box sup, sub;
    // (level 100 and up: inside a power or an index, where signs get less space)
    if (n.sup) sup = setNode(n.kids[1], ss, std::max(level + 1, 100));
    if (n.sub) sub = setNode(n.kids[2], ss, std::max(level + 1, 100));
    Box b;
    b.first = base.first; b.last = base.last; b.func = base.func; b.big = base.big;
    if (base.limits) {   // limits under and over: sum_{i=1}^{n}, lim_{x \to 0}
        const double w = std::max({base.w, sup.w, sub.w});
        b.put(base, (w - base.w) / 2, 0);
        if (n.sup) b.put(sup, (w - sup.w) / 2, base.asc + 0.12 * size + sup.desc);
        if (n.sub) b.put(sub, (w - sub.w) / 2, -(base.desc + 0.12 * size + sub.asc));
        b.w = w;
        return b;
    }
    b.put(base, 0, 0);
    const double xh = 0.45 * size;
    double u = 0, v = 0;
    if (n.sup) u = std::max({0.37 * size, base.asc - 0.386 * ss, sup.desc + 0.25 * xh});
    if (n.sub) v = std::max({0.15 * size, base.desc + 0.05 * ss, sub.asc - 0.8 * xh});
    if (n.sup && n.sub) {
        v = std::max(v, 0.247 * size);
        const double gap = (u - sup.desc) - (sub.asc - v);
        if (gap < 0.16 * size) v += 0.16 * size - gap;
    }
    const double kern = base.italicEnd ? 0.06 * size : 0.02 * size;
    double w = base.w;
    if (n.sup) { b.put(sup, base.w + kern, u); w = std::max(w, base.w + kern + sup.w); }
    if (n.sub) { b.put(sub, base.w + (base.big ? -0.1 * size : 0.01 * size), -v); w = std::max(w, base.w + sub.w); }
    b.w = w + 0.03 * size;
    return b;
}

Box setFrac(const Node& n, double size, int level) {
    const double fs = level == 0 ? size : std::max(size * 0.85, 4.5);
    const Box top = setNode(n.kids[0], fs, level + 1), bottom = setNode(n.kids[1], fs, level + 1);
    const double axis = axisOf(size), th = rule(size), gap = 0.14 * size, pad = 0.1 * size, side = 0.08 * size;
    const double W = std::max(top.w, bottom.w) + 2 * pad;
    Box b;
    b.put(top, side + (W - top.w) / 2, axis + th / 2 + gap + top.desc);
    b.put(bottom, side + (W - bottom.w) / 2, axis - th / 2 - gap - bottom.asc);
    Stroke bar;
    bar.points = {{side, axis}, {side + W, axis}};
    bar.width = th;
    b.strokes.push_back(bar);
    b.asc = std::max(b.asc, axis + th / 2);
    b.w = W + 2 * side;
    return b;
}

Box setRoot(const Node& n, double size, int level) {
    const Box body = setNode(n.kids[0], size, level);
    const double th = rule(size);
    const double top = std::max(body.asc, 0.66 * size) + 0.14 * size + th / 2;
    const double bot = -std::max(body.desc, 0.0) - 0.08 * size;
    const double h = top - bot;
    const double sw = 0.38 * size + 0.08 * h;
    Box idx;
    double sx = 0.03 * size;
    if (n.index) {
        idx = setNode(n.kids[1], std::max(size * 0.55, 4.0), std::max(level + 1, 100));
        sx = std::max(sx, idx.w - 0.45 * sw);
    }
    Box b;
    if (n.index) b.put(idx, sx + 0.45 * sw - idx.w, bot + 0.5 * h + idx.desc);
    Stroke sign;
    sign.points = {{sx, bot + 0.42 * h}, {sx + 0.16 * sw, bot + 0.5 * h}, {sx + 0.45 * sw, bot}, {sx + sw, top},
                   {sx + sw + 0.06 * size + body.w + 0.08 * size, top}};
    sign.width = th;
    b.strokes.push_back(sign);
    b.put(body, sx + sw + 0.06 * size, 0);
    b.asc = std::max(b.asc, top + th / 2);
    b.desc = std::max(b.desc, -bot + th / 2);
    b.w = sx + sw + 0.06 * size + body.w + 0.08 * size + 0.05 * size;
    return b;
}

// A bracket around something taller than a line (a fraction) is drawn as tall as what it holds.
void stretchFence(Box& f, double asc, double desc, double size) {
    Glyph& g = f.glyphs[0];
    const Metrics& m = metrics(g.font, g.text[0]);
    const double inkH = (m.t - m.b) * g.size;
    const double want = asc + desc + 0.08 * size;
    if (inkH <= 0 || want <= inkH * 1.08) return;
    const double k = want / inkH;
    const double centre = (asc - desc) / 2;
    g.stretch = k;
    g.y = centre - k * (m.t + m.b) / 2 * g.size;
    f.asc = g.y + k * m.t * g.size;
    f.desc = -(g.y + k * m.b * g.size);
    // a little more room beside a tall bracket
    const double extra = std::min(0.12 * size, (k - 1) * 0.06 * size);
    g.x += extra / 2;
    f.w += extra;
}

// An accent over its letters: a bar drawn as a line, the others as small marks centred over them.
Box setAccent(const Node& n, double size, int level) {
    Box body = setNode(n.kids[0], size, level);
    Box b = body;
    const wchar_t a = n.text.empty() ? 0x00AF : n.text[0];
    const double shift = body.italicEnd && body.glyphs.size() == 1 ? 0.08 * size : 0;   // over an italic letter's middle
    const double top = std::max(body.asc, 0.45 * size);
    if (a == 0x00AF) {
        double l = 1e9, r = -1e9;
        for (const Glyph& g : body.glyphs) { l = std::min(l, g.x); r = std::max(r, g.x + g.advance); }
        if (l > r) { l = 0; r = body.w; }
        Stroke bar;
        const double y = top + 0.12 * size;
        bar.points = {{l + 0.05 * size + shift, y}, {r - 0.02 * size + shift, y}};
        bar.width = rule(size);
        b.strokes.push_back(bar);
        b.asc = std::max(b.asc, y + bar.width);
        return b;
    }
    Node mark;
    mark.kind = Node::Char;
    mark.text = std::wstring(1, a == 0x02D9 ? L'.' : a);
    mark.font = Roman;
    mark.cls = Ord;
    const double ms = a == 0x2192 ? size * 0.7 : size;
    Box m = setChar(mark, ms);
    double ml = 1e9, mb = 1e9;
    for (const Glyph& g : m.glyphs) {
        const Metrics& mt = metrics(g.font, g.text[0]);
        ml = std::min(ml, g.x + mt.l * g.size);
        mb = std::min(mb, mt.b * g.size);
    }
    double mw = 0;
    for (const Glyph& g : m.glyphs) { const Metrics& mt = metrics(g.font, g.text[0]); mw = std::max(mw, g.x + mt.r * g.size - ml); }
    b.put(m, (body.w - mw) / 2 - ml + shift, top + 0.06 * size - mb);
    return b;
}

Box setRow(const Node& n, double size, int level) {
    std::vector<Box> items;
    items.reserve(n.kids.size());
    for (const Node& k : n.kids) items.push_back(setNode(k, size, level));
    // brackets: as tall as what is between them (a bracket with a power, (...)^2, too)
    auto fenceChar = [&](size_t k) -> wchar_t {
        const Node& nd = n.kids[k];
        const Node* c = nd.kind == Node::Script ? &nd.kids[0] : &nd;
        return c->kind == Node::Char && c->text.size() == 1 ? c->text[0] : 0;
    };
    auto opens_ = [](wchar_t c) { return c == L'(' || c == L'[' || c == L'{' || c == 0x2329 || c == L'|' || c == 0x2225; };
    auto closes_ = [](wchar_t c) { return c == L')' || c == L']' || c == L'}' || c == 0x232A || c == L'|' || c == 0x2225; };
    auto stretchItem = [&](size_t k, double asc, double desc) {
        const Node& nd = n.kids[k];
        if (nd.kind == Node::Script) {
            Box base = setChar(nd.kids[0], size);
            if (base.glyphs.size() != 1) return;
            stretchFence(base, asc, desc, size);
            items[k] = setScript(nd, size, level, &base);
        } else if (items[k].glyphs.size() == 1) stretchFence(items[k], asc, desc, size);
    };
    std::vector<size_t> opens;
    for (size_t k = 0; k < items.size(); ++k) {
        const wchar_t c = fenceChar(k);
        const bool o = opens_(c), cl = closes_(c);
        if (cl && !opens.empty() && (!o || fenceChar(opens.back()) == c)) {
            const size_t a = opens.back();
            opens.pop_back();
            double asc = 0, desc = 0;
            for (size_t m = a + 1; m < k; ++m) { asc = std::max(asc, items[m].asc); desc = std::max(desc, items[m].desc); }
            stretchItem(a, asc, desc);
            stretchItem(k, asc, desc);
        } else if (o) opens.push_back(k);
    }
    Box b;
    const double scale = level >= 100 ? 0.6 : 1.0;
    double x = 0;
    Cls prev = Open;   // at the start a + or - is a sign
    bool prevFunc = false, any = false;
    for (size_t k = 0; k < items.size(); ++k) {
        Box& it = items[k];
        if (it.glyphs.empty() && it.strokes.empty() && it.w == 0) continue;
        Cls cls = it.first;
        const bool lastItem = k + 1 == items.size();
        if (cls == Bin && (prev == Open || prev == Bin || prev == Rel || prev == Punct || prev == Big || lastItem)) cls = Ord;   // a sign
        double gap = 0;
        if (any) {
            if (cls == Rel || prev == Rel) gap = 0.28 * size * scale;
            else if (cls == Bin || prev == Bin) gap = 0.22 * size * scale;
            else if (prev == Punct) gap = 0.17 * size;
            else if ((prevFunc || prev == Big) && cls != Open && cls != Close && cls != Punct) gap = 0.17 * size;
            else if (it.func && prev != Open) gap = 0.17 * size;
        }
        x += gap;
        b.put(it, x, 0);
        x += it.w;
        if (!any) b.first = cls;
        any = true;
        prev = it.last == Bin && cls == Ord && it.first == Bin ? Ord : it.last;
        if (it.big) prev = Big;
        prevFunc = it.func;
        b.italicEnd = it.italicEnd;
        b.last = prev;
    }
    b.w = x;
    return b;
}

Box setNode(const Node& n, double size, int level) {
    switch (n.kind) {
    case Node::Row: return setRow(n, size, level);
    case Node::Char: return setChar(n, size);
    case Node::Script: return setScript(n, size, level);
    case Node::Frac: return setFrac(n, size, level);
    case Node::Root: return setRoot(n, size, level);
    case Node::Space: { Box b; b.w = n.em * size; return b; }
    case Node::Accent: return setAccent(n, size, level);
    }
    return Box{};
}

// Glyphs that follow on in the same font, size and line become one piece of text.
void merge(std::vector<Glyph>& gs) {
    std::vector<Glyph> out;
    for (const Glyph& g : gs) {
        if (!out.empty()) {
            Glyph& p = out.back();
            if (p.font == g.font && p.slant == g.slant && std::fabs(p.size - g.size) < 1e-6 && std::fabs(p.stretch - g.stretch) < 1e-6 && p.stretch == 1 &&
                std::fabs(p.y - g.y) < 1e-6 && std::fabs(p.x + p.advance - g.x) < 1e-6) {
                p.text += g.text;
                p.advance += g.advance;
                continue;
            }
        }
        out.push_back(g);
    }
    gs.swap(out);
}

// ---- reading what is on a page ----
const char* const kMark = "FeatherEq";

std::string encode(const std::wstring& w) {   // ASCII for a mark's string: the rest of UTF-8 as %XX
    const std::string u = platform::utf8(w);
    std::string out;
    for (unsigned char c : u) {
        if (c >= 0x20 && c < 0x7F && c != '%') out += static_cast<char>(c);
        else { char buf[4]; std::snprintf(buf, sizeof buf, "%%%02X", c); out += buf; }
    }
    return out;
}
std::wstring decode(const std::wstring& w) {
    std::string u;
    for (size_t k = 0; k < w.size(); ++k) {
        if (w[k] == L'%' && k + 2 < w.size()) {
            const std::string hex{static_cast<char>(w[k + 1]), static_cast<char>(w[k + 2])};
            u += static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
            k += 2;
        } else u += static_cast<char>(w[k]);
    }
    return platform::wide(u);
}
std::wstring markParam(FPDF_PAGEOBJECTMARK m, const char* key) {
    const auto& api = pdfium::api();
    unsigned long n = 0;
    if (!api.FPDFPageObjMark_GetParamStringValue(m, key, nullptr, 0, &n) || n < 2) return {};
    std::vector<unsigned short> buf(n / 2 + 1);
    api.FPDFPageObjMark_GetParamStringValue(m, key, buf.data(), n, &n);
    return std::wstring(buf.begin(), buf.begin() + (n / 2 - 1));
}
// This editor's equation an object belongs to: its id (empty when none), notation and size.
std::wstring ourMark(FPDF_PAGEOBJECT o, std::wstring* src = nullptr, double* size = nullptr, std::wstring* wrap = nullptr) {
    const auto& api = pdfium::api();
    const int marks = api.FPDFPageObj_CountMarks(o);
    for (int k = 0; k < marks; ++k) {
        FPDF_PAGEOBJECTMARK m = api.FPDFPageObj_GetMark(o, k);
        unsigned long n = 0;
        if (!m || !api.FPDFPageObjMark_GetName(m, nullptr, 0, &n) || n < 2) continue;
        std::vector<unsigned short> buf(n / 2 + 1);
        api.FPDFPageObjMark_GetName(m, buf.data(), n, &n);
        if (std::wstring(buf.begin(), buf.begin() + (n / 2 - 1)) != L"FeatherEq") continue;
        if (src) *src = decode(markParam(m, "src"));
        if (size) *size = std::wcstod(markParam(m, "size").c_str(), nullptr);
        if (wrap) *wrap = markParam(m, "wrap");
        return markParam(m, "id");
    }
    return {};
}

struct Unit {
    int idx = -1;
    bool text = true;
    std::wstring chars;
    std::vector<bool> italic;     // per character: set italic (a letter of a mathematical alphabet, or an italic font)
    bool mathFont = false;        // Cambria Math, STIX Math, LaTeX's CMMI/CMSY ...
    bool uprightMath = false;     // plain letters in a mathematics font that has italic ones of its own: meant upright
    bool big = false;             // TeX's extension font (big brackets, roots, sums): not the line's size
    bool upright = false;         // an upright font (not italic)
    wchar_t accent = 0;           // an accent over it (read from the page: \bar, \hat ...)
    size_t accentAt = 0;          // over which of its characters
    bool accentMark = false;      // this text is only an accent over the letter below it
    bool ours = false;            // part of an equation this editor made (never part of another one)
    bool tex = false;             // in one of TeX's fonts (Computer Modern, Latin Modern)
    double l = 0, b = 0, r = 0, t = 0;
    double x = 0, y = 0, size = 0;
    COLORREF color = 0;
    bool used = false;
};

// TeX's extension font (CMEX) has no Unicode for its glyphs: its codes, as the characters they draw.
wchar_t cmexChar(unsigned code) {
    switch (code) {
    case 0x00: case 0x10: case 0x12: case 0x30: case 0x40: return L'(';   // (0x20 is read as a space)
    case 0x01: case 0x11: case 0x13: case 0x21: case 0x31: case 0x41: return L')';
    case 0x02: case 0x14: case 0x22: case 0x32: case 0x34: case 0x36: case 0x68: return L'[';
    case 0x03: case 0x15: case 0x23: case 0x33: case 0x35: case 0x37: case 0x69: return L']';
    case 0x08: case 0x1A: case 0x28: case 0x38: case 0x3A: case 0x3C: case 0x6E: return L'{';
    case 0x09: case 0x1B: case 0x29: case 0x39: case 0x3B: case 0x3D: case 0x6F: return L'}';
    case 0x0C: case 0x3E: case 0x42: case 0x43: return L'|';
    case 0x0A: case 0x1C: case 0x2A: return 0x2329;
    case 0x0B: case 0x1D: case 0x2B: return 0x232A;
    case 0x48: case 0x49: return 0x222E;
    case 0x50: case 0x58: return 0x2211;
    case 0x51: case 0x59: return 0x220F;
    case 0x52: case 0x5A: return 0x222B;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: return 0x221A;
    default: return 0;
    }
}
const wchar_t kAccents[] = L"\x00AF\x02C9\x02C6\x02DC\x02D9\x00A8\x20D7";
bool isAccentChar(wchar_t c) { return c != 0 && std::wcschr(kAccents, c) != nullptr; }
const wchar_t* accentCommand(wchar_t c) {
    switch (c) {
    case 0x00AF: case 0x02C9: return L"\\bar";
    case 0x02C6: case 0x005E: return L"\\hat";
    case 0x02DC: case 0x007E: return L"\\tilde";
    case 0x02D9: return L"\\dot";
    case 0x00A8: return L"\\ddot";
    default: return L"\\vec";
    }
}
// Text that reads as mathematics in an upright font (TeX's CMR digits and signs, a function's name): no word of two or
// more letters other than a function's name.
bool mathlike(const std::wstring& t) {
    size_t k = 0;
    while (k < t.size()) {
        if (t[k] < 0x80 && iswalpha(t[k])) {
            size_t j = k;
            while (j < t.size() && t[j] < 0x80 && iswalpha(t[j])) ++j;
            if (j - k > 1 && !isFunction(t.substr(k, j - k))) return false;
            k = j;
        } else ++k;
    }
    return true;
}

bool mathFontName(const std::string& n) {
    for (const char* k : {"Math", "CMMI", "CMSY", "CMEX", "MSAM", "MSBM", "STIX", "Euclid", "MTExtra", "MT-Extra", "LMMath", "lmmi"})
        if (n.find(k) != std::string::npos) return true;
    return false;
}

std::vector<Unit> unitsOf(FPDF_PAGE pg, FPDF_TEXTPAGE tp) {
    const auto& api = pdfium::api();
    std::vector<Unit> us;
    const int n = api.FPDFPage_CountObjects(pg);
    for (int k = 0; k < n; ++k) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, k);
        if (!o) continue;
        const int type = api.FPDFPageObj_GetType(o);
        float l, b, r, t;
        if (!api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
        Unit u;
        u.idx = k; u.l = l; u.b = b; u.r = r; u.t = t;
        u.ours = !ourMark(o).empty();
        if (type == FPDF_PAGEOBJ_PATH) {
            // a fraction's bar or a root's line: thin, flat and a few points long at least
            if (t - b > 2.5 || r - l < 3 || r - l < 4 * (t - b)) continue;
            u.text = false;
            u.y = (b + t) / 2;
            us.push_back(u);
            continue;
        }
        if (type != FPDF_PAGEOBJ_TEXT) continue;
        FS_MATRIX m{};
        api.FPDFPageObj_GetMatrix(o, &m);
        if (std::fabs(m.b) > 1e-3 || std::fabs(m.c) > 1e-3) continue;   // turned text: not an equation's
        float fs = 0;
        api.FPDFTextObj_GetFontSize(o, &fs);
        u.size = fs * std::fabs(m.d);
        u.x = m.e; u.y = m.f;
        FPDF_FONT font = api.FPDFTextObj_GetFont(o);
        const std::string name = pageobj::baseFontName(font);
        u.mathFont = mathFontName(name);
        u.tex = texFontName(name);
        u.big = name.find("CMEX") != std::string::npos;
        bool italicFont = name.find("Italic") != std::string::npos || name.find("Oblique") != std::string::npos ||
                          name.find("CMMI") != std::string::npos || name.find("-It") != std::string::npos;
        int angle = 0;
        if (font && api.FPDFFont_GetItalicAngle(font, &angle) && angle != 0) italicFont = true;
        unsigned long bytes = api.FPDFTextObj_GetText(o, tp, nullptr, 0);
        if (bytes < 2) continue;
        std::vector<unsigned short> buf(bytes / 2 + 1);
        api.FPDFTextObj_GetText(o, tp, buf.data(), bytes);
        const size_t cnt = bytes / 2 - 1;
        bool mathItalics = false, plainLetters = false;
        for (size_t q = 0; q < cnt; ++q) {
            unsigned cp = buf[q];
            if (cp >= 0xD800 && cp <= 0xDBFF && q + 1 < cnt && buf[q + 1] >= 0xDC00 && buf[q + 1] <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (buf[q + 1] - 0xDC00);
                ++q;
            }
            if (cp == 0x210E || (cp > 0xFFFF && pageobj::mathLetter(cp))) {
                u.chars += cp == 0x210E ? L'h' : pageobj::mathLetter(cp);
                u.italic.push_back(true);
                mathItalics = true;
                continue;
            }
            if (cp > 0xFFFF || cp == 0x2061 || cp == 0x2062 || cp == 0x2063 || cp == 0x200B) continue;
            if (u.big && cp < 0x80) {   // CMEX: its codes
                if (const wchar_t m = cmexChar(cp)) { u.chars += m; u.italic.push_back(false); }
                continue;
            }
            if (cp == L'\r' || cp == L'\n' || cp == 0xFFFE || cp == 0) continue;
            if (cp < 0x80 && iswalpha(static_cast<wint_t>(cp))) plainLetters = true;
            u.chars += static_cast<wchar_t>(cp);
            u.italic.push_back(italicFont);
        }
        if (u.chars.find_first_not_of(L' ') == std::wstring::npos) continue;
        // Cambria Math and its kind write italic letters as the mathematical alphabet's: their plain letters are upright
        u.uprightMath = u.mathFont && !italicFont && plainLetters && (name.find("Math") != std::string::npos || mathItalics);
        u.upright = !italicFont;
        // TeX's roman (CMR, Latin Modern) in an equation: its digits, signs and function names are mathematics
        const bool texRoman = name.find("CMR") != std::string::npos || name.find("CMBX") != std::string::npos || name.find("LMRoman") != std::string::npos;
        if (texRoman && mathlike(u.chars)) u.mathFont = true;
        {   // an accent alone (TeX sets \bar h as a macron over the h)
            std::wstring t = u.chars;
            t.erase(std::remove(t.begin(), t.end(), L' '), t.end());
            u.accentMark = t.size() == 1 && isAccentChar(t[0]) && (texRoman || u.mathFont || u.size > 0);
            if (u.accentMark) u.mathFont = true;
        }
        unsigned R = 0, G = 0, B = 0, A = 255;
        api.FPDFPageObj_GetFillColor(o, &R, &G, &B, &A);
        u.color = RGB(R, G, B);
        // TeX's extension font draws a sum's or an integral's sign and a tall bracket in one piece of text: each its own,
        // where it is (limits follow the sign, not the bracket)
        std::wstring letters = u.chars;
        letters.erase(std::remove(letters.begin(), letters.end(), L' '), letters.end());
        if (u.big && letters.size() > 1) {
            std::vector<std::array<double, 4>> boxes;
            for (int ci = 0, nc = api.FPDFText_CountChars(tp); ci < nc; ++ci) {
                if (api.FPDFText_GetTextObject(tp, ci) != o) continue;
                double l, r, b, t;
                if (api.FPDFText_GetUnicode(tp, ci) > L' ' && api.FPDFText_GetCharBox(tp, ci, &l, &r, &b, &t)) boxes.push_back({l, b, r, t});
            }
            if (boxes.size() == letters.size()) {
                for (size_t q = 0; q < letters.size(); ++q) {
                    Unit p = u;
                    p.chars = std::wstring(1, letters[q]);
                    p.italic.assign(1, false);
                    p.l = boxes[q][0]; p.b = boxes[q][1]; p.r = boxes[q][2]; p.t = boxes[q][3];
                    p.x = p.l;
                    us.push_back(p);
                }
                continue;
            }
        }
        us.push_back(u);
    }
    return us;
}

std::wstring escape(wchar_t c) {
    switch (c) {
    case L'{': return L"\\{";
    case L'}': return L"\\}";
    case L'\\': return L"\\\\";
    case L'^': return L"\\^";
    case L'_': return L"\\_";
    case 0x2212: return L"-";
    case 0x2032: return L"'";
    case 0x22C5: return L"\x00B7";
    default: return std::wstring(1, c);
    }
}

// The notation for one piece of text: its letters (upright words of a mathematics font as \text{} or a function's name),
// signs, and superscript characters as powers.
std::wstring notationOf(const Unit& u) {
    std::wstring out;
    size_t k = 0;
    while (k < u.chars.size()) {
        const wchar_t c = u.chars[k];
        wchar_t base;
        const int sk = of(c, base);
        if (sk != 0) {
            std::wstring run;
            while (k < u.chars.size() && of(u.chars[k], base) == sk) { run += escape(base); ++k; }
            out += (sk > 0 ? L"^{" : L"_{") + run + L"}";
            continue;
        }
        if (c < 0x80 && iswalpha(c) && !u.italic[k]) {   // upright letters: a function's name, a word, or one letter
            size_t j = k;
            while (j < u.chars.size() && u.chars[j] < 0x80 && iswalpha(u.chars[j]) && !u.italic[j]) ++j;
            const std::wstring word = u.chars.substr(k, j - k);
            if (isFunction(word)) out += L"\\" + word + L" ";
            else if (u.uprightMath || word.size() > 1) out += L"\\text{" + word + L"}";
            else out += word;
            k = j;
            continue;
        }
        if (c == L' ') { if (!out.empty() && out.back() != L' ') out += L' '; ++k; continue; }
        out += escape(c);
        ++k;
    }
    return out;
}

struct Reader {
    std::vector<Unit>& us;
    std::vector<int> barsUsed;

    // The most common size and the baseline of the text in it among `ids`.
    void mainLine(const std::vector<int>& ids, double& size, double& base) {
        // the line's size: the largest text (powers, indices and a word set small under a letter are smaller; TeX's big
        // brackets and roots do not count)
        double big = 0;
        for (int id : ids) if (us[id].text && !us[id].big && !us[id].accentMark) big = std::max(big, us[id].size);
        if (big <= 0) for (int id : ids) if (us[id].text) big = std::max(big, us[id].size);
        size = big;
        double most = -1;
        std::map<long, double> byBase;
        // (the top and bottom of a fraction are not on the line: TeX may set them full size)
        auto fractionPart = [&](const Unit& u) {
            for (int id : ids) {
                const Unit& v = us[id];
                if (v.text || u.r <= v.l || u.l >= v.r || (v.t - v.b) > 0.3 * size || (v.r - v.l) < 0.5 * (u.r - u.l)) continue;
                const double y = (v.b + v.t) / 2;
                if ((u.b >= y - 0.3 * size && u.b - y < 0.9 * size) || (u.t <= y + 0.3 * size && y - u.t < 0.9 * size)) return true;
            }
            return false;
        };
        for (int id : ids)
            if (us[id].text && !us[id].big && !us[id].accentMark && std::fabs(us[id].size - size) < 0.6 && !fractionPart(us[id]))
                byBase[std::lround(us[id].y * 2)] += static_cast<double>(us[id].chars.size());
        if (byBase.empty()) {   // nothing but fractions (and brackets round them): the line is where the widest bar's axis is
            double widest = -1;
            for (int id : ids) {
                const Unit& v = us[id];
                if (!v.text && (v.t - v.b) <= 0.3 * size && v.r - v.l > widest) { widest = v.r - v.l; base = (v.b + v.t) / 2 - axisOf(size); }
            }
            if (widest > 0) return;
            for (int id : ids)
                if (us[id].text && !us[id].big && !us[id].accentMark && std::fabs(us[id].size - size) < 0.6) byBase[std::lround(us[id].y * 2)] += static_cast<double>(us[id].chars.size());
        }
        long bb = 0;
        for (auto& [k, v] : byBase) if (v > most) { most = v; bb = k; }
        base = bb / 2.0;
    }

    // TeX's accents (\bar h, \dot m): the mark over a letter goes with it
    void attachAccents(const std::vector<int>& ids) {
        for (int a : ids) {
            Unit& m = us[static_cast<size_t>(a)];
            if (!m.text || !m.accentMark || m.used == false) continue;
            const double cx = (m.l + m.r) / 2;
            int best = -1;
            double bestGap = 1e9;
            for (int id : ids) {
                const Unit& u = us[static_cast<size_t>(id)];
                if (id == a || !u.text || u.accentMark || cx < u.l - 1 || cx > u.r + 1) continue;
                const double gap = m.b - u.t;
                if (gap > -0.35 * u.size && gap < 0.5 * u.size && std::fabs(gap) < bestGap) { bestGap = std::fabs(gap); best = id; }
            }
            if (best < 0) { m.accentMark = false; continue; }   // not over a letter: an ordinary character
            std::wstring t = m.chars;
            t.erase(std::remove(t.begin(), t.end(), L' '), t.end());
            Unit& under = us[static_cast<size_t>(best)];
            under.accent = t[0];
            // over which letter: by where it is along the text (its letters are about as wide)
            size_t letters = 0;
            for (wchar_t ch : under.chars) if (ch != L' ') ++letters;
            const double along = under.r > under.l ? (cx - under.l) / (under.r - under.l) : 0;
            const size_t nth = std::min(letters ? letters - 1 : 0, static_cast<size_t>(std::max(0.0, along * letters)));
            size_t seen = 0;
            for (size_t q = 0; q < under.chars.size(); ++q) if (under.chars[q] != L' ' && seen++ == nth) { under.accentAt = q; break; }
            accentsTaken.push_back(a);
        }
    }
    std::vector<int> accentsTaken;
    bool twoLines = false;   // full-size text a line above or below the main line: a formula wrapped onto two lines

    std::wstring build(std::vector<int> ids, double* sizeOut = nullptr, double* baseOut = nullptr) {
        attachAccents(ids);
        ids.erase(std::remove_if(ids.begin(), ids.end(), [&](int id) { return std::find(accentsTaken.begin(), accentsTaken.end(), id) != accentsTaken.end(); }), ids.end());
        // fractions and roots first, the widest bar first (an outer fraction holds the inner ones)
        std::vector<int> bars;
        for (int id : ids) if (!us[id].text) bars.push_back(id);
        std::sort(bars.begin(), bars.end(), [&](int a, int b) { return us[a].r - us[a].l > us[b].r - us[b].l; });
        struct Item { double x; std::wstring text; int unit = -1; double y = 0, size = 0; };
        std::vector<Item> items;
        std::vector<bool> taken(us.size(), false);
        double approx = 0;
        for (int id : ids) if (us[id].text) approx = std::max(approx, us[id].size);
        if (approx <= 0) approx = 12;
        for (int bar : bars) {
            if (taken[bar]) continue;
            const Unit& B = us[bar];
            std::vector<int> above, below;
            for (int id : ids) {
                if (id == bar || taken[id]) continue;
                const Unit& u = us[id];
                const double cx = (u.l + u.r) / 2;
                if (cx < B.l - 0.1 * approx || cx > B.r + 0.1 * approx || u.l < B.l - 0.6 * approx || u.r > B.r + 0.6 * approx) continue;
                const double mid = u.text ? (u.b + u.t) / 2 : u.y;
                if (mid > B.y && u.b >= B.y - 0.25 * approx && u.b < B.y + 3 * approx) above.push_back(id);
                else if (mid < B.y && u.t <= B.y + 0.25 * approx && u.t > B.y - 3 * approx) below.push_back(id);
            }
            if (!above.empty() && !below.empty()) {
                for (int id : above) taken[id] = true;
                for (int id : below) taken[id] = true;
                taken[bar] = true;
                barsUsed.push_back(bar);
                double fs = 0;   // the size of its text (a fraction set small and raised is a power)
                for (int id : above) if (us[id].text) fs = std::max(fs, us[id].size);
                for (int id : below) if (us[id].text) fs = std::max(fs, us[id].size);
                items.push_back({B.l, L"\\frac{" + build(above) + L"}{" + build(below) + L"}", -1, B.y, fs});
                continue;
            }
            if (above.empty() && !below.empty()) {   // a root's line: the radical sign just before it
                int sign = -1;
                for (int id : ids)
                    if (!taken[id] && us[id].text && us[id].chars.find(0x221A) != std::wstring::npos && us[id].r <= B.l + 0.5 * approx &&
                        us[id].r >= B.l - 1.5 * approx && us[id].t >= B.y - 0.5 * approx) sign = id;
                if (sign >= 0) {
                    for (int id : below) taken[id] = true;
                    taken[bar] = taken[sign] = true;
                    barsUsed.push_back(bar);
                    barsUsed.push_back(sign);
                    // what the sign's text holds besides the sign (rare) stays before it
                    std::wstring before = us[sign].chars.substr(0, us[sign].chars.find(0x221A));
                    Unit pre = us[sign];
                    pre.chars = before;
                    pre.italic.resize(before.size());
                    items.push_back({us[sign].l, notationOf(pre) + L"\\sqrt{" + build(below) + L"}"});
                    continue;
                }
            }
            taken[bar] = true;   // a line the equation does not need (it stays on the page)
        }
        std::vector<int> rest;
        for (int id : ids) if (!taken[id] && us[id].text) rest.push_back(id);
        double size = approx, base = 0;
        bool onLine = false;   // text of the line's size outside the fractions
        for (int id : rest) if (!us[id].big && !us[id].accentMark && us[id].size > 0.88 * approx) onLine = true;
        if (onLine) mainLine(rest, size, base);
        else if (!barsUsed.empty()) {   // the line runs through a full-size fraction's bar (not a small one in a power)
            base = us[barsUsed.back()].y - axisOf(size);
            double widest = -1;
            for (const Item& it : items)
                if (it.unit < 0 && it.size >= 0.88 * approx) {
                    for (int bar : bars)
                        if (std::fabs(us[bar].y - it.y) < 1e-6 && std::fabs(us[bar].l - it.x) < 1e-6 && us[bar].r - us[bar].l > widest) { widest = us[bar].r - us[bar].l; base = it.y - axisOf(size); }
                }
        }
        else if (!rest.empty()) mainLine(rest, size, base);
        if (sizeOut) *sizeOut = size;
        if (baseOut) *baseOut = base;
        for (int id : rest) items.push_back({us[id].x, std::wstring(), id});
        std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.x < b.x; });
        // powers and indices: smaller text raised or lowered from the main line
        std::wstring out;
        int openKind = 0;
        double prevRight = -1e9;
        auto close = [&] { if (openKind != 0) { out += L"}"; openKind = 0; } };
        for (const Item& it : items) {
            if (it.unit < 0) {
                // a fraction set small, over the line (an exponent such as (γ-1)/γ) or under it
                int fk = 0;
                if (it.size > 0 && it.size < 0.88 * size) fk = it.y > base + 0.35 * size ? 1 : it.y < base - 0.1 * size ? -1 : 0;
                if (fk != openKind) {
                    close();
                    if (fk != 0) { while (!out.empty() && out.back() == L' ') out.pop_back(); out += fk > 0 ? L"^{" : L"_{"; openKind = fk; }
                }
                if (fk == 0 && !out.empty() && out.back() != L' ') out += L' ';
                out += it.text;
                if (fk == 0) out += L' ';
                prevRight = -1e9;
                continue;
            }
            const Unit& u = us[it.unit];
            int kind = 0;
            const bool smaller = u.size < 0.88 * size && !u.big;
            if (!u.big && !smaller && std::fabs(u.y - base) > 0.9 * size) twoLines = true;
            if (u.big) kind = 0;   // TeX's tall brackets and roots hang below the line: never a power
            else if ((smaller && u.y > base + 0.12 * size) || u.y > base + 0.45 * size) kind = 1;
            else if ((smaller && u.y < base - 0.08 * size) || u.y < base - 0.3 * size) kind = -1;
            std::wstring t = notationOf(u);
            if (u.accent) {   // \bar{h}: the letter under it in braces
                Unit before = u, mark = u, after = u;
                before.chars = u.chars.substr(0, u.accentAt);
                mark.chars = u.chars.substr(u.accentAt, 1);
                after.chars = u.accentAt + 1 < u.chars.size() ? u.chars.substr(u.accentAt + 1) : std::wstring();
                before.italic.assign(u.italic.begin(), u.italic.begin() + static_cast<long>(std::min(u.accentAt, u.italic.size())));
                mark.italic.assign(1, u.accentAt < u.italic.size() ? u.italic[u.accentAt] : true);
                after.italic.assign(u.italic.begin() + static_cast<long>(std::min(u.accentAt + 1, u.italic.size())), u.italic.end());
                before.accent = mark.accent = after.accent = 0;
                std::wstring m = notationOf(mark);
                while (!m.empty() && m.back() == L' ') m.pop_back();
                t = notationOf(before) + std::wstring(accentCommand(u.accent)) + L"{" + m + L"}" + notationOf(after);
            }
            if (kind != openKind) {
                close();
                if (kind != 0) { while (!out.empty() && out.back() == L' ') out.pop_back(); out += kind > 0 ? L"^{" : L"_{"; openKind = kind; }
                else if (u.l - prevRight > 0.3 * size && !out.empty() && out.back() != L' ') out += L' ';
            } else if (u.l - prevRight > 0.3 * size && !out.empty() && out.back() != L' ' && out.back() != L'{') out += L' ';
            if (kind != 0) { while (!t.empty() && t.back() == L' ') t.pop_back(); while (!t.empty() && t.front() == L' ') t.erase(0, 1); }
            out += t;
            prevRight = u.r;
        }
        close();
        // tidy: single spaces, none at the ends
        std::wstring tidy;
        for (wchar_t c : out) if (!(c == L' ' && (tidy.empty() || tidy.back() == L' '))) tidy += c;
        while (!tidy.empty() && tidy.back() == L' ') tidy.pop_back();
        return tidy;
    }
};

std::string newId() {
    static std::mt19937 rng{std::random_device{}()};
    char buf[16];
    std::snprintf(buf, sizeof buf, "%08x", static_cast<unsigned>(rng()));
    return buf;
}

// Puts the equation's objects on the page, starting at (x, y) on the baseline. False when one cannot be made.
// `tex`: the document's Latin Modern fonts (upright, italic, bold), for an equation laid out in them.
// `id` and `wrap` (the second line's start from the first's, "dx dy") for the two lines of a wrapped equation: both are
// placed with one mark, so the equation is found whole.
bool place(FPDF_DOCUMENT doc, FPDF_PAGE pg, const Layout& L, double x, double y, COLORREF color, const std::wstring& source, double size, bool mark,
           const FPDF_FONT* tex = nullptr, const std::string& givenId = std::string(), const std::string& wrap = std::string()) {
    const auto& api = pdfium::api();
    const std::string id = givenId.empty() ? newId() : givenId, src = encode(source);
    char sz[32];
    std::snprintf(sz, sizeof sz, "%.2f", size);
    auto tag = [&](FPDF_PAGEOBJECT o) {
        if (!mark) return;
        FPDF_PAGEOBJECTMARK m = api.FPDFPageObj_AddMark(o, kMark);
        if (!m) return;
        api.FPDFPageObjMark_SetStringParam(doc, o, m, "id", id.c_str());
        api.FPDFPageObjMark_SetStringParam(doc, o, m, "src", src.c_str());
        api.FPDFPageObjMark_SetStringParam(doc, o, m, "size", sz);
        if (!wrap.empty()) api.FPDFPageObjMark_SetStringParam(doc, o, m, "wrap", wrap.c_str());
    };
    for (const Glyph& g : L.glyphs) {
        FPDF_PAGEOBJECT o = isTex(g.font) ? (tex && tex[texFile(g.font)] ? api.FPDFPageObj_CreateTextObj(doc, tex[texFile(g.font)], static_cast<float>(g.size)) : nullptr)
                                          : api.FPDFPageObj_NewTextObj(doc, kFontNames[g.font], static_cast<float>(g.size));
        if (!o) return false;
        std::vector<unsigned short> w(g.text.begin(), g.text.end());
        w.push_back(0);
        if (!api.FPDFText_SetText(o, w.data())) { api.FPDFPageObj_Destroy(o); return false; }
        api.FPDFPageObj_SetFillColor(o, GetRValue(color), GetGValue(color), GetBValue(color), 255);
        const FS_MATRIX m{1, 0, g.slant ? 0.21f : 0.f, static_cast<float>(g.stretch), static_cast<float>(x + g.x), static_cast<float>(y + g.y)};
        api.FPDFPageObj_SetMatrix(o, &m);
        tag(o);
        api.FPDFPage_InsertObject(pg, o);
    }
    for (const Stroke& s : L.strokes) {
        if (s.points.size() < 2) continue;
        FPDF_PAGEOBJECT p = api.FPDFPageObj_CreateNewPath(static_cast<float>(x + s.points[0].first), static_cast<float>(y + s.points[0].second));
        if (!p) return false;
        for (size_t k = 1; k < s.points.size(); ++k) api.FPDFPath_LineTo(p, static_cast<float>(x + s.points[k].first), static_cast<float>(y + s.points[k].second));
        api.FPDFPath_SetDrawMode(p, FPDF_FILLMODE_NONE, 1);
        api.FPDFPageObj_SetStrokeWidth(p, static_cast<float>(s.width));
        api.FPDFPageObj_SetStrokeColor(p, GetRValue(color), GetGValue(color), GetBValue(color), 255);
        api.FPDFPageObj_SetLineJoin(p, FPDF_LINEJOIN_MITER);
        api.FPDFPageObj_SetLineCap(p, FPDF_LINECAP_BUTT);
        tag(p);
        api.FPDFPage_InsertObject(pg, p);
    }
    return true;
}

} // namespace

bool layout(const std::wstring& source, double size, Layout& out, std::wstring* error, bool tex) {
    struct Face { bool was = gTex; ~Face() { gTex = was; } } keep;   // (laid out in the face asked for, then as before)
    gTex = tex;
    out = Layout{};
    out.tex = tex;
    const std::wstring src = normalise(source);
    Parser p(src);
    Node root;
    if (!p.row(root, 0)) { if (error) *error = p.err; return false; }
    if (size < 2 || size > 400) { if (error) *error = L"The size must be between 2 and 400 points."; return false; }
    Box b = setNode(root, size, 0);
    for (const Glyph& g : b.glyphs)
        for (wchar_t c : g.text)
            if (!metrics(g.font, c).drawn) {
                if (error) *error = std::wstring(L"“") + c + L"” cannot be set in an equation. Use the buttons or a command such as \\alpha.";
                return false;
            }
    merge(b.glyphs);
    out.glyphs = std::move(b.glyphs);
    out.strokes = std::move(b.strokes);
    out.width = b.w;
    out.ascent = b.asc;
    out.descent = b.desc;
    return true;
}


Found at(Document& d, int page, double x, double y, bool anyText) {
    Found f;
    FPDF_PAGE pg = d.page(page);
    if (!pg) return f;
    // objects are remembered by number until the equation is set again: split now what writing the page would split
    if (contentgen::prepare(d.handle(), pg)) { d.resetText(page); d.pinPage(page); }
    FPDF_TEXTPAGE tp = d.text(page);
    if (!tp) return f;
    const auto& api = pdfium::api();
    std::vector<Unit> us = unitsOf(pg, tp);
    // the text under the point (the smallest), else the nearest within a few points
    int seed = -1;
    double bestArea = 1e18, bestDist = 4;
    for (size_t k = 0; k < us.size(); ++k) {
        const Unit& u = us[k];
        if (!u.text) continue;
        const double dx = std::max({u.l - x, 0.0, x - u.r}), dy = std::max({u.b - y, 0.0, y - u.t});
        const double dist = std::hypot(dx, dy);
        const double area = (u.r - u.l) * (u.t - u.b);
        if (dist == 0 && (bestDist > 0 || area < bestArea)) { seed = static_cast<int>(k); bestArea = area; bestDist = 0; }
        else if (dist > 0 && dist < bestDist) { seed = static_cast<int>(k); bestDist = dist; }
    }
    if (std::getenv("FEATHER_TRACE_EQ") && seed >= 0) std::fprintf(stderr, "seed %d '%s' math=%d\n", us[seed].idx, platform::utf8(us[seed].chars).c_str(), us[seed].mathFont);
    if (seed < 0) return f;
    // one of this editor's: all its objects, its notation as it was typed
    {
        std::wstring src, wrap;
        double size = 0;
        const std::wstring id = ourMark(api.FPDFPage_GetObject(pg, us[seed].idx), &src, &size, &wrap);
        if (!id.empty()) {
            f.found = f.ours = true;
            f.page = page;
            f.source = src;
            const int n = api.FPDFPage_CountObjects(pg);
            double L = 1e9, B = 1e9, R = -1e9, T = -1e9;
            FPDF_PAGEOBJECT firstText = nullptr;
            int firstIdx = -1;
            for (int k = 0; k < n; ++k) {
                FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, k);
                if (!o || ourMark(o) != id) continue;
                f.objects.push_back(k);
                float l, b, r, t;
                if (api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) { L = std::min<double>(L, l); B = std::min<double>(B, b); R = std::max<double>(R, r); T = std::max<double>(T, t); }
                if (!firstText && api.FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_TEXT) { firstText = o; firstIdx = k; }
            }
            f.left = L; f.bottom = B; f.right = R; f.top = T;
            f.size = size > 0 ? size : 12;
            f.tex = firstText && texFontName(pageobj::baseFontName(api.FPDFTextObj_GetFont(firstText)));   // set in Latin Modern
            // where it starts: from its first piece of text, as the notation sets it (it may have been moved or scaled)
            Layout lay;
            if (firstText && layout(src, f.size, lay, nullptr, f.tex) && !lay.glyphs.empty()) {
                FS_MATRIX m{};
                api.FPDFPageObj_GetMatrix(firstText, &m);
                float fs = 0;
                api.FPDFTextObj_GetFontSize(firstText, &fs);
                const Glyph& g = lay.glyphs[0];
                const double k = g.size > 0 && fs > 0 ? fs * std::fabs(m.a) / g.size : 1;
                f.size *= k;
                f.baseline = m.f - g.y * k;
                f.left = m.e - g.x * k;
                unsigned r = 0, gg = 0, b = 0, a = 255;
                api.FPDFPageObj_GetFillColor(firstText, &r, &gg, &b, &a);
                f.color = RGB(r, gg, b);
            } else { f.baseline = B; (void)firstIdx; }
            f.startX = f.left;
            double dx = 0, dy = 0;   // set on two lines: where the second starts
            if (!wrap.empty() && std::swscanf(wrap.c_str(), L"%lf %lf", &dx, &dy) == 2) {
                f.wrapped = true;
                f.wrapX = f.left + dx;
                f.wrapBaseline = f.baseline + dy;
            }
            const double pw = d.pageSize(page).w;
            f.centred = std::fabs((L + R) / 2 - pw / 2) < 0.04 * pw;
            return f;
        }
    }
    // a power or an index was clicked: the equation grows from the text it hangs from
    for (int pass = 0; pass < 2; ++pass)
        for (size_t k = 0; k < us.size(); ++k) {
            const Unit& u = us[k];
            const Unit& s0 = us[static_cast<size_t>(seed)];
            if (!u.text || u.size < s0.size * 1.12 || (!u.mathFont && s0.mathFont)) continue;
            const double hgap = std::max(u.l - s0.r, s0.l - u.r);
            const bool overlap = std::min(u.t, s0.t) - std::max(u.b, s0.b) > 0;
            if (overlap && hgap < 0.6 * u.size) seed = static_cast<int>(k);
        }
    if (!us[seed].mathFont && !anyText) return f;
    // a power of ordinary text (the ² of m/s² in a sentence or a table): that text is edited as text
    if (!anyText)
        for (const Unit& u : us)
            if (u.text && !u.mathFont && u.size > us[seed].size * 1.12 && std::max(us[seed].l - u.r, u.l - us[seed].r) < 0.3 * u.size &&
                std::min(u.t, us[seed].t) - std::max(u.b, us[seed].b) > 0)
                return f;
    // Grow from it: what is near and belongs to it. Mathematics fonts, the bars, smaller text (powers) and text on the same
    // line between its parts; not the sentences above and below.
    const double size = us[seed].size > 0 ? us[seed].size : 12;
    std::vector<int> in{seed};
    us[seed].used = true;
    double L = us[seed].l, B = us[seed].b, R = us[seed].r, T = us[seed].t;
    // Each part joins through a part next to it (not through the box of all of them: a formula that runs onto a second line
    // of a table's cell would take in that line).
    auto mathish = [&](const Unit& u) {
        if (u.mathFont) return true;
        std::wstring t = u.chars;
        t.erase(std::remove(t.begin(), t.end(), L' '), t.end());
        return u.upright && !t.empty() && mathlike(t);
    };
    auto joins = [&](const Unit& u, const Unit& v) {
        if (u.ours || v.ours) return false;   // an equation made here is one of its own
        const double hgap = std::max(u.l - v.r, v.l - u.r);
        const double vov = std::min(u.t, v.t) - std::max(u.b, v.b);   // how far they overlap up and down (< 0: the gap)
        if (!u.text && !v.text) return false;
        if (!u.text)   // a bar (a fraction's, a root's) just over or under the text, across it, or the line runs into it
            return (u.r - u.l) < 30 * size && mathish(v) &&
                   ((hgap < -0.2 * std::min(u.r - u.l, v.r - v.l) && -vov < 0.75 * size) ||
                    (std::fabs(v.y + axisOf(v.size) - u.y) < 0.35 * size && hgap < 0.6 * size));
        if (!v.text) {
            const bool side = u.l >= v.l - 0.4 * size && u.r <= v.r + 0.4 * size && -vov < 0.75 * size;            // over or under it
            const bool beside = std::fabs(u.y + axisOf(u.size) - v.y) < 0.35 * size && hgap < 0.6 * size && mathish(u);   // the line through it
            const bool touching = vov > 0 && hgap < 0.3 * size && mathish(u);                                  // a root's sign
            return side || beside || touching;
        }
        const bool sameLine = std::fabs(u.y - v.y) < 0.2 * size && std::fabs(u.size - v.size) < 0.12 * size;
        if (sameLine && hgap < 0.6 * size && (mathish(u) || anyText)) return true;
        // TeX's tall brackets, roots, sums and its radical signs (they hang below the line)
        auto tall = [](const Unit& x) { return x.big || x.chars.find(0x221A) != std::wstring::npos; };
        if ((tall(u) || tall(v)) && vov > 0 && hgap < 0.6 * size && mathish(u)) return true;
        // a power or an index and what it hangs from (either way round)
        auto script = [&](const Unit& small, const Unit& base) {
            const double rise = small.y - base.y;   // a power sits up to about half its base's size higher, an index lower
            return small.size < 0.88 * base.size && small.size > 0.3 * base.size && hgap < 0.6 * base.size && vov > -0.2 * base.size &&
                   rise < 0.65 * base.size && rise > -0.5 * base.size;
        };
        if (script(u, v) || (script(v, u) && mathish(u))) return true;
        if (u.accentMark && hgap < 0 && u.b >= v.t - 0.35 * size && u.b - v.t < 0.5 * size) return true;   // an accent over it
        if (v.accentMark && hgap < 0 && v.b >= u.t - 0.35 * size && v.b - u.t < 0.5 * size && mathish(u)) return true;
        return false;
    };
    // the parts joined from `from`; with `bands`, only parts on the same line of text as each other (bandOf)
    std::vector<int> bandOf(us.size(), -1);
    auto grow = [&](int from, bool bands) {
        std::vector<int> got{from};
        us[static_cast<size_t>(from)].used = true;
        for (bool grew = true; grew;) {
            grew = false;
            for (size_t k = 0; k < us.size(); ++k) {
                Unit& u = us[k];
                if (u.used) continue;
                bool j = false;
                for (int id : got)
                    if ((!bands || bandOf[k] == bandOf[static_cast<size_t>(id)]) && joins(u, us[static_cast<size_t>(id)])) { j = true; break; }
                if (!j) continue;
                u.used = true;
                got.push_back(static_cast<int>(k));
                grew = true;
            }
        }
        return got;
    };
    in = grow(seed, false);
    for (int id : in) { const Unit& u = us[static_cast<size_t>(id)]; L = std::min(L, u.l); B = std::min(B, u.b); R = std::max(R, u.r); T = std::max(T, u.t); }
    // A formula LaTeX wrapped onto a second line of a table's cell: what was taken in runs over two lines of text, and
    // the lines' fractions and powers interleave. The lines are told apart by the baselines of the full-size text; each
    // part joins only parts on its own line. The clicked formula is read on its line, and with the start of the next
    // line when it ends in "=" or a sign (the wrap), or with the end of the line before when that one does.
    std::vector<int> second;   // the other line's part of it, if any
    bool secondFirst = false;  // that part comes first (the line above)
    {
        // The baselines of the lines: full-size text that is not the top or bottom of a fraction (TeX sets those full
        // size too), and full-size fractions by their bars (on the line's axis).
        auto radical = [](const Unit& u) { return u.chars.find(0x221A) != std::wstring::npos; };   // set raised, hanging below
        auto full = [&](const Unit& u) { return u.text && !u.big && !radical(u) && !u.accentMark && std::fabs(u.size - size) < 0.12 * size; };
        auto barY = [](const Unit& v) { return (v.b + v.t) / 2; };
        // a fraction's bar (not a table's rule across the page)
        auto thinBar = [&](const Unit& v) { return !v.text && (v.t - v.b) <= 0.3 * size && v.r - v.l > 0.3 * size && v.r - v.l < 20 * size; };
        // u is in the top or the bottom of the fraction of bar v (directly over or under it, or higher up in a top that
        // holds a fraction of its own): within the bar's width and less than two and a half lines from it
        auto overUnder = [&](const Unit& u, const Unit& v) {
            if (!thinBar(v)) return false;
            const double cx = (u.l + u.r) / 2, cy = (u.b + u.t) / 2, y = barY(v);
            return cx > v.l && cx < v.r && std::fabs(cy - y) > 0.1 * size && std::fabs(cy - y) < 2.5 * size;
        };
        // a bar inside another fraction's top or bottom (a fraction in a fraction) is not on the line
        auto innerBar = [&](const Unit& v, const std::vector<int>& among) {
            for (int o : among) {
                const Unit& w = us[static_cast<size_t>(o)];
                if (&w == &v || !thinBar(w) || w.r - w.l <= v.r - v.l) continue;
                const double cx = (v.l + v.r) / 2;
                if (cx > w.l && cx < w.r && std::fabs(barY(v) - barY(w)) < 2.5 * size && std::fabs(barY(v) - barY(w)) > 0.1 * size) return true;
            }
            return false;
        };
        // what is near: the mathematics a few lines up and down, across about the cell's width
        std::vector<int> pool;
        // The column it is in: between the ordinary text on the rows around it (the names on its left, the conditions on
        // its right), or about a cell's width each way.
        double colL = L - 25 * size, colR = R + 25 * size;
        {
            const Unit& s0 = us[static_cast<size_t>(seed)];
            auto wordy = [](const std::wstring& t) {   // a word (two letters running), not a bracket or a digit of a formula
                for (size_t i = 0; i + 1 < t.size(); ++i) if (iswalpha(t[i]) && iswalpha(t[i + 1])) return true;
                return false;
            };
            for (const Unit& u : us) {
                // (full size: a word set small under a letter, as in p_{abs}, is a formula's)
                if (!u.text || u.used || mathish(u) || !wordy(u.chars) || std::fabs(u.y - s0.y) > 4 * size || u.size < 0.88 * size) continue;
                if (u.r <= L) colL = std::max(colL, u.r);          // (outside what was taken in: a column's edge is not
                else if (u.l >= R) colR = std::min(colR, u.l);     // inside the formula)
            }
        }
        {
            const double y0 = us[static_cast<size_t>(seed)].y;
            for (size_t k = 0; k < us.size(); ++k) {
                const Unit& u = us[k];
                if (u.ours || std::fabs((u.text ? u.y : (u.b + u.t) / 2) - y0) > 5 * size || u.r <= colL + 1 || u.l >= colR - 1) continue;
                if (!u.text || mathish(u) || u.used) pool.push_back(static_cast<int>(k));
            }
        }
        std::vector<double> ys;
        for (int id : pool) {
            const Unit& u = us[static_cast<size_t>(id)];
            if (full(u)) {
                bool part = false;
                for (int b2 : pool) if (overUnder(u, us[static_cast<size_t>(b2)])) { part = true; break; }
                if (!part) ys.push_back(u.y);
            } else if (thinBar(u) && !innerBar(u, pool)) {   // a fraction with full-size text over and under its bar
                bool over = false, under = false;
                for (int t2 : pool) {
                    const Unit& w = us[static_cast<size_t>(t2)];
                    if (!full(w) || !overUnder(w, u)) continue;
                    if (w.b >= barY(u) - 0.3 * size) over = true; else under = true;
                }
                if (over && under) ys.push_back(barY(u) - axisOf(size));
            }
        }
        std::sort(ys.rbegin(), ys.rend());
        std::vector<double> lines;
        for (double y : ys) if (lines.empty() || lines.back() - y > 0.7 * size) lines.push_back(y);
        if (std::getenv("FEATHER_TRACE_EQ")) { std::fprintf(stderr, "lines:"); for (double y : lines) std::fprintf(stderr, " %.1f", y); std::fprintf(stderr, " (from %zu)\n", ys.size()); }
        if (lines.size() > 1) {
            // every part on the page near the formula belongs to the line whose baseline is nearest
            for (size_t k = 0; k < us.size(); ++k) {   // (across the neighbourhood the lines were found in)
                const Unit& u = us[k];
                if (std::fabs((u.text ? u.y : (u.b + u.t) / 2) - us[static_cast<size_t>(seed)].y) > 5 * size || u.r <= colL + 1 || u.l >= colR - 1) continue;
                // (a tall bracket, root or integral of TeX's stands on the line's axis; its origin is at its top)
                const double y = !u.text ? (u.b + u.t) / 2 : (u.big || radical(u)) ? (u.b + u.t) / 2 - axisOf(size) : u.y;
                double best = 1e18;
                for (size_t q = 0; q < lines.size(); ++q) if (std::fabs(y - lines[q]) < best) { best = std::fabs(y - lines[q]); bandOf[k] = static_cast<int>(q); }
            }
            // the top and bottom of a fraction (a small one in a power too) are on the line of its bar (a fraction's: with
            // text over it and under it; a root's line has text only under it)
            std::vector<char> fractionBar(us.size(), 0);
            for (size_t q = 0; q < us.size(); ++q) {
                const Unit& v = us[q];
                if (v.text || bandOf[q] < 0 || (v.t - v.b) > 0.3 * size) continue;
                bool over = false, under = false;
                for (const Unit& u : us) {
                    if (!u.text || u.r <= v.l || u.l >= v.r) continue;
                    const double y = barY(v), us0 = std::max(u.size, 1.0);
                    if (u.b >= y - 0.3 * us0 && u.b - y < 0.6 * us0) over = true;
                    else if (u.t <= y + 0.3 * us0 && y - u.t < 0.6 * us0) under = true;
                }
                fractionBar[q] = over && under;
            }
            for (size_t k = 0; k < us.size(); ++k) {
                const Unit& u = us[k];
                if (!u.text || bandOf[k] < 0) continue;
                double best = 1e18;
                for (size_t q = 0; q < us.size(); ++q) {
                    const Unit& v = us[q];
                    if (!fractionBar[q] || u.r <= v.l || u.l >= v.r) continue;
                    const double y = barY(v), us0 = std::max(u.size, 1.0);
                    const double d = u.b >= y - 0.3 * us0 ? u.b - y : y - u.t;   // over it, or under it
                    if (d > -0.3 * us0 && d < 0.6 * us0 && d < best) { best = d; bandOf[k] = bandOf[q]; }
                }
            }
            if (std::getenv("FEATHER_TRACE_EQ")) {
                std::fprintf(stderr, "col %.1f-%.1f\n", colL, colR);
                for (int id : in) std::fprintf(stderr, "  first %s '%s' y=%.1f band %d\n", us[static_cast<size_t>(id)].text ? "text" : "line", platform::utf8(us[static_cast<size_t>(id)].chars).c_str(), us[static_cast<size_t>(id)].y, bandOf[static_cast<size_t>(id)]);
            }
            for (Unit& u : us) u.used = false;
            in = grow(seed, true);
            const int band = bandOf[static_cast<size_t>(seed)];
            auto extent = [&](const std::vector<int>& ids, double& l, double& r) {
                l = 1e18; r = -1e18;
                for (int id : ids) { l = std::min(l, us[static_cast<size_t>(id)].l); r = std::max(r, us[static_cast<size_t>(id)].r); }
            };
            // ends in a sign that a formula cannot end with: the rest is on the next line
            auto endsOpen = [&](const std::vector<int>& ids) {
                int last = -1;
                for (int id : ids) {
                    const Unit& u = us[static_cast<size_t>(id)];
                    if (u.text && bandOf[static_cast<size_t>(id)] >= 0 && std::fabs(u.size - size) < 0.12 * size && (last < 0 || u.r > us[static_cast<size_t>(last)].r)) last = id;
                }
                if (last < 0) return false;
                std::wstring t = us[static_cast<size_t>(last)].chars;
                while (!t.empty() && t.back() == L' ') t.pop_back();
                return !t.empty() && std::wstring(L"=+-\x2212\x00D7\x00B7\x00F7<>\x2264\x2265\x2248\x2260\x00B1\x2192\x21D2").find(t.back()) != std::wstring::npos;
            };
            // the first (or last) formula of another line, within the cell's width
            auto partOn = [&](int line, bool first) {
                int pick = -1;
                for (size_t k = 0; k < us.size(); ++k) {
                    const Unit& u = us[k];
                    if (u.used || bandOf[k] != line || !u.text || std::fabs(u.size - size) >= 0.12 * size || !mathish(u)) continue;
                    if (u.r <= colL + 1 || u.l >= colR - 1) continue;
                    if (first && u.l > L + 0.5 * size) continue;   // the next line's part starts under the formula's start or left of it
                    if (pick < 0 || (first ? u.l < us[static_cast<size_t>(pick)].l : u.r > us[static_cast<size_t>(pick)].r)) pick = static_cast<int>(k);
                }
                return pick >= 0 ? grow(pick, true) : std::vector<int>{};
            };
            double ml, mr;
            extent(in, ml, mr);
            if (band + 1 < static_cast<int>(lines.size()) && endsOpen(in)) {
                std::vector<int> next = partOn(band + 1, true);
                double nl, nr;
                extent(next, nl, nr);
                if (!next.empty() && nl <= ml + 0.5 * size) second = next;   // it starts the next line, under or left of it
                else for (int id : next) us[static_cast<size_t>(id)].used = false;
            } else if (band > 0 && [&] {   // (only when it starts its line: nothing of a formula before it in the cell)
                           for (size_t k = 0; k < us.size(); ++k)
                               if (!us[k].used && bandOf[k] == band && us[k].text && mathish(us[k]) && us[k].r <= ml && us[k].l > colL) return false;
                           return true;
                       }()) {
                std::vector<int> prev = partOn(band - 1, false);
                double pl, pr;
                extent(prev, pl, pr);
                // (more than a lone sign: the end of a formula, its start or its name with it)
                size_t letters = 0;
                for (int id : prev) for (wchar_t ch : us[static_cast<size_t>(id)].chars) if (iswalnum(ch)) ++letters;
                if (!prev.empty() && letters > 0 && endsOpen(prev) && ml <= pl + 0.5 * size) { second = prev; secondFirst = true; }
                else for (int id : prev) us[static_cast<size_t>(id)].used = false;
            }
            if (std::getenv("FEATHER_TRACE_EQ")) {
                std::fprintf(stderr, "wrap: band %d open %d main %.1f-%.1f second %zu first %d:", band, endsOpen(in) ? 1 : 0, ml, mr, second.size(), secondFirst ? 1 : 0);
                for (int id : second) std::fprintf(stderr, " '%s'", platform::utf8(us[static_cast<size_t>(id)].chars).c_str());
                std::fprintf(stderr, "\n");
            }
            L = 1e9; B = 1e9; R = -1e9; T = -1e9;
            for (int id : in) { const Unit& u = us[static_cast<size_t>(id)]; L = std::min(L, u.l); B = std::min(B, u.b); R = std::max(R, u.r); T = std::max(T, u.t); }
        }
    }
    // Mathematics inside a sentence (a variable in a paragraph of a LaTeX paper, "the range of K"): ordinary text on its
    // line close by. The sentence is edited as text then (its editor's Equation button reads it as an equation).
    if (!anyText) {
        const double seedY = us[seed].y;
        for (const Unit& u : us) {
            if (u.used || !u.text || u.mathFont || u.ours) continue;
            if (u.chars.find_first_not_of(L" ") == std::wstring::npos) continue;
            // on the line: its baseline, or (a power clicked beside a word, kg/m³) overlapping the clicked text
            const bool onLine = std::fabs(u.y - seedY) < 0.2 * size ||
                                (std::min(u.t, us[seed].t) - std::max(u.b, us[seed].b) > 0 && u.size > us[seed].size * 1.12);
            if (!onLine) continue;
            const double hgap = std::max(u.l - R, L - u.r);
            if (hgap < 0.8 * std::max(size, u.size)) return f;
        }
    }
    if (std::getenv("FEATHER_TRACE_EQ"))
        for (int id : in) std::fprintf(stderr, "unit %d %s '%s' x=%.1f y=%.1f size=%.1f box %.1f %.1f %.1f %.1f math=%d\n", us[id].idx, us[id].text ? "text" : "line",
                                       platform::utf8(us[id].chars).c_str(), us[id].x, us[id].y, us[id].size, us[id].l, us[id].b, us[id].r, us[id].t, us[id].mathFont);
    Reader rd{us, {}};
    double mainSize = size, base = us[seed].y;
    f.source = rd.build(in, &mainSize, &base);
    if (f.source.empty()) return f;
    f.found = true;
    f.page = page;
    f.size = mainSize;
    f.baseline = base;
    f.uncertain = rd.twoLines;
    if (!second.empty()) {   // the two lines of a wrapped formula, read each on its own and put together
        Reader r2{us, {}};
        double size2 = mainSize, base2 = base;
        const std::wstring more = r2.build(second, &size2, &base2);
        if (!more.empty()) {
            double l2 = 1e18;
            for (int id : second) l2 = std::min(l2, us[static_cast<size_t>(id)].l);
            double l1 = 1e18;
            for (int id : in) l1 = std::min(l1, us[static_cast<size_t>(id)].l);
            rd.barsUsed.insert(rd.barsUsed.end(), r2.barsUsed.begin(), r2.barsUsed.end());
            if (secondFirst) {
                f.source = more + L" " + f.source;
                f.wrapX = l1; f.wrapBaseline = base;   // the clicked part is the second line
                f.baseline = base2;
                f.startX = l2;
            } else {
                f.source += L" " + more;
                f.wrapX = l2; f.wrapBaseline = base2;
                f.startX = l1;
            }
            f.wrapped = true;
            f.uncertain = rd.twoLines || r2.twoLines;
            in.insert(in.end(), second.begin(), second.end());
        }
    }
    std::vector<bool> isBar(us.size(), false);
    for (int bId : rd.barsUsed) isBar[static_cast<size_t>(bId)] = true;
    L = 1e9; B = 1e9; R = -1e9; T = -1e9;
    bool colour = false;
    for (int id : in) {
        const Unit& u = us[id];
        if (!u.text && !isBar[static_cast<size_t>(id)]) continue;   // a line that is not part of it stays
        f.objects.push_back(u.idx);
        L = std::min(L, u.l); B = std::min(B, u.b); R = std::max(R, u.r); T = std::max(T, u.t);
        if (u.text && !colour) { f.color = u.color; colour = true; }
        if (u.text && u.tex) f.tex = true;   // set again in Latin Modern, as TeX set it
    }
    std::sort(f.objects.begin(), f.objects.end());
    f.objects.erase(std::unique(f.objects.begin(), f.objects.end()), f.objects.end());
    f.left = L; f.bottom = B; f.right = R; f.top = T;
    const double pw = d.pageSize(page).w;
    f.centred = std::fabs((L + R) / 2 - pw / 2) < 0.04 * pw;
    return f;
}

namespace {
// Where an equation written in the notation may be broken onto a second line: after a relation or a sign that is not
// inside braces or \left ... \right (and not a power's or an index's sign).
std::vector<size_t> breaks(const std::wstring& src) {
    std::vector<size_t> out;
    int depth = 0;
    static const std::wstring kSigns = L"=+-\x2212<>\x2264\x2265\x2248\x2260\x00B1\x00D7\x00B7\x2192";
    static const wchar_t* const kCommands[] = {L"le", L"ge", L"leq", L"geq", L"ne", L"neq", L"approx", L"times", L"cdot", L"pm", L"to", L"equiv"};
    wchar_t prev = 0;
    for (size_t i = 0; i < src.size(); ++i) {
        const wchar_t c = src[i];
        if (c == L'\\') {
            size_t j = i + 1;
            while (j < src.size() && iswalpha(src[j])) ++j;
            const std::wstring name = src.substr(i + 1, j - i - 1);
            if (name == L"left") ++depth;
            else if (name == L"right") --depth;
            else if (depth == 0 && prev != L'^' && prev != L'_')
                for (const wchar_t* k : kCommands) if (name == k) { out.push_back(j); break; }
            i = j > i + 1 ? j - 1 : i + 1;   // (a sign after the backslash: \{ \})
            prev = L'a';
            continue;
        }
        if (c == L'{' || c == L'(' || c == L'[') ++depth;   // (not inside brackets either: TeX breaks between terms)
        else if (c == L'}' || c == L')' || c == L']') --depth;
        // (a sign straight after a relation or another sign is a leading minus or plus: no break after it)
        else if (depth == 0 && i > 0 && kSigns.find(c) != std::wstring::npos && prev != L'^' && prev != L'_' && kSigns.find(prev) == std::wstring::npos)
            out.push_back(i + 1);
        if (c != L' ') prev = c;
    }
    while (!out.empty() && src.find_first_not_of(L" ", out.back()) == std::wstring::npos) out.pop_back();   // (one at the very end)
    return out;
}
} // namespace

// Whether the page's text is mostly in TeX's fonts (a LaTeX document): a new equation there is set in Latin Modern.
bool pageIsTex(Document& d, int page) {
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    const auto& api = pdfium::api();
    int tex = 0, other = 0;
    for (int i = 0, n = api.FPDFPage_CountObjects(pg); i < n; ++i) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
        (texFontName(pageobj::baseFontName(api.FPDFTextObj_GetFont(o))) ? tex : other) += 1;
    }
    return tex > 0 && tex >= other;
}

bool replace(Document& d, const Found& f, const std::wstring& source, double size, std::wstring* why) {
    Layout L;
    const bool remove = source.find_first_not_of(L" \t\r\n") == std::wstring::npos;
    if (!remove && !layout(source, size, L, why, f.tex)) return false;
    FPDF_PAGE pg = d.page(f.page);
    if (!pg) return false;
    const auto& api = pdfium::api();
    // the edge of the table cell it is in (what follows it moves up to there): read before anything changes on the page
    double limit = d.pageSize(f.page).w;
    bool column = false;   // a column's edge was found (what follows may then be further away and still be moved)
    if (!remove && !f.centred) {
        for (const tables::Table& t : tables::detect(d, f.page))
            for (size_t c = 0; c + 1 < t.colX.size(); ++c)
                if (f.left >= t.colX[c] - 1 && f.left <= t.colX[c + 1] && f.bottom <= t.top() && f.top >= t.bottom()) { limit = t.colX[c + 1]; column = true; }
        {
            // the next column starts where many lines to the right start (a table without ruled columns, or one found
            // wrongly: the further of the two)
            // (and nothing near the equation runs across that place)
            std::map<long, std::set<long>> rowsAt;   // the lines (by baseline) that start at each place
            std::vector<std::array<double, 4>> nearby;
            for (int i = 0, n = api.FPDFPage_CountObjects(pg); i < n; ++i) {
                FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                if (!o || api.FPDFPageObj_GetType(o) != FPDF_PAGEOBJ_TEXT) continue;
                FS_MATRIX m{};
                api.FPDFPageObj_GetMatrix(o, &m);
                rowsAt[std::lround(m.e)].insert(std::lround(m.f));
                float l, b, r, t;
                if (api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t) && t > f.bottom - 4 * size && b < f.top + 4 * size) nearby.push_back({l, b, r, t});
            }
            int most = 0;   // a column's start is shared by far more lines than any other place
            for (const auto& [xs, rows] : rowsAt) most = std::max(most, static_cast<int>(rows.size()));
            for (const auto& [xs, rows] : rowsAt) {
                const int count = static_cast<int>(rows.size());
                if (xs <= f.right + 2 || count < std::max(8, most * 3 / 10)) continue;
                bool crossed = false;
                for (const auto& q : nearby) if (q[0] < xs - 2 && q[2] > xs + 2) { crossed = true; break; }
                if (!crossed) { limit = column ? std::max(limit, xs - 0.8 * size) : xs - 0.8 * size; column = true; break; }   // (a clear gap kept: TeX leaves an em or more between columns)
            }
        }
    }
    pg = d.page(f.page);
    if (!pg) return false;
    d.beginChange(remove ? L"Remove an equation" : L"Edit an equation");
    std::vector<int> idx = f.objects;
    std::sort(idx.begin(), idx.end(), std::greater<int>());
    for (int i : idx) {
        FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
        if (o && api.FPDFPage_RemoveObject(pg, o)) api.FPDFPageObj_Destroy(o);
    }
    double setSize = size;   // what it is set at (smaller when it would not fit its column)
    bool placed = false;
    if (!remove && f.wrapped && !f.centred) {
        // A formula on two lines of its cell: on one line when it now fits, else broken again at the last relation or
        // sign that keeps its first line inside the cell, the rest where the second line was. Both lines are one
        // equation (one mark, with where the second line starts).
        FPDF_FONT texFonts[3] = {};
        if (L.tex) for (int k = 0; k < 3; ++k) texFonts[k] = d.embeddedFont(kTexFontKey + k, mathfonts::kFonts[k].data, mathfonts::kFonts[k].size);
        const double x1 = f.startX, room = limit - x1;
        bool ok = true;
        if (L.width <= room + 0.5) {
            ok = place(d.handle(), pg, L, x1, f.baseline, f.color, source, size, true, texFonts);
        } else {
            const std::vector<size_t> cuts = breaks(source);
            Layout A, Bl;
            size_t cut = std::wstring::npos;
            for (size_t c : cuts) {   // (the first part grows with each later place)
                Layout t;
                if (!layout(source.substr(0, c), size, t, nullptr, f.tex)) continue;
                if (t.width <= room + 0.5) { cut = c; A = t; continue; }   // fits: a later place may too
                if (cut == std::wstring::npos) { cut = c; A = t; }          // none fits: broken as early as it can be
                break;
            }
            if (cut != std::wstring::npos && layout(source.substr(cut), size, Bl, nullptr, f.tex)) {
                char wrap[64];
                std::snprintf(wrap, sizeof wrap, "%.3f %.3f", f.wrapX - x1, f.wrapBaseline - f.baseline);
                const std::string id = newId();
                ok = place(d.handle(), pg, A, x1, f.baseline, f.color, source, size, true, texFonts, id, wrap) &&
                     place(d.handle(), pg, Bl, f.wrapX, f.wrapBaseline, f.color, source, size, true, texFonts, id, wrap);
            } else {
                ok = place(d.handle(), pg, L, x1, f.baseline, f.color, source, size, true, texFonts);   // nowhere to break it
            }
        }
        if (!ok) {
            d.rollbackChange();
            d.resetText(f.page);
            if (why) *why = L"The equation could not be set on the page.";
            return false;
        }
        placed = true;
    }
    if (!remove && !placed) {
        const double x = f.centred ? (f.left + f.right) / 2 - L.width / 2 : f.left;
        // A wider equation pushes what follows it on its line (the next equation of a table's cell, a word) to the right,
        // as far as the edge of its cell: they would overlap.
        const double grow = x + L.width - f.right;
        if (!f.centred && grow > 0.3) {
            struct Piece { FPDF_PAGEOBJECT o; double l, r; };
            struct Box4 { FPDF_PAGEOBJECT o; double l, b, r, t; };
            std::vector<Piece> after;
            std::vector<Box4> around;   // what lies over or under the line (fractions' tops and bottoms)
            for (int i = 0, n = api.FPDFPage_CountObjects(pg); i < n; ++i) {
                FPDF_PAGEOBJECT o = api.FPDFPage_GetObject(pg, i);
                float l, b, r, t;
                if (!o || !api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) continue;
                const int type = api.FPDFPageObj_GetType(o);
                if (type != FPDF_PAGEOBJ_TEXT && type != FPDF_PAGEOBJ_PATH) continue;
                // (what starts in the column moves whole, even when it reaches past its edge: a piece left behind would mix)
                if (l < f.right - 0.5 || l > limit || r - l > 0.6 * (limit - f.left + 1)) continue;
                const double cy = (b + t) / 2;
                if (cy >= f.bottom && cy <= f.top) after.push_back({o, l, r});
                else if (t > f.bottom - 1.2 * size && b < f.top + 1.2 * size) around.push_back({o, l, b, r, t});
            }
            std::sort(after.begin(), after.end(), [](const Piece& a, const Piece& b) { return a.l < b.l; });
            double reach = f.right, from = 1e9;
            std::vector<FPDF_PAGEOBJECT> move;
            for (const Piece& p : after) {
                if (p.l - reach > (column ? 2.5 : 1.2) * size) break;   // a wider gap: the next column's text
                move.push_back(p.o);
                reach = std::max(reach, p.r);
                from = std::min(from, p.l);
            }
            // and what stands just over or under a moved fraction's bar, within its width (the fraction's top and bottom; not
            // the row above or below)
            std::vector<std::array<double, 4>> bars;
            for (FPDF_PAGEOBJECT o : move) {
                float l, b, r, t;
                if (api.FPDFPageObj_GetType(o) == FPDF_PAGEOBJ_PATH && api.FPDFPageObj_GetBounds(o, &l, &b, &r, &t)) bars.push_back({l, b, r, t});
            }
            for (const Box4& q : around) {
                const double cx = (q.l + q.r) / 2;
                for (const auto& bar : bars)
                    if (cx >= bar[0] - 0.5 && cx <= bar[2] + 0.5 && std::max(q.b - bar[3], bar[1] - q.t) < 0.8 * size) { move.push_back(q.o); break; }
            }
            // Pushed past the edge of its column, it would run into the next column: it is set smaller to fit (down to
            // three quarters of its size; a fixed page cannot make the column wider).
            double shift = grow;
            if (column && reach + grow > limit + 0.5) {
                const double fit = std::max(0.75, (L.width - (reach + grow - limit - 0.5)) / L.width);
                Layout smaller;
                if (fit < 0.995 && layout(source, size * fit, smaller, nullptr, f.tex)) {
                    L = std::move(smaller);
                    setSize = size * fit;
                    shift = std::max(0.0, x + L.width - f.right);
                }
            }
            if (std::getenv("FEATHER_TRACE_EQ")) std::fprintf(stderr, "push: grow %.1f limit %.1f column %d after %zu move %zu reach %.1f size %.2f\n", shift, limit, column, after.size(), move.size(), reach, setSize);
            if (shift > 0.3) for (FPDF_PAGEOBJECT o : move) api.FPDFPageObj_Transform(o, 1, 0, 0, 1, shift, 0);   // the gap stays as it was
        }
        FPDF_FONT texFonts[3] = {};
        if (L.tex) for (int k = 0; k < 3; ++k) texFonts[k] = d.embeddedFont(kTexFontKey + k, mathfonts::kFonts[k].data, mathfonts::kFonts[k].size);
        if (!place(d.handle(), pg, L, x, f.baseline, f.color, source, setSize, true, texFonts)) {
            d.rollbackChange();
            d.resetText(f.page);
            if (why) *why = L"The equation could not be set on the page.";
            return false;
        }
    }
    if (!contentgen::generate(d, pg)) {
        d.rollbackChange();
        d.resetText(f.page);
        if (why) *why = L"This page uses drawing that could not be written back exactly, so it was left as it was.";
        return false;
    }
    d.resetText(f.page);
    return true;
}

bool insert(Document& d, int page, double x, double baseline, const std::wstring& source, double size, COLORREF color, std::wstring* why) {
    Layout L;
    if (!layout(source, size, L, why, pageIsTex(d, page))) return false;
    if (source.find_first_not_of(L" \t\r\n") == std::wstring::npos) { if (why) *why = L"Type the equation first."; return false; }
    FPDF_PAGE pg = d.page(page);
    if (!pg) return false;
    d.beginChange(L"Insert an equation");
    FPDF_FONT texFonts[3] = {};
    if (L.tex) for (int k = 0; k < 3; ++k) texFonts[k] = d.embeddedFont(kTexFontKey + k, mathfonts::kFonts[k].data, mathfonts::kFonts[k].size);
    if (!place(d.handle(), pg, L, x, baseline, color, source, size, true, texFonts) || !contentgen::generate(d, pg)) {
        d.rollbackChange();
        d.resetText(page);
        if (why) *why = L"The equation could not be set on the page.";
        return false;
    }
    d.resetText(page);
    return true;
}

bool preview(const std::wstring& source, double size, double scale, std::vector<unsigned char>& bgra, int& w, int& h, std::wstring* error, bool tex) {
    Layout L;
    if (!layout(source, size, L, error, tex)) return false;
    const auto& api = pdfium::api();
    const double pad = 0.25 * size;
    const double pw = std::max(L.width, 0.5 * size) + 2 * pad, ph = std::max(L.ascent + L.descent, 0.8 * size) + 2 * pad;
    w = std::max(1, static_cast<int>(std::ceil(pw * scale)));
    h = std::max(1, static_cast<int>(std::ceil(ph * scale)));
    if (static_cast<double>(w) * h > 40e6) { if (error) *error = L"The equation is too large to show."; return false; }
    FPDF_DOCUMENT doc = api.FPDF_CreateNewDocument();
    if (!doc) return false;
    FPDF_PAGE pg = api.FPDFPage_New(doc, 0, pw, ph);
    FPDF_FONT texFonts[3] = {};
    if (L.tex) for (int k = 0; k < 3; ++k) texFonts[k] = api.FPDFText_LoadFont(doc, mathfonts::kFonts[k].data, static_cast<uint32_t>(mathfonts::kFonts[k].size), FPDF_FONT_TRUETYPE, 1);
    bool ok = pg && place(doc, pg, L, pad, pad + std::max(L.descent, 0.2 * size), RGB(0, 0, 0), source, size, false, texFonts);
    if (ok) {
        FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRA, nullptr, 0);
        ok = bmp != nullptr;
        if (ok) {
            api.FPDFBitmap_FillRect(bmp, 0, 0, w, h, 0xFFFFFFFF);
            api.FPDF_RenderPageBitmap(bmp, pg, 0, 0, w, h, 0, FPDF_ANNOT | FPDF_LCD_TEXT);
            const auto* src = static_cast<const unsigned char*>(api.FPDFBitmap_GetBuffer(bmp));
            const int stride = api.FPDFBitmap_GetStride(bmp);
            bgra.resize(static_cast<size_t>(w) * h * 4);
            for (int r = 0; r < h; ++r) std::memcpy(bgra.data() + static_cast<size_t>(r) * w * 4, src + static_cast<size_t>(r) * stride, static_cast<size_t>(w) * 4);
            api.FPDFBitmap_Destroy(bmp);
        }
    }
    if (pg) api.FPDF_ClosePage(pg);
    for (FPDF_FONT f : texFonts) if (f) api.FPDFFont_Close(f);
    api.FPDF_CloseDocument(doc);
    return ok;
}

std::wstring toScript(const std::wstring& text, int kind, bool* all) {
    std::wstring out;
    bool every = true;
    for (wchar_t c : text) {
        wchar_t b;
        if (of(c, b) != 0) c = b;   // already one kind: made the other
        const wchar_t m = featherscript::script::make(c, kind);
        if (m == c && c != L' ') every = false;
        out += m;
    }
    if (all) *all = every;
    return out;
}

std::wstring fromScript(const std::wstring& text) { return featherscript::script::plain(text); }

int scriptKind(const std::wstring& text) {
    int k = 0;
    for (wchar_t c : text) {
        wchar_t b;
        const int s = of(c, b);
        if (s == 0 || (k != 0 && s != k)) return 0;
        k = s;
    }
    return k;
}

} // namespace equation
