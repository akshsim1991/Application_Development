#pragma once
// Password protection (AES-256, PDF revision 6) applied to the bytes of a saved PDF.
#include <string>
#include <vector>

namespace protect {

struct Options {
    std::wstring userPassword;     // needed to open the document; may be empty when only limits are wanted
    std::wstring ownerPassword;    // needed to lift the limits; required when limiting
    bool allowPrint = true;
    bool allowCopy = true;
    bool allowModify = true;       // editing, comments, filling in forms, assembling pages
};

// Encrypts a PDF as written by PDFium (plain objects, classic xref table). Returns false with `error` set on failure.
bool encrypt(const std::vector<unsigned char>& plain, const Options& opt, std::vector<unsigned char>& out, std::wstring& error);

} // namespace protect
