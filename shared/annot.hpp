#pragma once
// Annotation operations on a Document. Every function that changes the document calls
// Document::beginChange() first, so each one is a single undo step.
#include "platform.hpp"
#include <string>
#include <vector>
#include "document.hpp"

namespace annot {

struct PointD { double x, y; };                       // page coordinates, in points (origin bottom-left)
struct RectD { double left, bottom, right, top; };

enum class Markup { Highlight, Underline, Strikethrough, Squiggly };
enum class Shape { Rectangle, Ellipse };

struct Info {
    int index = -1;
    int subtype = 0;                                  // FPDF_ANNOT_*
    RectD rect{};
    std::wstring contents;
    bool callout = false;                             // a callout made here: its text can be changed (setCalloutText)
    bool reply = false;                               // a reply to another comment, or its review status (not on the page)
};

// Highlight / underline / strikethrough / squiggly over text rectangles (page coordinates).
bool addMarkup(Document& d, int page, Markup kind, const std::vector<RectD>& rects, COLORREF color,
               const std::wstring& comment = L"");
bool addShape(Document& d, int page, Shape kind, const RectD& r, COLORREF color, double width);
// Lines, arrows and freehand drawing are ink annotations: one stroke per entry.
bool addInk(Document& d, int page, const std::vector<std::vector<PointD>>& strokes, COLORREF color, double width);
// One stroke of a pen with pressure (0..1 for each point): thinner where it pressed lightly, up to `width` where it
// pressed hard. The stroke itself is kept for other readers; its look follows the pressure.
bool addPressureInk(Document& d, int page, const std::vector<PointD>& points, const std::vector<double>& pressure, COLORREF color, double width);
// The width drawn for a pressure (0..1) with a pen of `width`.
double pressureWidth(double pressure, double width);
bool addNote(Document& d, int page, PointD at, const std::wstring& text, COLORREF color);
// A closed shape through the points (drawn as one ink stroke, so every viewer shows it).
bool addPolygon(Document& d, int page, const std::vector<PointD>& pts, COLORREF color, double width);
// A "cloud" drawn around a rectangle (for marking an area on a drawing).
bool addCloud(Document& d, int page, const RectD& r, COLORREF color, double width);
// A text box with a leader line and arrowhead pointing at `tip`; the box sits next to `at`.
bool addCallout(Document& d, int page, PointD tip, PointD at, const std::wstring& text, COLORREF color);
// Draws the callout again with new text, in the same place and colour (it moves to the top of the other annotations).
bool setCalloutText(Document& d, int page, int index, const std::wstring& text);
// A clickable box that opens a web page.
bool addLink(Document& d, int page, const RectD& r, const std::string& uri);

std::vector<Info> list(Document& d, int page);
// Topmost annotation whose rectangle contains the point; -1 if none. Links and popups are ignored.
int hitTest(Document& d, int page, PointD p);
bool remove(Document& d, int page, int index);
bool setContents(Document& d, int page, int index, const std::wstring& text);
const wchar_t* typeName(int subtype);
bool hasComment(int subtype);                         // can carry a comment (note, markup)

// A Markdown summary of every comment and highlight in the document (empty when there are none).
std::wstring notesSummary(Document& d);

} // namespace annot
