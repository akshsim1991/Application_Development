#pragma once
#include <string>
#include <vector>
#include "pdfium_api.hpp"

struct PageSize { float w = 612, h = 792; };   // in points

enum class OpenResult { Ok, FileUnreadable, TooLarge, NotPdf, PasswordNeeded, WrongPassword, Damaged, Unsupported };

class Document {
public:
    ~Document();
    Document() = default;
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    OpenResult open(const std::wstring& path, const std::string& password = {});
    static std::wstring describe(OpenResult r, const std::wstring& path);

    const std::wstring& path() const { return path_; }
    std::wstring title() const;
    int pageCount() const { return static_cast<int>(sizes_.size()); }
    const PageSize& pageSize(int i) const { return sizes_[i]; }
    FPDF_DOCUMENT handle() const { return doc_; }

    // View state, kept per tab.
    double zoom = 1.0;       // 1.0 == 100 % (96 dpi)
    int scrollX = 0, scrollY = 0;
    bool fitWidth = true;

private:
    std::wstring path_;
    std::vector<unsigned char> data_;   // PDFium reads from this buffer for the document's life
    FPDF_DOCUMENT doc_ = nullptr;
    std::vector<PageSize> sizes_;
};
