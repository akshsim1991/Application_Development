#pragma once
// Text recognition (OCR) for scanned pages, using the bundled Tesseract engine. The result is an invisible text layer
// on top of the page picture, so the page can be searched, selected and copied.
#include <functional>
#include <string>
#include <vector>
#include "document.hpp"

namespace ocr {

// False with an explanation (and what to do about it) when recognition cannot run on this PC.
bool available(std::wstring& whyNot);
// Language codes found in the tessdata folder next to the exe (e.g. "eng").
std::vector<std::wstring> languages();

struct Result {
    int pagesDone = 0;
    int pagesSkipped = 0;       // pages that already had text
    int words = 0;
    bool cancelled = false;
    std::wstring error;         // set when something went wrong (pages done before it are kept)
};

// Recognises the given pages. `progress(done, total)` is called before each page; return false to stop.
// One undo step. Pages that already have text are skipped when `skipTextPages` is set.
Result run(Document& d, const std::vector<int>& pages, const std::wstring& language, bool skipTextPages,
           const std::function<bool(int, int)>& progress);

} // namespace ocr
