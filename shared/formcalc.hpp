#pragma once
// Calculated form fields (a total, a product, an average ...) and checks on what may be typed into a field (a number,
// a range, a date, an e-mail address, a phone number). Written as Acrobat's standard form scripts (AFSimple_Calculate,
// AFNumber_*, AFRange_Validate, AFDate_*), so Acrobat and other readers do the same; this app evaluates them itself
// (its PDF engine runs no scripts).
#include <string>
#include <vector>
#include "document.hpp"

namespace formcalc {

enum class Op { Sum, Product, Average, Min, Max };
struct Calc {
    std::wstring field;                 // full field name
    Op op = Op::Sum;
    std::vector<std::wstring> sources;  // full field names
};

enum class Kind { None, Number, Range, Date, Email, Phone };
struct Rule {
    std::wstring field;
    Kind kind = Kind::None;
    int decimals = 2;                   // Number and Range: decimals shown
    bool hasMin = false, hasMax = false;
    double min = 0, max = 0;            // Range
    std::wstring dateFormat = L"dd/mm/yyyy";
};

const wchar_t* opName(Op o);
std::vector<Calc> calculations(Document& d);   // in calculation order
std::vector<Rule> rules(Document& d);

// One undo step each; Kind::None / empty sources removes it.
bool setCalculation(Document& d, const Calc& c);
bool setRule(Document& d, const Rule& r);

// Works out every calculated field from the others; returns how many values changed (no undo step of its own).
int recalculate(Document& d);
// What is wrong with `value` for this rule ("" when it is fine).
std::wstring check(const Rule& r, const std::wstring& value);
// The fields whose value breaks their rule: field, message.
std::vector<std::pair<std::wstring, std::wstring>> problems(Document& d);

} // namespace formcalc
