#pragma once
#include <string>
#include <vector>
#include "pdfium_api.hpp"

struct PageSize { float w = 612, h = 792; };   // in points

struct TextHit { int page = 0, start = 0, count = 0; };   // a run of characters on one page

struct Bookmark {
    std::wstring title;
    int page = -1;                 // 0-based, -1 when the bookmark has no page target
    std::vector<Bookmark> children;
};

enum class SaveResult { Ok, CannotWrite, DiskFull, Failed };

enum class OpenResult { Ok, FileUnreadable, TooLarge, NotPdf, PasswordNeeded, WrongPassword, Damaged, Unsupported };

class Document {
public:
    ~Document();
    Document() = default;
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    OpenResult open(const std::wstring& path, const std::string& password = {});
    // Replaces the content with `bytes` (used by undo/redo). View state and path are kept.
    OpenResult load(std::vector<unsigned char>&& bytes, const std::string& password);
    static std::wstring describe(OpenResult r, const std::wstring& path);

    const std::wstring& path() const { return path_; }
    std::wstring title() const;
    int pageCount() const { return static_cast<int>(sizes_.size()); }
    const PageSize& pageSize(int i) const { return sizes_[i]; }
    FPDF_DOCUMENT handle() const { return doc_; }

    // Text. The page and its text layer are loaded on first use and kept until the document closes.
    FPDF_PAGE page(int i);
    FPDF_TEXTPAGE text(int i);
    int charCount(int i);
    std::wstring textRange(int page, int start, int count);
    // Every match of `query`, in page order. Pages not already loaded are loaded only for the search.
    std::vector<TextHit> find(const std::wstring& query, bool matchCase);
    std::vector<Bookmark> bookmarks() const;

    // Saving. The document keeps encryption when it was opened with a password.
    bool serialize(std::vector<unsigned char>& out) const;
    SaveResult saveAs(const std::wstring& path);       // writes a temp file, then replaces `path`
    void setPath(const std::wstring& p) { path_ = p; }
    bool isEncrypted() const { return !password_.empty(); }

    // Change tracking. Call beginChange() BEFORE modifying the document: it stores an undo step.
    // A change that turns out to be a no-op can be dropped with cancelChange().
    bool dirty() const { return dirty_; }
    void beginChange();
    void cancelChange();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool undo();
    bool redo();
    int revision() const { return revision_; }         // bumps on every change, undo and redo

    // View state, kept per tab.
    double zoom = 1.0;       // 1.0 == 100 % (96 dpi)
    int scrollX = 0, scrollY = 0;
    bool fitWidth = true;

private:
    std::wstring path_;
    std::vector<unsigned char> data_;   // PDFium reads from this buffer for the document's life
    FPDF_DOCUMENT doc_ = nullptr;
    std::vector<PageSize> sizes_;
    std::string password_;
    bool dirty_ = false;
    int revision_ = 0;
    std::vector<std::vector<unsigned char>> undo_, redo_;
    size_t undoBytes_ = 0;
    bool restoreFrom(const std::vector<unsigned char>& snap);
    void trimUndo();
    std::vector<FPDF_PAGE> pages_;
    std::vector<FPDF_TEXTPAGE> texts_;

    void closePages();
};
