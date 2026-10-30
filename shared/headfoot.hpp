#pragma once
// Delete header and footer: finds what repeats in the top and bottom strips of the pages (running titles, dates, page
// numbers, rules, logos; on odd and even pages separately too), and removes it from the pages for good: the objects
// are taken out of the page (not covered), so their text can no longer be found or copied. On scanned pages the area
// of the picture is made white and the recognised text in it is removed. Nothing is removed on a guess: only elements
// that repeat (or are marked as header / footer in a tagged PDF), and areas the user draws.
#include <string>
#include <vector>
#include "document.hpp"

namespace headfoot {

enum class Band { Header, Footer };
enum class Kind { PageNumber, Date, Text, Line, Picture, Shape, Group, Area };

// A rectangle on the page as it is shown: x from the left, y from the top, in points.
struct Box { double l = 0, t = 0, r = 0, b = 0; };

// One piece of a header or footer on one page.
struct Item {
    int page = 0;
    std::vector<int> path;    // the object's index on the page, or {form object's index, index inside the form}
    Box box;                  // where it is (shown page)
    bool scanned = false;     // recognised text on a scanned page: the picture is whitened under it as well
    std::wstring text;
};

// Something that repeats: one line in the list (it can be left out).
struct Element {
    Kind kind = Kind::Text;
    Band band = Band::Header;
    std::wstring label;       // "Page number (Page 3 of 6)"
    std::vector<Item> items;
    Box area;                 // Kind::Area: the drawn rectangle (on every chosen page, measured from the top or the bottom)
    bool fromBottom = false;  // Kind::Area: `area` is kept at the same distance from the bottom of each page
    double refHeight = 0;     // Kind::Area: the height of the page it was drawn on
    bool selected = true;
    bool locked = false;      // cannot be removed on its own (it is part of a page template that also holds body content)
    int pageCount() const;
};

struct Detection {
    std::vector<Element> elements;
    std::vector<char> scannedPage;    // a page that is one picture (with recognised text, if any)
    std::vector<char> scannedNoText;  // ... without recognised text: its header and footer cannot be found
    int pages = 0;
};

// Looks at every page of `d`.
Detection detect(Document& d);

// What to delete: 0 header only, 1 footer only, 2 both, 3 page numbers only, 4 the elements ticked one by one (left as
// they are). Ticks the elements that match and unticks the others (drawn areas stay ticked).
void selectFor(Detection& det, int what);

// The pages a choice means. `which`: 0 all, 1 current, 2 odd, 3 even, 4 the list in `spec` ("2,5,8", "3-15", "1-3, 7").
std::vector<int> choosePages(const Document& d, int which, int current, const std::wstring& spec, std::wstring* why);

// What will be removed from one page.
struct PagePlan {
    int page = 0;
    std::vector<std::vector<int>> objects;   // paths, as in Item
    std::vector<Box> whiten;                 // areas of a scanned page's picture
    int crossing = 0;                        // objects across the edge of a drawn area (kept)
    int sharedKept = 0;                      // pieces of a page template (kept)
    int bodyTaken = 0;                       // objects inside a drawn area that are not part of a header or footer found
    std::wstring bodySample;                 // the text of one of them, for the warning
};
struct Plan {
    std::vector<PagePlan> pages;
    int objects = 0, areas = 0, crossing = 0, sharedKept = 0, bodyTaken = 0;
    std::wstring bodySample;
};

// The plan for the selected elements on the chosen pages.
Plan plan(Document& d, const Detection& det, const std::vector<int>& pages);

// Carries out the plan on `d` as one undoable change. False (and `why`) when it could not be done; nothing changes then.
bool apply(Document& d, const Plan& p, std::wstring* why);

// Carries out one page's plan on page `index` of `doc` (the preview uses a copy of the page).
bool applyPage(Document& d, int index, const PagePlan& pp);

// "Page numbers", "Header text"...
std::wstring kindName(Kind k);

} // namespace headfoot
