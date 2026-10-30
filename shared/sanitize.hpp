#pragma once
// Removes hidden data before a document is shared: document information and XMP metadata, attachments, scripts,
// page thumbnails and application data, hidden annotations, and every object left over from earlier versions.
// The result is a new, compact file that holds only what the pages still use.
#include <string>
#include <vector>

namespace sanitize {

struct Options {
    bool metadata = true;      // document information and XMP metadata
    bool attachments = true;   // embedded files and file-attachment annotations
    bool scripts = true;       // JavaScript, open actions and other automatic actions
    bool hidden = true;        // hidden annotations, page thumbnails, application data (PieceInfo), XFA form data
    bool comments = false;     // every comment, highlight and drawing (links and form fields stay)
    bool bookmarks = false;
};

struct Report {
    std::vector<std::wstring> removed;   // one line per kind of data that was found and removed
    int leftovers = 0;                   // objects no longer used by the document
    size_t before = 0, after = 0;        // file sizes
};

// `pdf` is a complete file as written by PDFium, without a password. Returns false with `error` set when the
// structure is not supported.
bool run(const std::vector<unsigned char>& pdf, const Options& opt, std::vector<unsigned char>& out, Report& report, std::wstring& error);

} // namespace sanitize
