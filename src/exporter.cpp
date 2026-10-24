#include "exporter.hpp"
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <algorithm>
using std::min; using std::max;
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>

namespace exporter {
namespace {

// ---------- helpers ----------

std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::string xmlEscape(const std::wstring& w) {
    std::wstring o;
    for (wchar_t c : w) {
        if (c < 0x20 && c != L'\t' && c != L'\n' && c != L'\r') continue;   // not allowed in XML
        if (c == 0xFFFE || c == 0xFFFF) continue;
        switch (c) {
            case L'&': o += L"&amp;"; break;
            case L'<': o += L"&lt;"; break;
            case L'>': o += L"&gt;"; break;
            case L'"': o += L"&quot;"; break;
            default: o += c;
        }
    }
    return utf8(o);
}

bool writeFileBytes(const std::wstring& path, const std::string& data, std::wstring& err) {
    std::vector<unsigned char> v(data.begin(), data.end());
    // Reuse the same safe write used for saving: temp file, then replace.
    const std::wstring tmp = path + L".feather-tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { err = L"The file could not be written. The folder may be read-only, or the file may be open in another program."; return false; }
    size_t done = 0; DWORD e = 0;
    while (done < v.size()) {
        DWORD n = 0;
        if (!WriteFile(f, v.data() + done, static_cast<DWORD>(std::min<size_t>(v.size() - done, 1u << 24)), &n, nullptr)) { e = GetLastError(); break; }
        done += n;
    }
    CloseHandle(f);
    if (e || done != v.size()) {
        DeleteFileW(tmp.c_str());
        err = (e == ERROR_DISK_FULL || e == ERROR_HANDLE_DISK_FULL) ? L"The disk is full. Free some space and try again."
                                                                      : L"The file could not be written.";
        return false;
    }
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        err = L"The file could not be saved. It may be open in Word or Excel.";
        return false;
    }
    return true;
}

// ---------- minimal zip writer (stored, no compression) for .docx / .xlsx ----------

uint32_t crc32(const std::string& s) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : s) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

class Zip {
public:
    void add(const std::string& name, const std::string& data) {
        Entry e{name, crc32(data), static_cast<uint32_t>(data.size()), static_cast<uint32_t>(out_.size())};
        entries_.push_back(e);
        u32(0x04034b50); u16(20); u16(0x0800); u16(0); u16(time_); u16(date_);
        u32(e.crc); u32(e.size); u32(e.size); u16(static_cast<uint16_t>(name.size())); u16(0);
        out_ += name; out_ += data;
    }
    std::string finish() {
        const uint32_t start = static_cast<uint32_t>(out_.size());
        for (const Entry& e : entries_) {
            u32(0x02014b50); u16(20); u16(20); u16(0x0800); u16(0); u16(time_); u16(date_);
            u32(e.crc); u32(e.size); u32(e.size); u16(static_cast<uint16_t>(e.name.size()));
            u16(0); u16(0); u16(0); u16(0); u32(0); u32(e.offset);
            out_ += e.name;
        }
        const uint32_t len = static_cast<uint32_t>(out_.size()) - start;
        u32(0x06054b50); u16(0); u16(0); u16(static_cast<uint16_t>(entries_.size())); u16(static_cast<uint16_t>(entries_.size()));
        u32(len); u32(start); u16(0);
        return out_;
    }
    Zip() {
        SYSTEMTIME t; GetLocalTime(&t);
        time_ = static_cast<uint16_t>((t.wHour << 11) | (t.wMinute << 5) | (t.wSecond / 2));
        date_ = static_cast<uint16_t>(((t.wYear - 1980) << 9) | (t.wMonth << 5) | t.wDay);
    }
private:
    struct Entry { std::string name; uint32_t crc, size, offset; };
    void u16(uint16_t v) { out_.push_back(static_cast<char>(v & 0xFF)); out_.push_back(static_cast<char>(v >> 8)); }
    void u32(uint32_t v) { u16(static_cast<uint16_t>(v & 0xFFFF)); u16(static_cast<uint16_t>(v >> 16)); }
    std::string out_;
    std::vector<Entry> entries_;
    uint16_t time_ = 0, date_ = 0;
};

