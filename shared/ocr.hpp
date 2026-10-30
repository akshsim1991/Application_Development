#pragma once
// Text recognition (OCR) for scanned pages. Windows: the bundled Tesseract engine. Other systems: the engine the app sets
// with setEngine (the Mac app: Apple's Vision). The result is an invisible text layer on top of the page picture, so
// the page can be searched, selected and copied.
#include <functional>
#include <string>
#include <vector>
#include "document.hpp"

namespace ocr {

// A recognised word: its text and box in pixels of the page picture (top left is 0, 0).
struct Word {
    std::wstring text;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

// An engine given by the app (systems without the bundled one).
struct Engine {
    std::function<std::vector<std::wstring>()> languages;
    // `grey`: w x h pixels, one byte each, rows top to bottom. False with `error` set when it failed.
    std::function<bool(const std::vector<unsigned char>& grey, int w, int h, const std::wstring& language,
                       std::vector<Word>& words, std::wstring& error)> recognise;
};
void setEngine(Engine engine);

// False with an explanation (and what to do about it) when recognition cannot run on this PC.
bool available(std::wstring& whyNot);
// The languages it knows: Windows: codes found in the tessdata folder next to the exe (e.g. "eng"); the Mac: the
// languages of Apple's text recognition (e.g. "en-US").
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
