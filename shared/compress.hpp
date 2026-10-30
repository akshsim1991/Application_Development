#pragma once
#include <string>
#include "document.hpp"

namespace compress {

struct Options {
    int dpi = 150;              // pictures sharper than this are reduced to it
    int jpegQuality = 80;
    bool greyscale = false;
    double targetMB = 0;        // when > 0, tries stronger settings until the file is below this size
};

struct Result {
    bool changed = false;       // at least one picture was made smaller
    int pictures = 0;           // pictures replaced
    size_t before = 0, after = 0;
    bool reachedTarget = true;
};

// Recompresses the pictures of the document. One undo step; if nothing could be reduced the document is left unchanged.
Result run(Document& d, const Options& o);

} // namespace compress
