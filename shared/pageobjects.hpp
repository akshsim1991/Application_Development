#pragma once
// Content added to the page itself: watermarks and page numbers (and text boxes).
#include "platform.hpp"
#include <string>
#include <vector>
#include <memory>
#include "document.hpp"

namespace pageobj {

struct WatermarkStyle {
    COLORREF color = RGB(0xE5, 0x39, 0x35);
    int opacityPercent = 30;
    int sizeMode = 0;          // 0 = fit the page, 1 = small, 2 = medium, 3 = large
    bool diagonal = true;
    bool behind = false;       // put it behind the page content
};

// True when every character can be drawn with the built-in Latin fonts.
bool isLatin(const std::wstring& text);

bool addWatermark(Document& d, const std::vector<int>& pages, const std::wstring& text, const WatermarkStyle& style);

struct NumberStyle {
    int format = 0;            // 0 "1", 1 "Page 1", 2 "Page 1 of N", 3 "1 / N"
    int position = 0;          // 0..5: bottom centre/left/right, top centre/left/right
    int startAt = 1;
    double sizePt = 10;
    COLORREF color = RGB(0x30, 0x30, 0x30);
};

bool addPageNumbers(Document& d, const std::vector<int>& pages, const NumberStyle& style);

// ---- text boxes and editing existing text ----

enum class Family { Sans, Serif, Mono };
struct TextStyle {
    Family family = Family::Sans;
    bool bold = false, italic = false;
    double sizePt = 14;
    COLORREF color = RGB(0x20, 0x20, 0x20);
};

// Adds `text` (several lines allowed) with its top-left corner at `at` (page coordinates).
bool addTextBox(Document& d, int page, double atX, double atY, const std::wstring& text, const TextStyle& st);

struct TextHit {
    int index = -1;             // page object index
    std::wstring text;
    double sizePt = 0;
    COLORREF color = 0;
    bool embeddedFont = false;
    std::string fontName;
    unsigned long flags = 0;
    double left = 0, bottom = 0, right = 0, top = 0;   // bounds on the page
    double shownSizePt = 0;                             // the size as drawn (font size times the object's scale)
    double originX = 0, baseline = 0;                   // where the text starts, on its baseline (page coordinates)
};

// The text object under a page point, preferring the smallest one. index == -1 when there is none.
TextHit textAt(Document& d, int page, double x, double y);
// Replaces the text of an existing object. Characters the original (embedded) font may lack are drawn in a
// similar standard font instead, at the same place and size. An empty `text` deletes the object.
bool replaceText(Document& d, int page, int objectIndex, const std::wstring& text, bool track = true);
// The width of `text` set in the font and size of a text object (from its start to the last letter's right edge, points).
double objectTextWidth(Document& d, int page, int objectIndex, const std::wstring& text);

// Replaces `find` by `with` inside every text object of the document (a match must lie within one object).
// One undo step. Returns the number of replacements; `pages` lists the pages that changed.
int replaceAll(Document& d, const std::wstring& find, const std::wstring& with, bool matchCase, std::vector<int>& pages);

// ---- paragraphs ----
enum class Align { Left, Centre, Right, Justify };
struct ParagraphHit {
    bool found = false;
    int page = 0;
    std::vector<int> objects;       // the text objects that make up the paragraph, lines top to bottom
    std::wstring text;              // the lines joined into running text
    double left = 0, top = 0, width = 0, lineHeight = 0;   // page coordinates / points
    TextStyle style;                // the closest standard font, size and colour
    int lines = 0;
    // Exact geometry, for setting the text again where it was: the first line's baseline starts at (originX, baseline);
    // lines are `lineStep` apart; text is wrapped at `wrapWidth` from the origin. Measured in the paragraph's own font.
    double originX = 0, baseline = 0, lineStep = 0, wrapWidth = 0;
    double firstIndent = 0, restIndent = 0;   // where the first line and the other lines start, from originX
    double sizeExact = 0;           // the size as drawn (style.sizePt is rounded to half a point)
    Align align = Align::Left;      // as the lines are set now
    bool upright = true;            // the text runs left to right, not turned
    std::string fontName;           // the PDF's name for that font
    std::vector<int> charObject;    // for each character of `text`, the page object it was drawn by (-1: a space between)
    // Text recognised on a scanned page (invisible, over the page's picture): edited, it is set visibly in the colour and
    // size of the words in the picture, over a patch of the picture's background (scanL..scanT) that hides the old words.
    double markerGap = 0;           // a list item's text starts this far from originX, apart from its number (a tab): 0 none
    bool overScan = false;
    COLORREF scanBg = RGB(255, 255, 255);
    double scanL = 0, scanB = 0, scanR = 0, scanT = 0;
};
// What can be edited on a page, for the outlines of edit mode: each paragraph (grouped as paragraphAt groups lines, so the
// outline is what a click there edits) and each picture. Page coordinates.
struct EditBlock {
    enum Kind { Text, Picture } kind = Text;
    double left = 0, bottom = 0, right = 0, top = 0;
};
std::vector<EditBlock> editableBlocks(Document& d, int page);
// The paragraph (consecutive lines with the same size and left edge) around a page point; with `lineOnly`, only the line
// there (every text object on that baseline that belongs to it).
ParagraphHit paragraphAt(Document& d, int page, double x, double y, bool lineOnly = false);
// Replaces the paragraph by `text` set in `style` inside the same width, starting on the same baseline. The paragraph's own
// font is kept when the family, weight and slant are unchanged; a word with letters it lacks (an embedded font may hold only
// the letters the document uses) is set in the closest standard font. When the paragraph gets longer, what is below it in
// the same column moves down. One undo step. False, with the reason in `why` when known, when the text cannot be set or does
// not fit on the page; the document is then unchanged.
// With `oneLine`, the text stays on one line however long it gets (line breaks become spaces).
bool replaceParagraph(Document& d, const ParagraphHit& hit, const std::wstring& text, const TextStyle& style, Align align,
                      std::wstring* why = nullptr, bool oneLine = false);
// The width of `text` on one line (from its start to the right edge of the last letter, in points) as replaceParagraph would
// set it in `style`. 0 when it cannot be set.
double paragraphTextWidth(Document& d, const ParagraphHit& hit, const std::wstring& text, const TextStyle& style);

// ---- more page tools ----
// Pages with no text and (almost) nothing drawn. Scanned pages count as blank when they are nearly white.
std::vector<int> blankPages(Document& d);
// Groups of pages that look the same (same text and same picture at a small size). Each group lists 2 or more pages.
std::vector<std::vector<int>> duplicatePages(Document& d);
struct HeaderFooter {
    std::wstring text[6];   // header left, centre, right, footer left, centre, right; {page} {pages} {date} {file} {title}
    double sizePt = 9;
    COLORREF color = RGB(0x40, 0x40, 0x40);
    int firstNumber = 1;
    double marginPt = 28;
};
bool addHeaderFooter(Document& d, const std::vector<int>& pages, const HeaderFooter& hf);
// A coloured rectangle behind everything on the page (it shows where the page itself is not painted).
bool addBackground(Document& d, const std::vector<int>& pages, COLORREF color);

// ---- picking up and moving page objects ----
struct GroupHit {
    bool found = false;
    enum Kind { Picture, Paragraph, Table, Shape } kind = Picture;   // Shape: a drawn line, box or other path
    std::vector<int> objects;
    double left = 0, bottom = 0, right = 0, top = 0;
};
// The picture, paragraph or table at a page point (a picture wins over text, text over a table).
GroupHit groupAt(Document& d, int page, double x, double y);
// When the text under a point is inside a form object (a page another program stamped or merged): its contents are put on
// the page itself so the text can be edited. True when that was done (one undo step); false when there is none or it could
// not be done exactly.
bool unwrapFormAt(Document& d, int page, double x, double y);
// Moves and scales the objects so the rectangle `from` becomes `to` (page coordinates). One undo step.
bool transformGroup(Document& d, int page, const std::vector<int>& objects, double fl, double fb, double fr, double ft, double tl, double tb, double tr, double tt);
bool deleteGroup(Document& d, int page, std::vector<int> objects);
// Lines that moved objects snap to: the page edges and centre, and the edges and centres of the other objects.
void snapGuides(Document& d, int page, const std::vector<int>& exclude, std::vector<double>& gx, std::vector<double>& gy);

// ---- tables drawn on the page (kept recognisable through a mark, so they can be edited or deleted later) ----
struct TableSpec {
    int rows = 3, cols = 3;
    double colWidth = 90, rowHeight = 24, fontSize = 11;   // points
    bool header = true;
    std::vector<std::wstring> cells;                        // rows * cols, row by row
};
struct TableHit { bool found = false; int page = 0; std::string id; TableSpec spec; double left = 0, top = 0; };   // left/top: page coordinates of the top-left corner
bool addTable(Document& d, int page, double atX, double atY, const TableSpec& spec, bool track = true);
TableHit tableAt(Document& d, int page, double x, double y);
bool removeTable(Document& d, int page, const std::string& id, bool track = true);

// ---- text set like a text object already on the page (table cells) ----
// The look of a text object of the page (its index): style and PDF font name, with `size` the size it is drawn at.
bool textObjectStyle(Document& d, int page, int object, TextStyle& style, std::string& fontName, double& size);
// Sets lines of text upright in `style`, in the font of the text object `like` where that font has every letter of a word
// (else the installed font of that name, else the closest standard font), or in a standard font when `like` is -1.
// Make it while a change is being made: a font may be embedded on first use.
class TextSetter {
public:
    TextSetter(Document& d, int page, int like, const TextStyle& style);
    ~TextSetter();
    TextSetter(const TextSetter&) = delete;
    TextSetter& operator=(const TextSetter&) = delete;
    bool ok() const;
    double width(const std::wstring& line);   // from the start to the right edge of the last letter; -1 when it cannot be set
    // The text objects for one line, its baseline starting at (x, y) in page coordinates; not yet on the page. Empty when
    // the line cannot be set.
    std::vector<FPDF_PAGEOBJECT> make(const std::wstring& line, double x, double y);
    // The same, justified: the words spread so that the line ends `width` from x.
    std::vector<FPDF_PAGEOBJECT> make(const std::wstring& line, double x, double y, double width);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Puts the baseline-left origin of a text object at (originX, originY) measured in points from the top-left of the
// page as displayed, stretching it horizontally by `sx`. Used for the invisible text layer of recognised text.
bool placeScaled(FPDF_PAGE page, FPDF_PAGEOBJECT obj, double visW, double visH, double originX, double originY, double sx);
// A text object with a standard font (for measuring or adding); the caller owns it until it is inserted.
FPDF_PAGEOBJECT newText(Document& d, const char* font, double size, const std::wstring& text, COLORREF color, int alphaPercent);

// A boxed stamp (for example APPROVED) centred on a page point, drawn in bold with a border of the same colour.
bool addStamp(Document& d, int page, double centreX, double centreY, const std::wstring& text, COLORREF color);

} // namespace pageobj