// ---------- page text layout ----------

struct Word { std::wstring text; double x0, x1; };

struct Line {
    std::wstring text;
    std::vector<Word> words;
    double x0 = 0, x1 = 0, top = 0, bottom = 0, size = 0;
    bool bold = false;
};

// Rebuilds the lines of a page (reading order as PDFium reports it) with positions and fonts.
std::vector<Line> pageLines(Document& d, int page) {
    std::vector<Line> lines;
    FPDF_TEXTPAGE tp = d.text(page);
    if (!tp) return lines;
    const auto& api = pdfium::api();
    const int n = api.FPDFText_CountChars(tp);
    Line cur;
    Word word;
    bool inWord = false;
    double sizeSum = 0; int sizeN = 0, boldN = 0;
    auto endWord = [&] {
        if (inWord) { cur.words.push_back(word); inWord = false; word = {}; }
    };
    auto endLine = [&] {
        endWord();
        if (!cur.words.empty()) {
            cur.size = sizeN ? sizeSum / sizeN : 0;
            cur.bold = sizeN && boldN * 2 > sizeN;
            while (!cur.text.empty() && cur.text.back() == L' ') cur.text.pop_back();
            lines.push_back(cur);
        }
        cur = {}; sizeSum = 0; sizeN = 0; boldN = 0;
    };
    for (int i = 0; i < n; ++i) {
        unsigned u = api.FPDFText_GetUnicode(tp, i);
        if (u == '\r' || u == '\n') { endLine(); continue; }
        if (u == 0xFFFE || u == 0) continue;
        wchar_t ch = static_cast<wchar_t>(u);
        if (u > 0xFFFF) continue;
        double l, r, b, t;
        if (!api.FPDFText_GetCharBox(tp, i, &l, &r, &b, &t)) continue;
        const bool space = ch == L' ' || ch == 0xA0 || ch == L'\t';
        if (cur.words.empty() && !inWord) { cur.x0 = l; cur.top = t; cur.bottom = b; }
        if (!space) {
            if (!inWord) { word.x0 = l; inWord = true; }
            word.text += ch;
            word.x1 = r;
            cur.x1 = std::max(cur.x1, r);
            cur.top = std::max(cur.top, t); cur.bottom = std::min(cur.bottom, b);
            double fs = api.FPDFText_GetFontSize(tp, i);
            if (fs > 0) { sizeSum += fs; ++sizeN; if (api.FPDFText_GetFontWeight(tp, i) >= 600) ++boldN; }
            cur.text += ch;
        } else {
            endWord();
            if (!cur.text.empty() && cur.text.back() != L' ') cur.text += L' ';
        }
    }
    endLine();
    return lines;
}

double bodySize(const std::vector<Line>& lines) {
    std::map<int, int> hist;   // size in half points -> character count
    for (const Line& l : lines) hist[static_cast<int>(std::lround(l.size * 2))] += static_cast<int>(l.text.size());
    int best = 0, bestN = -1;
    for (auto& kv : hist) if (kv.second > bestN) { best = kv.first; bestN = kv.second; }
    return best / 2.0;
}

int headingLevel(const Line& l, double body) {
    if (body <= 0 || l.text.size() > 120) return 0;
    const double ratio = l.size / body;
    if (ratio >= 1.7) return 1;
    if (ratio >= 1.35) return 2;
    if (ratio >= 1.12) return 3;
    if (l.bold && l.text.size() < 70 && ratio >= 0.95 && l.text.back() != L'.') return 4;
    return 0;
}

bool isBullet(const std::wstring& t, std::wstring& rest) {
    if (t.size() > 2 && (t[0] == L'•' || t[0] == L'-' || t[0] == L'*' || t[0] == L'●' || t[0] == L'▪') && t[1] == L' ') {
        rest = t.substr(2);
        return true;
    }
    size_t i = 0;
    while (i < t.size() && iswdigit(t[i])) ++i;
    if (i > 0 && i < 4 && i + 1 < t.size() && (t[i] == L'.' || t[i] == L')') && t[i + 1] == L' ') return false;   // numbered: keep the number
    return false;
}

