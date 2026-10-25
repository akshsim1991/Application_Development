#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "pdfium_api.hpp"
#include "protect.hpp"
#include "sign.hpp"

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
    Document();
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
    void setName(const std::wstring& n) { name_ = n; }   // shown when the document has no file yet
    // A PDF with empty pages of the given sizes (points); the bytes can be passed to load().
    static bool blankBytes(const std::vector<std::pair<double, double>>& sizes, std::vector<unsigned char>& out);
    bool cropBoxOf(int page, double& l, double& b, double& r, double& t);
    int pageCount() const { return static_cast<int>(sizes_.size()); }
    const PageSize& pageSize(int i) const { return sizes_[i]; }
    FPDF_DOCUMENT handle() const { return doc_; }

    // ---- fillable forms ----
    FPDF_FORMHANDLE form() const { return form_; }
    bool hasForm() const { return form_ != nullptr; }
    // Draws a page into a bitmap as shown on screen: content, annotations, and form fields when the document has them.
    void renderPage(int page, FPDF_BITMAP bmp, int x, int y, int w, int h, bool annotations = true);
    // Call when the user starts interacting with a field (mouse press): remembers the state so the edit can be undone.
    void formBeforeInput();
    std::function<void(int page)> onFormInvalidate;   // a field changed how it looks
    std::function<void()> onFormChanged;              // a field value was committed (the document is now modified)
    int currentPageForForm = 0;
    static void setTimerWindow(void* hwnd);
    static void fireTimer(int id);
    int indexOfPage(FPDF_PAGE p) const;

    // Text. The page and its text layer are loaded on first use and kept until the document closes.
    FPDF_PAGE page(int i);
    FPDF_TEXTPAGE text(int i);
    int charCount(int i);
    std::wstring textRange(int page, int start, int count);
    // Every match of `query`, in page order. Pages not already loaded are loaded only for the search.
    std::vector<TextHit> find(const std::wstring& query, bool matchCase);
    std::vector<Bookmark> bookmarks() const;

    // Saving. The document keeps encryption when it was opened with a password.
    bool serialize(std::vector<unsigned char>& out, int flags = 0) const;
    SaveResult saveAs(const std::wstring& path);       // writes a temp file, then replaces `path`
    void setPath(const std::wstring& p) { path_ = p; }
    // Password protection is applied when the document is saved. Set/Remove are one undo step each.
    enum class Protection { Keep, Set, Remove };
    bool isProtected() const { return protMode_ == Protection::Set || (protMode_ == Protection::Keep && !password_.empty()); }
    bool setProtection(const protect::Options& opt);
    bool removeProtection();
    Protection protectionMode() const { return protMode_; }

    // Digital signatures found in the file when it was opened (checked against the bytes on disk).
    const std::vector<sign::Status>& signatures() const { return signatures_; }

    // Change tracking. Call beginChange() BEFORE modifying the document: it stores an undo step.
    // A change that turns out to be a no-op can be dropped with cancelChange().
    bool dirty() const { return dirty_; }
    void markModified() { dirty_ = true; ++revision_; }   // for documents restored with changes that were never saved
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
    bool movePages(std::vector<int> pages, int destination);
    bool reversePages();   // last page first; one undo step   // destination is the index before the move
    bool cropPages(const std::vector<int>& pages, double leftPt, double bottomPt, double rightPt, double topPt);
    // Copies pages of `src` into this document, so that the first copy lands at index `at`.
    bool importPages(Document& src, const std::vector<int>& srcPages, int at);
    // Writes bytes to a file safely (temp file, then replace).
    static SaveResult writeFile(const std::wstring& path, const std::vector<unsigned char>& bytes);
    // Writes the given pages to a new PDF file (the document itself is not changed).
    SaveResult extractPages(const std::vector<int>& pages, const std::wstring& path) const;
    // The same pages as a complete PDF in memory (used to copy, cut and paste pages between documents).
    bool pagesToBytes(const std::vector<int>& pages, std::vector<unsigned char>& out) const;
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
    std::wstring name_;
    std::vector<PageSize> sizes_;
    std::string password_;
    std::vector<sign::Status> signatures_;
    bool dirty_ = false;
    int revision_ = 0;
    struct Snapshot { std::vector<unsigned char> bytes; Protection mode; protect::Options opt; };
    Protection protMode_ = Protection::Keep;
    protect::Options protOpt_;
    std::vector<Snapshot> undo_, redo_;
    size_t undoBytes_ = 0;
    bool restoreFrom(const Snapshot& snap);
    void trimUndo();
    std::vector<FPDF_PAGE> pages_;
    std::vector<FPDF_TEXTPAGE> texts_;

    FPDF_FORMHANDLE form_ = nullptr;
    struct FormInfo;
    std::unique_ptr<FormInfo> formInfo_;
    std::vector<unsigned char> formSnap_;     // state before the current field interaction
    bool formSnapValid_ = false;
    void initForm();
    void exitForm();
    void formCommitted();
    friend struct FormCallbacks;
    void closePages();
    void rebuildPages();      // after pages were added, removed or resized
};
