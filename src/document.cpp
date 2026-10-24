#include "document.hpp"
#include <windows.h>
#include <algorithm>

Document::~Document() {
    closePages();
    if (doc_) pdfium::api().FPDF_CloseDocument(doc_);
}

void Document::closePages() {
    const auto& api = pdfium::api();
    for (auto& t : texts_) if (t) api.FPDFText_ClosePage(t);
    for (auto& p : pages_) if (p) api.FPDF_ClosePage(p);
    texts_.clear();
    pages_.clear();
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
    OpenResult r = load(std::move(buf), password);
    if (r == OpenResult::Ok) { path_ = path; dirty_ = false; undo_.clear(); redo_.clear(); undoBytes_ = 0; }
    return r;
}

OpenResult Document::load(std::vector<unsigned char>&& buf, const std::string& password) {
    if (buf.size() < 8) return OpenResult::NotPdf;
    // A PDF header may be preceded by a little junk, but must be within the first 1 KB.
    const std::string head(reinterpret_cast<char*>(buf.data()), std::min<size_t>(buf.size(), 1024));
    if (head.find("%PDF-") == std::string::npos) return OpenResult::NotPdf;

    const auto& api = pdfium::api();
    FPDF_DOCUMENT d = api.FPDF_LoadMemDocument64(buf.data(), buf.size(), password.empty() ? nullptr : password.c_str());
    if (!d) {
        switch (api.FPDF_GetLastError()) {
            case FPDF_ERR_PASSWORD: return password.empty() ? OpenResult::PasswordNeeded : OpenResult::WrongPassword;
            case FPDF_ERR_SECURITY: return OpenResult::Unsupported;
            default: return OpenResult::Damaged;
        }
    }
    int n = api.FPDF_GetPageCount(d);
    if (n <= 0) { api.FPDF_CloseDocument(d); return OpenResult::Damaged; }

    sizes_.assign(n, PageSize{});
    for (int i = 0; i < n; ++i) {
        FS_SIZEF s{};
        if (api.FPDF_GetPageSizeByIndexF(d, i, &s) && s.width > 0 && s.height > 0) sizes_[i] = {s.width, s.height};
    }
    closePages();
    if (doc_) api.FPDF_CloseDocument(doc_);
    doc_ = d;
    pages_.assign(n, nullptr);
    texts_.assign(n, nullptr);
    data_ = std::move(buf);   // the buffer must outlive doc_, and moving a vector keeps its storage
    password_ = password;
    ++revision_;
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

FPDF_PAGE Document::page(int i) {
    if (i < 0 || i >= pageCount()) return nullptr;
    if (!pages_[i]) pages_[i] = pdfium::api().FPDF_LoadPage(doc_, i);
    return pages_[i];
}

FPDF_TEXTPAGE Document::text(int i) {
    if (i < 0 || i >= pageCount()) return nullptr;
    if (!texts_[i]) {
        FPDF_PAGE p = page(i);
        if (p) texts_[i] = pdfium::api().FPDFText_LoadPage(p);
    }
    return texts_[i];
}

int Document::charCount(int i) {
    FPDF_TEXTPAGE t = text(i);
    return t ? std::max(0, pdfium::api().FPDFText_CountChars(t)) : 0;
}

std::wstring Document::textRange(int pg, int start, int count) {
    FPDF_TEXTPAGE t = text(pg);
    if (!t || count <= 0) return {};
    std::vector<unsigned short> buf(static_cast<size_t>(count) + 1);
    int got = pdfium::api().FPDFText_GetText(t, start, count, buf.data());   // includes the terminator
    if (got <= 1) return {};
    return std::wstring(buf.begin(), buf.begin() + (got - 1));
}

std::vector<TextHit> Document::find(const std::wstring& query, bool matchCase) {
    std::vector<TextHit> hits;
    if (query.empty() || !doc_) return hits;
    const auto& api = pdfium::api();
    std::vector<unsigned short> q(query.begin(), query.end());
    q.push_back(0);
    for (int i = 0; i < pageCount(); ++i) {
        const bool wasLoaded = texts_[i] != nullptr;
        FPDF_PAGE tmpPage = nullptr;
        FPDF_TEXTPAGE tp = texts_[i];
        if (!tp) {
            tmpPage = pages_[i] ? pages_[i] : api.FPDF_LoadPage(doc_, i);
            if (!tmpPage) continue;
            tp = api.FPDFText_LoadPage(tmpPage);
        }
        if (tp) {
            FPDF_SCHHANDLE sh = api.FPDFText_FindStart(tp, q.data(), matchCase ? FPDF_MATCHCASE : 0, 0);
            if (sh) {
                while (api.FPDFText_FindNext(sh)) {
                    hits.push_back({i, api.FPDFText_GetSchResultIndex(sh), api.FPDFText_GetSchCount(sh)});
                }
                api.FPDFText_FindClose(sh);
            }
            if (!wasLoaded) api.FPDFText_ClosePage(tp);
        }
        if (tmpPage && !pages_[i]) api.FPDF_ClosePage(tmpPage);
    }
    return hits;
}

namespace {
std::wstring bookmarkTitle(FPDF_BOOKMARK b) {
    const auto& api = pdfium::api();
    unsigned long bytes = api.FPDFBookmark_GetTitle(b, nullptr, 0);
    if (bytes < 4) return {};
    std::vector<unsigned short> buf(bytes / 2 + 1);
    api.FPDFBookmark_GetTitle(b, buf.data(), bytes);
    return std::wstring(buf.begin(), buf.begin() + (bytes / 2 - 1));
}

void readBookmarks(FPDF_DOCUMENT doc, FPDF_BOOKMARK parent, std::vector<Bookmark>& out, int depth) {
    const auto& api = pdfium::api();
    if (depth > 12) return;   // guards against damaged files that loop
    for (FPDF_BOOKMARK b = api.FPDFBookmark_GetFirstChild(doc, parent); b; b = api.FPDFBookmark_GetNextSibling(doc, b)) {
        Bookmark bm;
        bm.title = bookmarkTitle(b);
        FPDF_DEST dest = api.FPDFBookmark_GetDest(doc, b);
        if (!dest) {
            FPDF_ACTION act = api.FPDFBookmark_GetAction(b);
            if (act) dest = api.FPDFAction_GetDest(doc, act);
        }
        if (dest) bm.page = api.FPDFDest_GetDestPageIndex(doc, dest);
        readBookmarks(doc, b, bm.children, depth + 1);
        out.push_back(std::move(bm));
        if (out.size() > 20000) return;
    }
}
} // namespace

std::vector<Bookmark> Document::bookmarks() const {
    std::vector<Bookmark> out;
    if (doc_) readBookmarks(doc_, nullptr, out, 0);
    return out;
}

// ---- saving and undo ----

namespace {
struct Sink : FPDF_FILEWRITE {
    std::vector<unsigned char>* out;
};
int writeBlock(FPDF_FILEWRITE* self, const void* data, unsigned long size) {
    auto* s = static_cast<Sink*>(self);
    const auto* p = static_cast<const unsigned char*>(data);
    s->out->insert(s->out->end(), p, p + size);
    return 1;
}
}

bool Document::serialize(std::vector<unsigned char>& out) const {
    if (!doc_) return false;
    out.clear();
    Sink sink;
    sink.version = 1;
    sink.WriteBlock = writeBlock;
    sink.out = &out;
    return pdfium::api().FPDF_SaveAsCopy(doc_, &sink, 0) != 0 && out.size() > 8;
}

SaveResult Document::saveAs(const std::wstring& path) {
    std::vector<unsigned char> bytes;
    if (!serialize(bytes)) return SaveResult::Failed;
    const std::wstring tmp = path + L".feather-tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return SaveResult::CannotWrite;
    size_t done = 0;
    DWORD err = 0;
    while (done < bytes.size()) {
        DWORD n = 0;
        if (!WriteFile(f, bytes.data() + done, static_cast<DWORD>(std::min<size_t>(bytes.size() - done, 1u << 24)), &n, nullptr)) {
            err = GetLastError();
            break;
        }
        done += n;
    }
    if (!err && !FlushFileBuffers(f)) err = GetLastError();
    CloseHandle(f);
    if (err || done != bytes.size()) {
        DeleteFileW(tmp.c_str());
        return err == ERROR_DISK_FULL || err == ERROR_HANDLE_DISK_FULL ? SaveResult::DiskFull : SaveResult::CannotWrite;
    }
    // Replace the target only after the whole new file is safely on disk.
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return SaveResult::CannotWrite;
    }
    path_ = path;
    dirty_ = false;
    return SaveResult::Ok;
}