// ---------- table detection ----------

// A line is "table-like" when its words fall into at least two clearly separated groups.
std::vector<Word> cellsOf(const Line& l, double gapMin) {
    std::vector<Word> cells;
    for (const Word& w : l.words) {
        if (!cells.empty() && w.x0 - cells.back().x1 < gapMin) {
            cells.back().text += L' ' + w.text;
            cells.back().x1 = w.x1;
        } else cells.push_back(w);
    }
    return cells;
}

struct Block {
    bool table = false;
    std::vector<Line> lines;                         // paragraph lines
    std::vector<std::vector<std::wstring>> rows;     // table rows
};

std::vector<Block> pageBlocks(Document& d, int page, bool wantTables) {
    std::vector<Line> lines = pageLines(d, page);
    std::vector<Block> blocks;
    if (lines.empty()) return blocks;
    const double body = bodySize(lines);
    const double gapMin = std::max(body, 6.0) * 1.6;   // a gap wider than this splits cells

    size_t i = 0;
    while (i < lines.size()) {
        if (wantTables) {
            std::vector<Word> first = cellsOf(lines[i], gapMin);
            if (first.size() >= 2) {
                // Collect following lines whose cells line up with the first line's columns.
                std::vector<std::vector<Word>> rows{first};
                size_t j = i + 1;
                while (j < lines.size()) {
                    std::vector<Word> c = cellsOf(lines[j], gapMin);
                    if (c.size() < 2) break;
                    // each cell must start near a column start of the first row (or the row has the same count)
                    bool aligned = c.size() == first.size();
                    if (aligned)
                        for (size_t k = 0; k < c.size(); ++k)
                            if (std::fabs(c[k].x0 - first[k].x0) > body * 2.5 && std::fabs(c[k].x1 - first[k].x1) > body * 2.5) aligned = false;
                    if (!aligned) break;
                    rows.push_back(std::move(c));
                    ++j;
                }
                if (rows.size() >= 2) {
                    Block b; b.table = true;
                    for (auto& r : rows) {
                        std::vector<std::wstring> row;
                        for (auto& c : r) row.push_back(c.text);
                        b.rows.push_back(std::move(row));
                    }
                    blocks.push_back(std::move(b));
                    i = j;
                    continue;
                }
            }
        }
        if (blocks.empty() || blocks.back().table) blocks.push_back(Block{});
        blocks.back().lines.push_back(lines[i]);
        ++i;
    }
    return blocks;
}

// Paragraph assembly: lines belong together unless there is a heading, bullet, or a vertical gap.
struct Para { int heading = 0; bool bullet = false; std::wstring text; bool bold = false; };

std::vector<Para> paragraphs(const std::vector<Line>& lines, double body) {
    std::vector<Para> out;
    const Line* prev = nullptr;
    for (const Line& l : lines) {
        std::wstring rest;
        int h = headingLevel(l, body);
        bool bullet = h == 0 && isBullet(l.text, rest);
        bool newPara = true;
        if (prev && h == 0 && !bullet && !out.empty() && out.back().heading == 0 && !out.back().bullet) {
            const double lineH = std::max(prev->top - prev->bottom, 1.0);
            const double gap = prev->bottom - l.top;          // positive when there is white space between lines
            newPara = gap > lineH * 0.7 || std::fabs(l.size - prev->size) > 0.6;
        } else if (prev && h == 0 && !bullet && !out.empty() && out.back().bullet) {
            // wrapped continuation of a bullet item (indented relative to the bullet text)
            const double gap = prev->bottom - l.top;
            newPara = gap > std::max(prev->top - prev->bottom, 1.0) * 0.7;
        }
        if (newPara) {
            Para p;
            p.heading = h; p.bullet = bullet; p.text = bullet ? rest : l.text; p.bold = l.bold;
            out.push_back(std::move(p));
        } else {
            if (!out.back().text.empty() && out.back().text.back() == L'-' ) out.back().text.pop_back();   // hyphenated line break
            else out.back().text += L' ';
            out.back().text += l.text;
        }
        prev = &l;
    }
    return out;
}

// ---------- writers ----------

