#pragma once
// Accessibility: checks a document against the usual rules for people who use screen readers, keyboards or magnifiers
// (as Acrobat's accessibility check and PDF/UA do), and puts right what can be put right without a person: title, language,
// title shown in the window, tab order, descriptions of links, tooltips of form fields.
#include <string>
#include <vector>
#include "document.hpp"

namespace a11y {

enum class Result { Passed, Failed, NeedsPerson };   // NeedsPerson: only a person can judge or do it (colour contrast, reading order)

struct Item {
    std::string id;            // "title", "lang", "tagged", ... (stable, for tests)
    std::wstring rule;         // what is checked, in words
    Result result = Result::Passed;
    std::wstring detail;       // what was found
    bool fixable = false;      // fix() can put it right
};

std::vector<Item> check(Document& d);

struct FixOptions {
    std::wstring titleText;              // empty: from the first large text on page 1, else the file name
    std::wstring language = L"en-US";    // a language tag
    bool title = true, lang = true, showTitle = true, tabOrder = true, linkText = true, fieldTips = true;
    // (the bools say which fixes to make)
};

// One undo step. Returns what was done, in words (empty: nothing needed doing).
std::vector<std::wstring> fix(Document& d, const FixOptions& o);

// A title suggested for the document: the largest text on its first page, else its file name.
std::wstring suggestedTitle(Document& d);

// A report in Markdown, for textpdf::build (Passed / Failed / Needs a person, with the details).
std::wstring report(const std::vector<Item>& items, const std::wstring& name);

} // namespace a11y
