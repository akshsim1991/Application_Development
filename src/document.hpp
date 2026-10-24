#pragma once
#include <string>
#include <vector>
#include "pdfium_api.hpp"

struct PageSize { float w = 612, h = 792; };   // in points

// An area marked for redaction, in page coordinates (points, origin bottom-left). Not part of the file until applied.
struct RedactMark { int page; double left, bottom, right, top; };

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
    // Same, keeping the password the document was opened with.
    OpenResult reload(std::vector<unsigned char>&& bytes) { return load(std::move(bytes), password_); }
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
    int revision() const { return revision_; }

    // ---- page operations (each one is a single undo step; page numbers are 0-based) ----
    void resetText(int page);   // forget the cached text of a page whose content changed
    void refreshPages() { closePages(); rebuildPages(); }   // drop cached pages and text after content changed
    bool deletePages(std::vector<int> pages);          // refuses to delete every page
    bool insertBlankPage(int after, int templatePage); // `after` = -1 inserts at the start
    bool rotatePages(const std::vector<int>& pages, int quarterTurnsClockwise);
    bool movePages(std::vector<int> pages, int destination);   // destination is the index before the move
    bool cropPages(const std::vector<int>& pages, double leftPt, double bottomPt, double rightPt, double topPt);
    // Copies pages of `src` into this document, so that the first copy lands at index `at`.
    bool importPages(Document& src, const std::vector<int>& srcPages, int at);
    // Writes the given pages to a new PDF file (the document itself is not changed).
    SaveResult extractPages(const std::vector<int>& pages, const std::wstring& path) const;
    // Parses "1-3, 5, 8-" into 0-based page numbers (sorted, no duplicates). Empty on error.
    std::vector<int> parseRange(const std::wstring& text) const;         // bumps on every change, undo and redo

    std::vector<RedactMark> redactions;   // pending marks; cleared whenever the pages change

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
    void rebuildPages();      // after pages were added, removed or resized
    static SaveResult writeFile(const std::wstring& path, const std::vector<unsigned char>& bytes);
};