std::wstring escapeMd(const std::wstring& s) {
    std::wstring o;
    for (wchar_t c : s) {
        if (c == L'*' || c == L'_' || c == L'`' || c == L'|' || c == L'#') o += L'\\';
        o += c;
    }
    return o;
}

std::string writeText(Document& d, const std::vector<int>& pages) {
    std::wstring all;
    for (size_t k = 0; k < pages.size(); ++k) {
        std::vector<Line> lines = pageLines(d, pages[k]);
        for (const Line& l : lines) all += l.text + L"\r\n";
        if (k + 1 < pages.size()) all += L"\r\n";
    }
    return std::string("\xEF\xBB\xBF") + utf8(all);
}

std::string writeMarkdown(Document& d, const std::vector<int>& pages) {
    std::wstring all;
    for (size_t k = 0; k < pages.size(); ++k) {
        std::vector<Block> blocks = pageBlocks(d, pages[k], true);
        for (const Block& b : blocks) {
            if (b.table) {
                for (size_t r = 0; r < b.rows.size(); ++r) {
                    all += L"|";
                    for (auto& c : b.rows[r]) all += L" " + escapeMd(c) + L" |";
                    all += L"\r\n";
                    if (r == 0) { all += L"|"; for (size_t c = 0; c < b.rows[0].size(); ++c) all += L" --- |"; all += L"\r\n"; }
                }
                all += L"\r\n";
            } else {
                auto lines = b.lines;
                for (const Para& p : paragraphs(lines, bodySize(lines))) {
                    if (p.heading > 0) all += std::wstring(static_cast<size_t>(std::min(p.heading, 4)), L'#') + L" " + escapeMd(p.text) + L"\r\n\r\n";
                    else if (p.bullet) all += L"- " + escapeMd(p.text) + L"\r\n";
                    else all += escapeMd(p.text) + L"\r\n\r\n";
                }
                if (!all.empty() && all.substr(all.size() - 2) != L"\r\n\r\n") all += L"\r\n";
            }
        }
        if (k + 1 < pages.size()) all += L"---\r\n\r\n";
    }
    return std::string("\xEF\xBB\xBF") + utf8(all);
}

std::string run(const std::wstring& t, bool bold, bool italic = false) {
    std::string r = "<w:r>";
    if (bold || italic) r += std::string("<w:rPr>") + (bold ? "<w:b/>" : "") + (italic ? "<w:i/>" : "") + "</w:rPr>";
    r += "<w:t xml:space=\"preserve\">" + xmlEscape(t) + "</w:t></w:r>";
    return r;
}

