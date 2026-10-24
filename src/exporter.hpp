#pragma once
// Export of a document's pages as pictures, plain text, Markdown, Word, or Excel tables.
#include <string>
#include <vector>
#include "document.hpp"

namespace exporter {

enum class Format { Png, Jpeg, Text, Markdown, Word, ExcelXlsx, Csv };

struct Result {
    bool ok = false;
    std::wstring error;        // user-facing reason when !ok
    int items = 0;             // pages, files or tables written
    std::wstring firstFile;    // first file written
};

// Text-based formats write one file at `path`. Picture formats write "<name>-<page>.<ext>" next to `path`.
Result run(Document& d, const std::vector<int>& pages, Format f, const std::wstring& path, int dpi);

// Pieces exposed for testing.
struct Cell { std::wstring text; };
struct Table { int page = 0; std::vector<std::vector<std::wstring>> rows; };
std::vector<Table> findTables(Document& d, const std::vector<int>& pages);

const wchar_t* extension(Format f);

} // namespace exporter
