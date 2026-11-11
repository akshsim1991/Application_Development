#pragma once
// Files made into PDFs: pictures (one page each) and text or Markdown files. Used by Convert to PDF (the right-click
// menu of File Explorer), New PDF from pictures, and opening a .txt or .md file.
#include <string>
#include <vector>

namespace topdf {

enum class Kind { None, Picture, Text, Markdown };
// What kind of file this is, by its extension (.png .jpg .jpeg .bmp .gif .tif .tiff, .heic on the Mac; .txt; .md
// .markdown). None: it cannot be made into a PDF.
Kind kindOf(const std::wstring& path);
// The extensions, with the dot, lower case: what the right-click menu is offered on.
std::vector<std::wstring> extensions();

struct Result {
    bool ok = false;
    int pages = 0;
    int skipped = 0;      // pictures that could not be read or placed (their pages are left out)
    int replaced = 0;     // text: characters the standard fonts cannot show, written as "?"
    std::wstring error;   // why it failed, for a person
};
// Pictures, one page each in the order given, no larger than A4's long side (842 pt) at 96 dpi.
Result fromPictures(const std::vector<std::wstring>& files, std::vector<unsigned char>& out);
// One file of any kind above.
Result fromFile(const std::wstring& path, std::vector<unsigned char>& out);
// Where the PDF of `path` goes: the same folder and name with .pdf ("Photo.jpg" -> "Photo.pdf"), or "Photo (2).pdf",
// "Photo (3).pdf" ... when that file exists already.
std::wstring outputPath(const std::wstring& path);
// Converts `path` and writes the PDF to outputPath(path); `written` is where it went.
Result convert(const std::wstring& path, std::wstring& written);

} // namespace topdf
