#pragma once
// Creates form fields (text boxes, check boxes, drop-down lists) in a PDF written by PDFium. PDFium cannot create
// fields, so they are appended as an incremental update.
#include <string>
#include <vector>

namespace formcreate {

struct Spec {
    enum Kind { Text, Check, Combo } kind = Text;
    int page = 0;                        // 0-based
    double left = 0, bottom = 0, right = 0, top = 0;   // page coordinates
    std::wstring name;
    std::vector<std::wstring> options;   // Combo
    bool multiline = false;              // Text
};

bool add(const std::vector<unsigned char>& pdf, const std::vector<Spec>& specs, std::vector<unsigned char>& out, std::wstring& error);

} // namespace formcreate
