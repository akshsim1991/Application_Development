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

// A run of characters on one page; or, with annot >= 0, a run in the text of that comment (annotation index on the
// page), shown as the comment's rectangle (page coordinates, points from the bottom left).
struct TextHit {
    int page = 0, start = 0, count = 0;
    int annot = -1;
    double left = 0, bottom = 0, right = 0, top = 0;
};

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
    // `out` becomes a separate copy of the document as it is now (to try a change on before making it here).
    OpenResult copyInto(Document& out) const;
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
    void setFieldHighlight(bool on);                  // the tint behind form fields (off while printing)
    // Draws a page into a bitmap as shown on screen: content, annotations, and form fields when the document has them.
    void renderPage(int page, FPDF_BITMAP bmp, int x, int y, int w, int h, bool annotations = true);
    // For pictures that leave the screen (export, copying, redaction, comparison): form fields without the tint.
    void renderOutput(int page, FPDF_BITMAP bmp, int x, int y, int w, int h);
    // Call when the user starts interacting with a field (mouse press): remembers the state so the edit can be undone.
    void formBeforeInput();
    std::function<void(int page)> onFormInvalidate;   // a field changed how it looks
    std::function<void()> onFormChanged;              // a field value was committed (the document is now modified)
    std::function<void()> onFormStart;                // the user is starting to type into (or click) a field
    int currentPageForForm = 0;
#ifdef _WIN32
    static void setTimerWindow(void* hwnd);
#else
    // Off Windows the app gives form fields their timers (blinking caret, scripts) and the pointer shape
    // (0 arrow, 1 text, 2 hand); a timer it starts calls fireTimer(id) each time it is due.
    struct FormHost {
        bool (*startTimer)(int id, int ms) = nullptr;
        void (*stopTimer)(int id) = nullptr;
        void (*cursor)(int shape) = nullptr;
    };
    static void setFormHost(const FormHost& h);
#endif
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
    bool bytesToSave(std::vector<unsigned char>& out) const;   // the file as saveAs writes it (with the protection set)
    void savedAs(const std::wstring& path);            // the bytes were written to `path` by someone else (the Mac app)
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
    void beginChange(const std::wstring& what = L"Change");   // `what`: the step's name in the undo history
    void cancelChange();
    // The undo steps taken since there were `depth` of them become one, named `what` (several edits made as one command).
    size_t undoDepth() const { return undo_.size(); }
    void mergeChangesSince(size_t depth, const std::wstring& what);
    // Puts the document back as it was at the last beginChange() and forgets that step (for a change that failed half-way).
    bool rollbackChange();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    bool undo();
    bool redo();
    // The undo history: what each step did and when, oldest first; then the steps that can be redone, next first.
    struct Step { std::wstring what; long long when = 0; };   // when: seconds since 1970
    std::vector<Step> undoSteps() const;
    std::vector<Step> redoSteps() const;
    // Undoes (n > 0) or redoes (n < 0) |n| steps at once. False if one of them failed (the ones before stay done).
    bool stepBack(int n);
    int revision() const { return revision_; }

    // ---- page operations (each one is a single undo step; page numbers are 0-based) ----
    void resetText(int page);   // forget the cached text of a page whose content changed
    // Keeps a loaded page from being closed to save memory (its objects were changed in memory without writing them).
    void pinPage(int page);
    void refreshPages() { closePages(); rebuildPages(); }   // drop cached pages and text after content changed
    bool deletePages(std::vector<int> pages);          // refuses to delete every page
    bool insertBlankPage(int after, int templatePage); // `after` = -1 inserts at the start
    bool rotatePages(const std::vector<int>& pages, int quarterTurnsClockwise);
    bool movePages(std::vector<int> pages, int destination);
    bool reversePages();   // last page first; one undo step
    // Makes every annotation and form field part of the page content (FLAT_PRINT). Returns the pages changed; one undo step.
    int flattenAll();   // destination is the index before the move
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
    int shownPage = 0;       // the page on screen in the single-page layout
    bool fitWidth = true;
    int tabHue = -1;         // the tab's colour (one of the window's tab colours), given when the tab opens

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
    struct Snapshot { std::vector<unsigned char> bytes; Protection mode; protect::Options opt; std::wstring what; long long when = 0; };
    Protection protMode_ = Protection::Keep;
    protect::Options protOpt_;
    std::vector<Snapshot> undo_, redo_;
    size_t undoBytes_ = 0;
    bool restoreFrom(const Snapshot& snap);
    void trimUndo();
    std::vector<FPDF_PAGE> pages_;
    std::vector<FPDF_TEXTPAGE> texts_;
    // Loaded pages are kept for reuse, the most recently used ones only: a long document read to the end would otherwise
    // keep every page in memory.
    std::vector<unsigned> lastUse_;
    std::vector<char> pinned_;
    unsigned useTick_ = 0;
    void closePage(int i);
    void trimPages(int keep);

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
