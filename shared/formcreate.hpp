#pragma once
// Creates form fields (text boxes, check boxes, drop-down lists) in a PDF written by PDFium. PDFium cannot create
// fields, so they are appended as an incremental update.
#include <string>
#include <vector>

namespace formcreate {

struct Spec {
    enum Kind { Text, Check, Combo, Radio, List, Date } kind = Text;
    int page = 0;                        // 0-based
    double left = 0, bottom = 0, right = 0, top = 0;   // page coordinates
    std::wstring name;
    std::vector<std::wstring> options;   // Combo, List; Radio: one button per option, stacked in the box
    bool multiline = false;              // Text
};

bool add(const std::vector<unsigned char>& pdf, const std::vector<Spec>& specs, std::vector<unsigned char>& out, std::wstring& error);

// Where the labels of a radio group go (they are drawn in the page, next to the buttons). Empty for other kinds.
struct Label { int page; double x, y, sizePt; std::wstring text; };
std::vector<Label> radioLabels(const Spec& sp);

// Turns every radio button group off (PDFium cannot clear a radio group through the form).
bool clearRadios(const std::vector<unsigned char>& pdf, std::vector<unsigned char>& out);

} // namespace formcreate
