#pragma once
// Tables found on any page, edited as a grid and drawn again in their place.
//
// Detection looks at what is drawn: ruled tables from their lines and cell boxes (merged cells where a line between two
// cells is missing), tables with only horizontal rules or none from text that lines up in columns, and tables on scanned
// pages from the lines in the picture plus the recognised (OCR) text. Writing removes the table's own objects and draws the
// edited table in the same place: lines, cell colours and the cells' text in the table's own fonts where they have the
// letters. One undo step.
#include "platform.hpp"
#include <string>
#include <vector>
#include "document.hpp"
#include "pageobjects.hpp"

namespace tables {

constexpr COLORREF kNoFill = 0xFFFFFFFF;

enum class Borders { All, Outside, Rows, None };   // presets: every line; the frame only; horizontal lines only; no lines

struct Cell {
    std::wstring text;                // lines separated by '\n'
    int rowSpan = 1, colSpan = 1;     // on the first cell of a merged block
    bool covered = false;             // inside a merged block, not its first cell
    double size = 10;                 // points
    pageobj::Family family = pageobj::Family::Sans;
    bool bold = false, italic = false;
    COLORREF color = RGB(0, 0, 0);
    COLORREF fill = kNoFill;          // background
    int align = 0;                    // 0 left, 1 centre, 2 right, 3 justified
    int valign = 1;                   // 0 top, 1 middle, 2 bottom
    int like = -1;                    // a text object of the cell when found (its font is used again), else -1
    double baseOff = -1;              // from the cell's top to its first baseline as found (kept while the text fits as it did)
    int lines = 0;                    // its number of lines as found
    // as it was found on the page: a cell that is not changed keeps its own text objects (fonts, letters, places), moved
    // only when its row or column moves
    std::vector<int> objs;            // its text objects (page indices)
    bool sharedObjs = false;          // one of them also holds text of another cell (then it is set again when either changes)
    bool found = false;               // read from the page (the fields below are set)
    std::wstring origText;
    double origSize = 0;
    pageobj::Family origFamily = pageobj::Family::Sans;
    bool origBold = false, origItalic = false;
    COLORREF origColor = 0;
    int origAlign = 0, origValign = 1;
    double fL = 0, fR = 0, fT = 0, fB = 0;    // its block's edges as found
    int fRowSpan = 1, fColSpan = 1;
    COLORREF fFill = 0xFFFFFFFF;
    double textW = 0;                         // the width its text took
};

struct Table {
    int page = 0;
    std::vector<double> colX;         // column edges, left to right, page coordinates (cols + 1)
    std::vector<double> rowY;         // row edges, top to bottom: y decreases (rows + 1)
    std::vector<Cell> cells;          // rows * cols, row by row
    std::vector<char> hRule, vRule;   // a line along each row edge (rows + 1) and column edge (cols + 1); none inside merged cells
    double lineWidth = 0.75;
    COLORREF lineColor = RGB(0, 0, 0);
    // the pieces of line as found (each with its own width and colour: a thick coloured rule round one row stays so),
    // and for each row and column edge the edge it was found as (-1: added since)
    struct Piece { bool horizontal; double at, a, b, width; COLORREF colour; };
    std::vector<Piece> pieces;
    std::vector<int> hOrig, vOrig;
    // the grid as found: while it is the same, the table's own line drawings are kept as they are
    std::vector<double> foundColX, foundRowY;
    std::vector<char> foundH, foundV;
    double pad = 3;                   // space between a cell's edge and its text, points
    bool scanned = false;             // part of a picture: drawn again over a white box
    std::vector<int> objects;         // the page objects that make up the table (removed when it is drawn again)
    std::vector<int> sharedPaths;     // of those, line drawings that also draw outside the table: only their part inside goes
    double foundL = 0, foundB = 0, foundR = 0, foundT = 0;   // where it was found (edits move the edges above)
    double left() const { return colX.front(); }
    double right() const { return colX.back(); }
    double top() const { return rowY.front(); }
    double bottom() const { return rowY.back(); }
    int rows() const { return static_cast<int>(rowY.size()) - 1; }
    int cols() const { return static_cast<int>(colX.size()) - 1; }
    Cell& at(int r, int c) { return cells[static_cast<size_t>(r) * cols() + c]; }
    const Cell& at(int r, int c) const { return cells[static_cast<size_t>(r) * cols() + c]; }
};

// Every table on the page, largest first.
std::vector<Table> detect(Document& d, int page);
// The table around a page point. False when there is none (`why` says so, or why it cannot be edited).
// Text in form objects on the page is first put on the page itself (see unwrapTextForms).
bool tableAt(Document& d, int page, double x, double y, Table& out, std::wstring* why = nullptr);
bool unwrapTextForms(Document& d, int page);

// The table in a box drawn on the page (page coordinates): a table found there, else the text inside read as rows (its lines)
// and columns (where it lines up). For tables the automatic search misses.
bool tableIn(Document& d, int page, double l, double b, double r, double t, Table& out, std::wstring* why = nullptr);

// ---- changes to the grid (they keep the table's outer size unless they say otherwise) ----
void setBorders(Table& t, Borders b);
Borders bordersOf(const Table& t);                // the preset the lines match (All when they match none exactly)
// The first cell of the merged block that covers (r, c).
void anchorOf(const Table& t, int r, int c, int& ar, int& ac);
void insertRow(Table& t, int at);                 // a row before row `at` (rows() for the end), as tall as its neighbour; the table grows
void deleteRow(Table& t, int r);                  // the table shrinks
void insertColumn(Table& t, int at);              // a column before `at`; the columns share the old width
void deleteColumn(Table& t, int c);               // the others share its width
bool merge(Table& t, int r0, int c0, int r1, int c1);   // the texts are joined; false when the block cuts through another merge
void split(Table& t, int r, int c);              // undoes a merge
void setColumnWidth(Table& t, int c, double w);   // the table gets wider or narrower
void setRowHeight(Table& t, int r, double h);
void resize(Table& t, double width, double height);   // scales columns and rows
void moveTo(Table& t, double left, double top);

// Removes the table's objects and draws `t` in their place; one undo step. False, with the reason, when it could not be
// drawn (the document is then unchanged). Rows grow when their text needs more room (`grewBy`, points).
// `shrunk`: set when edited text had to be set smaller to fit on the page (there was no room below for the table to grow).
bool write(Document& d, const Table& t, std::wstring* why = nullptr, double* grewBy = nullptr, bool* shrunk = nullptr);
// Removes the table (on a scanned page it is covered with white). One undo step.
bool remove(Document& d, const Table& t);

} // namespace tables
