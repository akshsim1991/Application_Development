#pragma once
// Editing text where it is on the page: an edit box laid over the line or paragraph, in a matching font, size and
// colour, with a small style bar for paragraphs. Enter (a line) or Ctrl+Enter (a paragraph) applies, Esc cancels.
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

namespace inlineedit {

struct Style {
    int family = 0;              // 0 sans-serif, 1 serif, 2 monospaced
    bool bold = false, italic = false;
    double sizePt = 12;          // as drawn on the page
    COLORREF color = 0;
    int align = 0;               // 0 left, 1 centre, 2 right, 3 justified (shown as left)
};

struct Spec {
    std::wstring text;
    bool paragraph = false;      // several lines, with the style bar
    Style style;
    std::vector<COLORREF> palette;           // colours offered in the style bar
    std::vector<std::wstring> paletteNames;
    POINT click{-1, -1};         // where the text was clicked (viewer client coordinates): the caret starts there
    std::string pdfFont;         // the document's name for the font: the edit box uses that font when it is installed
    bool baselineBox = false;    // the located box starts where the text starts, with its top on the first baseline
    double wrapPt = 0;           // paragraphs: the width the lines are wrapped at, in points (0: the box's width)
    // The width of a text (start to the last letter's right edge, in points) as it will be set on the page in a style.
    // The edit box's font is matched to it, so the box breaks lines where the page will.
    std::function<double(const std::wstring& text, const Style& style)> measure;
    // When set, the bar has an "Equation" button: the editor closes and this opens the equation editor for the text.
    std::function<void()> onEquation;
};

class Editor {
public:
    // `locate` gives the box on screen (viewer client coordinates) and the pixels per point; false hides the editor.
    using Locate = std::function<bool(RECT& box, double& pxPerPt)>;
    // `commit` applies the text; returning false keeps the editor open (the reason was already shown).
    using Commit = std::function<bool(const std::wstring& text, const Style& style)>;

    void create(HWND viewer, HINSTANCE inst);
    bool active() const { return wnd_ != nullptr; }
    void start(const Spec& spec, Locate locate, Commit commit);
    bool commit();       // true when the editor is closed afterwards
    void cancel();
    void reposition();   // after scrolling or zooming

private:
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK editProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(HWND, UINT, WPARAM, LPARAM);
    void close();
    void makeEdit();
    void applyFont();
    void readBar();
    void layout();
    void script(int kind);   // the x² (1) and x₂ (-1) buttons

    HWND viewer_ = nullptr, wnd_ = nullptr, edit_ = nullptr;
    HINSTANCE inst_ = nullptr;
    HWND famBox_ = nullptr, sizeBox_ = nullptr, boldBtn_ = nullptr, italBtn_ = nullptr, colBox_ = nullptr, alignBox_ = nullptr, okBtn_ = nullptr, noBtn_ = nullptr, hint_ = nullptr;
    HWND supBtn_ = nullptr, subBtn_ = nullptr, eqBtn_ = nullptr;
    int scriptMode_ = 0;         // typing superscripts (1) or subscripts (-1)
    HFONT uiFont_ = nullptr, textFont_ = nullptr;
    HBRUSH editBrush_ = nullptr;
    WNDPROC oldEdit_ = nullptr;
    Spec spec_;
    Style style_;
    Locate locate_;
    Commit commit_;
    RECT box_{};
    double ppp_ = 1;
    double fit_ = 1;             // edit box font width / page font width, what is left after matching
    int barH_ = 0, barW_ = 0;
    bool closing_ = false;
};

} // namespace inlineedit