std::string writeDocx(Document& d, const std::vector<int>& pages) {
    std::string body;
    for (size_t k = 0; k < pages.size(); ++k) {
        for (const Block& b : pageBlocks(d, pages[k], true)) {
            if (b.table) {
                body += "<w:tbl><w:tblPr><w:tblStyle w:val=\"TableGrid\"/><w:tblW w:w=\"0\" w:type=\"auto\"/></w:tblPr><w:tblGrid>";
                for (size_t c = 0; c < b.rows[0].size(); ++c) body += "<w:gridCol/>";
                body += "</w:tblGrid>";
                for (auto& row : b.rows) {
                    body += "<w:tr>";
                    for (auto& cell : row) body += "<w:tc><w:p>" + run(cell, false) + "</w:p></w:tc>";
                    body += "</w:tr>";
                }
                body += "</w:tbl><w:p/>";
            } else {
                for (const Para& p : paragraphs(b.lines, bodySize(b.lines))) {
                    body += "<w:p>";
                    if (p.heading > 0) body += "<w:pPr><w:pStyle w:val=\"Heading" + std::to_string(std::min(p.heading, 3)) + "\"/></w:pPr>" + run(p.text, false);
                    else if (p.bullet) body += "<w:pPr><w:pStyle w:val=\"ListParagraph\"/><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr>" + run(p.text, false);
                    else body += run(p.text, p.bold);
                    body += "</w:p>";
                }
            }
        }
        if (k + 1 < pages.size()) body += "<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>";
    }
    const std::string ns = "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"";
    Zip z;
    z.add("[Content_Types].xml",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
          "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/>"
          "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
          "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
          "<Override PartName=\"/word/numbering.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml\"/></Types>");
    z.add("_rels/.rels",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
          "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>");
    z.add("word/_rels/document.xml.rels",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
          "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
          "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering\" Target=\"numbering.xml\"/></Relationships>");
    z.add("word/document.xml", "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:document " + ns + "><w:body>" + body +
          "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/><w:pgMar w:top=\"1134\" w:right=\"1134\" w:bottom=\"1134\" w:left=\"1134\"/></w:sectPr></w:body></w:document>");
    z.add("word/styles.xml",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:styles " + ns + ">"
          "<w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii=\"Calibri\" w:hAnsi=\"Calibri\"/><w:sz w:val=\"22\"/></w:rPr></w:rPrDefault>"
          "<w:pPrDefault><w:pPr><w:spacing w:after=\"120\" w:line=\"264\" w:lineRule=\"auto\"/></w:pPr></w:pPrDefault></w:docDefaults>"
          "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/></w:style>"
          "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/><w:basedOn w:val=\"Normal\"/><w:next w:val=\"Normal\"/><w:pPr><w:keepNext/><w:spacing w:before=\"360\" w:after=\"120\"/><w:outlineLvl w:val=\"0\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"40\"/></w:rPr></w:style>"
          "<w:style w:type=\"paragraph\" w:styleId=\"Heading2\"><w:name w:val=\"heading 2\"/><w:basedOn w:val=\"Normal\"/><w:next w:val=\"Normal\"/><w:pPr><w:keepNext/><w:spacing w:before=\"280\" w:after=\"100\"/><w:outlineLvl w:val=\"1\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"32\"/></w:rPr></w:style>"
          "<w:style w:type=\"paragraph\" w:styleId=\"Heading3\"><w:name w:val=\"heading 3\"/><w:basedOn w:val=\"Normal\"/><w:next w:val=\"Normal\"/><w:pPr><w:keepNext/><w:spacing w:before=\"200\" w:after=\"80\"/><w:outlineLvl w:val=\"2\"/></w:pPr><w:rPr><w:b/><w:sz w:val=\"26\"/></w:rPr></w:style>"
          "<w:style w:type=\"paragraph\" w:styleId=\"ListParagraph\"><w:name w:val=\"List Paragraph\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:ind w:left=\"720\"/></w:pPr></w:style>"
          "<w:style w:type=\"table\" w:styleId=\"TableGrid\"><w:name w:val=\"Table Grid\"/><w:tblPr><w:tblBorders>"
          "<w:top w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"808080\"/><w:left w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"808080\"/>"
          "<w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"808080\"/><w:right w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"808080\"/>"
          "<w:insideH w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"808080\"/><w:insideV w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"808080\"/>"
          "</w:tblBorders><w:tblCellMar><w:left w:w=\"80\" w:type=\"dxa\"/><w:right w:w=\"80\" w:type=\"dxa\"/></w:tblCellMar></w:tblPr></w:style></w:styles>");
    z.add("word/numbering.xml",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><w:numbering " + ns + ">"
          "<w:abstractNum w:abstractNumId=\"0\"><w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/><w:lvlText w:val=\"\xE2\x80\xA2\"/><w:lvlJc w:val=\"left\"/>"
          "<w:pPr><w:ind w:left=\"720\" w:hanging=\"360\"/></w:pPr></w:lvl></w:abstractNum><w:num w:numId=\"1\"><w:abstractNumId w:val=\"0\"/></w:num></w:numbering>");
    return z.finish();
}

bool isNumber(const std::wstring& s, double& v) {
    std::wstring t;
    for (wchar_t c : s) if (c != L',' && c != L' ' && c != 0xA0) t += c;
    if (t.empty()) return false;
    wchar_t* end = nullptr;
    v = wcstod(t.c_str(), &end);
    return end && *end == 0;
}

std::string colName(int c) {
    std::string s;
    for (c++; c > 0; c = (c - 1) / 26) s.insert(s.begin(), static_cast<char>('A' + (c - 1) % 26));
    return s;
}

