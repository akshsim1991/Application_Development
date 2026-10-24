#include "document.hpp"
#include <windows.h>

Document::~Document() {
    if (doc_) pdfium::api().CloseDocument(doc_);
}

std::wstring Document::title() const {
    size_t p = path_.find_last_of(L"\\/");
    return p == std::wstring::npos ? path_ : path_.substr(p + 1);
}

OpenResult Document::open(const std::wstring& path, const std::string& password) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return OpenResult::FileUnreadable;
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    if (size.QuadPart > 0xFFFFFFFFLL) { CloseHandle(f); return OpenResult::TooLarge; }
    std::vector<unsigned char> buf(static_cast<size_t>(size.QuadPart));
    size_t got = 0;
    while (got < buf.size()) {
        DWORD n = 0;
        if (!ReadFile(f, buf.data() + got, static_cast<DWORD>(buf.size() - got), &n, nullptr) || n == 0) break;
        got += n;
    }
    CloseHandle(f);
    if (got != buf.size()) return OpenResult::FileUnreadable;
    if (buf.size() < 8) return OpenResult::NotPdf;

    // A PDF header may be preceded by a little junk, but must be within the first 1 KB.
    const std::string head(reinterpret_cast<char*>(buf.data()), std::min<size_t>(buf.size(), 1024));
    if (head.find("%PDF-") == std::string::npos) return OpenResult::NotPdf;

    const auto& api = pdfium::api();
    FPDF_DOCUMENT d = api.LoadMemDocument64(buf.data(), buf.size(), password.empty() ? nullptr : password.c_str());
    if (!d) {
        switch (api.GetLastError()) {
            case FPDF_ERR_PASSWORD: return password.empty() ? OpenResult::PasswordNeeded : OpenResult::WrongPassword;
            case FPDF_ERR_SECURITY: return OpenResult::Unsupported;
            default: return OpenResult::Damaged;
        }
    }
    int n = api.GetPageCount(d);
    if (n <= 0) { api.CloseDocument(d); return OpenResult::Damaged; }

    sizes_.assign(n, PageSize{});
    for (int i = 0; i < n; ++i) {
        FS_SIZEF s{};
        if (api.GetPageSizeByIndexF(d, i, &s) && s.width > 0 && s.height > 0) sizes_[i] = {s.width, s.height};
    }
    if (doc_) api.CloseDocument(doc_);
    doc_ = d;
    data_ = std::move(buf);   // the buffer must outlive doc_, and moving a vector keeps its storage
    path_ = path;
    return OpenResult::Ok;
}

std::wstring Document::describe(OpenResult r, const std::wstring& path) {
    size_t p = path.find_last_of(L"\\/");
    std::wstring name = L"\"" + (p == std::wstring::npos ? path : path.substr(p + 1)) + L"\"";
    switch (r) {
        case OpenResult::FileUnreadable: return name + L" could not be read. It may have been moved, deleted, or be in use.";
        case OpenResult::TooLarge: return name + L" is too large to open (over 4 GB).";
        case OpenResult::NotPdf: return name + L" is not a PDF document.";
        case OpenResult::PasswordNeeded: return name + L" is protected. Enter the password to open it.";
        case OpenResult::WrongPassword: return L"That password did not open " + name + L".";
        case OpenResult::Unsupported: return name + L" uses an encryption method that is not supported.";
        case OpenResult::Damaged: return name + L" is damaged or has no pages.";
        default: return L"";
    }
}
