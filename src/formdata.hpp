#pragma once
// Reading and filling the fields of a PDF form, and finding the places where a flat (paper-style) form wants answers.
#include <map>
#include <string>
#include <vector>
#include "document.hpp"
#include "formcreate.hpp"

namespace formdata {

struct Field {
    std::wstring name, value, exportValue;
    int type = 0;                // FPDF_FORMFIELD_*
    bool checked = false;
    int page = 0;
};

std::vector<Field> list(Document& d);

// A table of the answers: a row of field names and one row of values (check boxes give their export value when ticked).
std::vector<std::vector<std::wstring>> answers(Document& d);

// Types the values into the fields with those names (check boxes and radio buttons: a true-ish value ticks them).
// Returns how many fields were changed.
int setValues(Document& d, const std::map<std::wstring, std::wstring>& values);

std::vector<std::vector<std::wstring>> parseCsv(const std::wstring& text);

// Places in a flat form that look like blanks to fill in: runs of underscores, rules after a "label:" and small squares.
std::vector<formcreate::Spec> detect(Document& d);

} // namespace formdata