std::string writeXlsx(const std::vector<Table>& tables) {
    Zip z;
    std::string wbSheets, wbRels, ct;
    for (size_t t = 0; t < tables.size(); ++t) {
        std::string rows;
        for (size_t r = 0; r < tables[t].rows.size(); ++r) {
            rows += "<row r=\"" + std::to_string(r + 1) + "\">";
            for (size_t c = 0; c < tables[t].rows[r].size(); ++c) {
                const std::wstring& txt = tables[t].rows[r][c];
                const std::string ref = colName(static_cast<int>(c)) + std::to_string(r + 1);
                double v;
                if (isNumber(txt, v) && std::isfinite(v)) {
                    char b[64]; snprintf(b, sizeof b, "%.15g", v);
                    rows += "<c r=\"" + ref + "\"><v>" + b + "</v></c>";
                } else rows += "<c r=\"" + ref + "\" t=\"inlineStr\"><is><t xml:space=\"preserve\">" + xmlEscape(txt) + "</t></is></c>";
            }
            rows += "</row>";
        }
        const std::string n = std::to_string(t + 1);
        z.add("xl/worksheets/sheet" + n + ".xml",
              "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>" + rows + "</sheetData></worksheet>");
        wbSheets += "<sheet name=\"Table " + n + " (page " + std::to_string(tables[t].page + 1) + ")\" sheetId=\"" + n + "\" r:id=\"rId" + n + "\"/>";
        wbRels += "<Relationship Id=\"rId" + n + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet" + n + ".xml\"/>";
        ct += "<Override PartName=\"/xl/worksheets/sheet" + n + ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
    }
    z.add("[Content_Types].xml",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
          "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/><Default Extension=\"xml\" ContentType=\"application/xml\"/>"
          "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>" + ct + "</Types>");
    z.add("_rels/.rels",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
          "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>");
    z.add("xl/workbook.xml",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
          "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>" + wbSheets + "</sheets></workbook>");
    z.add("xl/_rels/workbook.xml.rels",
          "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">" + wbRels + "</Relationships>");
    return z.finish();
}

std::wstring csvCell(const std::wstring& s) {
    bool q = false;
    for (wchar_t c : s) if (c == L',' || c == L'"' || c == L'\n' || c == L'\r' || c == L';') q = true;
    if (!q) return s;
    std::wstring o = L"\"";
    for (wchar_t c : s) { if (c == L'"') o += L"\"\""; else o += c; }
    return o + L"\"";
}

std::string writeCsv(const std::vector<Table>& tables) {
    std::wstring all;
    for (size_t t = 0; t < tables.size(); ++t) {
        for (auto& row : tables[t].rows) {
            for (size_t c = 0; c < row.size(); ++c) { if (c) all += L","; all += csvCell(row[c]); }
            all += L"\r\n";
        }
        if (t + 1 < tables.size()) all += L"\r\n";
    }
    return std::string("\xEF\xBB\xBF") + utf8(all);
}

// ---------- pictures (GDI+) ----------

bool encoderFor(const wchar_t* mime, CLSID& out) {
    UINT n = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&n, &size);
    if (!size) return false;
    std::vector<unsigned char> buf(size);
    auto* info = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    Gdiplus::GetImageEncoders(n, size, info);
    for (UINT i = 0; i < n; ++i) if (wcscmp(info[i].MimeType, mime) == 0) { out = info[i].Clsid; return true; }
    return false;
}

} // namespace

const wchar_t* extension(Format f) {
    switch (f) {
        case Format::Png: return L"png"; case Format::Jpeg: return L"jpg"; case Format::Text: return L"txt";
        case Format::Markdown: return L"md"; case Format::Word: return L"docx"; case Format::ExcelXlsx: return L"xlsx"; default: return L"csv";
    }
}

std::vector<Table> findTables(Document& d, const std::vector<int>& pages) {
    std::vector<Table> out;
    for (int p : pages)
        for (const Block& b : pageBlocks(d, p, true))
            if (b.table) { Table t; t.page = p; t.rows = b.rows; out.push_back(std::move(t)); }
    return out;
}

