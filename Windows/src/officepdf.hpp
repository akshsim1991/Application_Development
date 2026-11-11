#pragma once
// Office files made into PDFs by the office program on this PC (Feather PDF cannot lay out slides, pages of a word
// processor or sheets itself): Microsoft PowerPoint, Word or Excel, else WPS Office, through their automation
// interfaces (the same "Save as PDF" a person would choose), else LibreOffice (soffice --convert-to pdf).
#include <string>

namespace officepdf {

enum class Kind { None, Slides, Text, Sheet };
// By the extension: .ppt .pptx .pps .ppsx .odp (Slides), .doc .docx .rtf .odt (Text), .xls .xlsx .ods (Sheet).
Kind kindOf(const std::wstring& path);
// The extensions, with the dot, lower case (for the right-click menu).
const wchar_t* const* extensions(int& count);

// Writes `out` (a PDF) from `in`. `program` says which program did it ("Microsoft PowerPoint" ...); `error` why not.
bool convert(const std::wstring& in, const std::wstring& out, std::wstring& program, std::wstring& error);

// The tests: the automation calls tried on Windows' own Scripting.FileSystemObject (no Office needed). A report.
std::wstring selfTest(const std::wstring& existingFile);

} // namespace officepdf