void Document::trimUndo() {
    constexpr size_t kMaxSteps = 30, kMaxBytes = 300u << 20;
    while (!undo_.empty() && (undo_.size() > kMaxSteps || undoBytes_ > kMaxBytes)) {
        undoBytes_ -= undo_.front().size();
        undo_.erase(undo_.begin());
    }
}

void Document::beginChange() {
    std::vector<unsigned char> snap;
    if (serialize(snap)) {
        undoBytes_ += snap.size();
        undo_.push_back(std::move(snap));
        trimUndo();
    }
    redo_.clear();
    dirty_ = true;
    ++revision_;
}

void Document::cancelChange() {
    if (!undo_.empty()) { undoBytes_ -= undo_.back().size(); undo_.pop_back(); }
    if (undo_.empty() && redo_.empty()) dirty_ = false;   // nothing else was changed in this session
}

bool Document::restoreFrom(const std::vector<unsigned char>& snap) {
    std::vector<unsigned char> copy = snap;
    return load(std::move(copy), password_) == OpenResult::Ok;
}

bool Document::undo() {
    if (undo_.empty()) return false;
    std::vector<unsigned char> cur;
    if (!serialize(cur)) return false;
    std::vector<unsigned char> snap = std::move(undo_.back());
    undo_.pop_back();
    undoBytes_ -= snap.size();
    if (!restoreFrom(snap)) { undo_.push_back(std::move(snap)); undoBytes_ += undo_.back().size(); return false; }
    redo_.push_back(std::move(cur));
    dirty_ = true;
    return true;
}

bool Document::redo() {
    if (redo_.empty()) return false;
    std::vector<unsigned char> cur;
    if (!serialize(cur)) return false;
    std::vector<unsigned char> snap = std::move(redo_.back());
    redo_.pop_back();
    if (!restoreFrom(snap)) { redo_.push_back(std::move(snap)); return false; }
    undoBytes_ += cur.size();
    undo_.push_back(std::move(cur));
    dirty_ = true;
    return true;
}