Result run(Document& d, const std::vector<int>& pages, Format f, const std::wstring& path, int dpi) {
    Result res;
    if (pages.empty()) { res.error = L"There are no pages to export."; return res; }

    if (f == Format::Png || f == Format::Jpeg) {
        Gdiplus::GdiplusStartupInput in;
        ULONG_PTR token = 0;
        if (Gdiplus::GdiplusStartup(&token, &in, nullptr) != Gdiplus::Ok) { res.error = L"The picture encoder could not be started."; return res; }
        CLSID clsid;
        if (!encoderFor(f == Format::Png ? L"image/png" : L"image/jpeg", clsid)) {
            Gdiplus::GdiplusShutdown(token);
            res.error = L"This PC has no encoder for that picture format.";
            return res;
        }
        std::wstring base = path;
        size_t dot = base.find_last_of(L'.');
        if (dot != std::wstring::npos && dot > base.find_last_of(L"\\/")) base.resize(dot);
        const auto& api = pdfium::api();
        for (int p : pages) {
            const PageSize& ps = d.pageSize(p);
            int w = std::max(1, static_cast<int>(std::lround(ps.w * dpi / 72.0)));
            int h = std::max(1, static_cast<int>(std::lround(ps.h * dpi / 72.0)));
            if (1LL * w * h > 120000000LL) { res.error = L"Page " + std::to_wstring(p + 1) + L" could not be made into a picture. Try a lower resolution."; break; }
            std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
            FPDF_PAGE pg = d.page(p);
            FPDF_BITMAP fb = api.FPDFBitmap_CreateEx(w, h, FPDFBitmap_BGRx, px.data(), w * 4);
            if (!pg || !fb) { res.error = L"Page " + std::to_wstring(p + 1) + L" could not be rendered."; break; }
            api.FPDFBitmap_FillRect(fb, 0, 0, w, h, 0xFFFFFFFF);
            api.FPDF_RenderPageBitmap(fb, pg, 0, 0, w, h, 0, FPDF_ANNOT);
            api.FPDFBitmap_Destroy(fb);
            Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppRGB, px.data());
            std::wstring file = base + L"-" + std::to_wstring(p + 1) + L"." + extension(f);
            Gdiplus::EncoderParameters params{};
            ULONG quality = 90;
            const Gdiplus::EncoderParameters* pp = nullptr;
            if (f == Format::Jpeg) {
                params.Count = 1;
                params.Parameter[0].Guid = Gdiplus::EncoderQuality;
                params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
                params.Parameter[0].NumberOfValues = 1;
                params.Parameter[0].Value = &quality;
                pp = &params;
            }
            if (bmp.Save(file.c_str(), &clsid, pp) != Gdiplus::Ok) {
                res.error = L"The picture \"" + file.substr(file.find_last_of(L"\\/") + 1) + L"\" could not be saved. The folder may be read-only, or the disk may be full.";
                break;
            }
            if (res.items == 0) res.firstFile = file;
            ++res.items;
        }
        Gdiplus::GdiplusShutdown(token);
        res.ok = res.error.empty() && res.items > 0;
        return res;
    }

    std::string data;
    if (f == Format::Text || f == Format::Markdown || f == Format::Word) {
        bool any = false;
        for (int p : pages) if (d.charCount(p) > 0) { any = true; break; }
        if (!any) {
            res.error = L"No text was found on these pages. If they are scanned, they are pictures without text, so there is nothing to export as text.";
            return res;
        }
        data = f == Format::Text ? writeText(d, pages) : f == Format::Markdown ? writeMarkdown(d, pages) : writeDocx(d, pages);
        res.items = static_cast<int>(pages.size());
    } else {
        std::vector<Table> tables = findTables(d, pages);
        if (tables.empty()) {
            res.error = L"No tables were found on these pages. Tables are found where text lines up in rows and columns.";
            return res;
        }
        data = f == Format::ExcelXlsx ? writeXlsx(tables) : writeCsv(tables);
        res.items = static_cast<int>(tables.size());
    }
    if (!writeFileBytes(path, data, res.error)) return res;
    res.firstFile = path;
    res.ok = true;
    return res;
}

} // namespace exporter
