#include "document.hpp"
#include "pdfedit.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <cwctype>

struct Document::FormInfo : FPDF_FORMFILLINFO {
    Document* doc = nullptr;
};

namespace {
std::map<int, TimerCallback> g_timers;
HWND g_timerWnd = nullptr;
int g_nextTimer = 1000;
}

struct FormCallbacks {
    static Document* doc(FPDF_FORMFILLINFO* self) { return static_cast<Document::FormInfo*>(self)->doc; }
    static void invalidate(FPDF_FORMFILLINFO* self, FPDF_PAGE page, double, double, double, double) {
        Document* d = doc(self);
        int i = d->indexOfPage(page);
        if (i >= 0 && d->onFormInvalidate) d->onFormInvalidate(i);
    }
    static void setCursor(FPDF_FORMFILLINFO*, int type) {
        LPCWSTR id = type == FXCT_VBEAM || type == FXCT_HBEAM ? IDC_IBEAM : type == FXCT_HAND ? IDC_HAND : IDC_ARROW;
        SetCursor(LoadCursor(nullptr, id));
    }
    static int setTimer(FPDF_FORMFILLINFO*, int ms, TimerCallback cb) {
        if (!g_timerWnd) return 0;
        const int id = g_nextTimer++;
        if (!SetTimer(g_timerWnd, static_cast<UINT_PTR>(id), static_cast<UINT>(ms), nullptr)) return 0;
        g_timers[id] = cb;
        return id;
    }
    static void killTimer(FPDF_FORMFILLINFO*, int id) {
        if (g_timerWnd) KillTimer(g_timerWnd, static_cast<UINT_PTR>(id));
        g_timers.erase(id);
    }
    static FPDF_SYSTEMTIME localTime(FPDF_FORMFILLINFO*) {
        SYSTEMTIME t; GetLocalTime(&t);
        FPDF_SYSTEMTIME o{};
        o.wYear = t.wYear; o.wMonth = t.wMonth; o.wDayOfWeek = t.wDayOfWeek; o.wDay = t.wDay;
        o.wHour = t.wHour; o.wMinute = t.wMinute; o.wSecond = t.wSecond; o.wMilliseconds = t.wMilliseconds;
        return o;
    }
    static void onChange(FPDF_FORMFILLINFO* self) { doc(self)->formCommitted(); }
    static FPDF_PAGE getPage(FPDF_FORMFILLINFO* self, FPDF_DOCUMENT, int index) { return doc(self)->page(index); }
    static FPDF_PAGE getCurrentPage(FPDF_FORMFILLINFO* self, FPDF_DOCUMENT) { return doc(self)->page(doc(self)->currentPageForForm); }
    static int getRotation(FPDF_FORMFILLINFO*, FPDF_PAGE) { return 0; }
};

void Document::setTimerWindow(void* hwnd) { g_timerWnd = static_cast<HWND>(hwnd); }

void Document::fireTimer(int id) {
    auto it = g_timers.find(id);
    if (it != g_timers.end()) it->second(id);
}

int Document::indexOfPage(FPDF_PAGE p) const {
    for (size_t i = 0; i < pages_.size(); ++i) if (pages_[i] == p) return static_cast<int>(i);
    return -1;
}

void Document::initForm() {
    const auto& api = pdfium::api();
    if (!doc_) return;
    const int type = api.FPDF_GetFormType(doc_);
    if (type != FORMTYPE_ACRO_FORM) return;   // XFA forms are not supported (this PDFium build has no XFA)
    formInfo_ = std::make_unique<FormInfo>();
    std::memset(static_cast<FPDF_FORMFILLINFO*>(formInfo_.get()), 0, sizeof(FPDF_FORMFILLINFO));
    formInfo_->version = 1;
    formInfo_->doc = this;
    formInfo_->FFI_Invalidate = FormCallbacks::invalidate;
    formInfo_->FFI_SetCursor = FormCallbacks::setCursor;
    formInfo_->FFI_SetTimer = FormCallbacks::setTimer;
    formInfo_->FFI_KillTimer = FormCallbacks::killTimer;
    formInfo_->FFI_GetLocalTime = FormCallbacks::localTime;
    formInfo_->FFI_OnChange = FormCallbacks::onChange;
    formInfo_->FFI_GetPage = FormCallbacks::getPage;
    formInfo_->FFI_GetCurrentPage = FormCallbacks::getCurrentPage;
    formInfo_->FFI_GetRotation = FormCallbacks::getRotation;
    form_ = api.FPDFDOC_InitFormFillEnvironment(doc_, formInfo_.get());
    if (!form_) { formInfo_.reset(); return; }
    setFieldHighlight(true);
}

void Document::setFieldHighlight(bool on) {
    if (!form_) return;
    const auto& api = pdfium::api();
    api.FPDF_SetFormFieldHighlightColor(form_, 0, 0xFFE4DD);   // light blue behind fields, so they can be found
    api.FPDF_SetFormFieldHighlightAlpha(form_, on ? 100 : 0);
}

void Document::exitForm() {
    if (form_) {
        pdfium::api().FPDFDOC_ExitFormFillEnvironment(form_);
        form_ = nullptr;
    }
    formInfo_.reset();
    formSnapValid_ = false;
}

void Document::formBeforeInput() {
    formSnapValid_ = serialize(formSnap_);
}

void Document::formCommitted() {
    if (formSnapValid_) {
        undoBytes_ += formSnap_.size();
        undo_.push_back({std::move(formSnap_), protMode_, protOpt_});
        formSnap_.clear();
        formSnapValid_ = false;
        trimUndo();
        redo_.clear();
    }
    dirty_ = true;
    ++revision_;
    if (onFormChanged) onFormChanged();
}

void Document::renderPage(int page, FPDF_BITMAP bmp, int x, int y, int w, int h, bool annotations) {
    const auto& api = pdfium::api();
    FPDF_PAGE pg = this->page(page);
    if (!pg || !bmp) return;
    api.FPDF_RenderPageBitmap(bmp, pg, x, y, w, h, 0, annotations ? FPDF_ANNOT : 0);
    if (form_ && annotations) api.FPDF_FFLDraw(form_, bmp, pg, x, y, w, h, 0, FPDF_ANNOT);
}

Document::Document() = default;

Document::~Document() {
    closePages();
    exitForm();
    if (doc_) pdfium::api().FPDF_CloseDocument(doc_);
}

void Document::closePages() {
    const auto& api = pdfium::api();
    for (auto& t : texts_) if (t) api.FPDFText_ClosePage(t);
    for (auto& p : pages_) if (p) {
        if (form_) api.FORM_OnBeforeClosePage(p, form_);
        api.FPDF_ClosePage(p);
    }
    texts_.clear();
    pages_.clear();
    lastUse_.clear();
    pinned_.clear();
}

std::wstring Document::title() const {
    if (path_.empty() && !name_.empty()) return name_;
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
    // Signatures are checked against the file exactly as it is on disk, before PDFium takes the buffer.
    static const char kMarker[] = "/ByteRange";
    std::vector<sign::Status> sigs;
    if (std::search(buf.begin(), buf.end(), kMarker, kMarker + 10) != buf.end()) sigs = sign::check(buf);
    OpenResult r = load(std::move(buf), password);
    if (r == OpenResult::Ok) {
        signatures_ = std::move(sigs); path_ = path; dirty_ = false; undo_.clear(); redo_.clear(); undoBytes_ = 0; }
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
    exitForm();
    if (doc_) api.FPDF_CloseDocument(doc_);
    doc_ = d;
    pages_.assign(n, nullptr);
    texts_.assign(n, nullptr);
    initForm();
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
        case OpenResult::NotPdf: {
            WIN32_FILE_ATTRIBUTE_DATA fa{};
            if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fa) && fa.nFileSizeLow == 0 && fa.nFileSizeHigh == 0)
                return name + L" is empty (0 bytes): there is nothing in it to open. It may not have been saved or copied completely.";
            return name + L" is not a PDF document.";
        }
        case OpenResult::PasswordNeeded: return name + L" is protected. Enter the password to open it.";
        case OpenResult::WrongPassword: return L"That password did not open " + name + L".";
        case OpenResult::Unsupported: return name + L" uses an encryption method that is not supported.";
        case OpenResult::Damaged: return name + L" is damaged or has no pages.";
        default: return L"";
    }
}

namespace { constexpr int kKeepPages = 64; }   // loaded pages kept at most (a few MB of text and drawing data each at worst)

FPDF_PAGE Document::page(int i) {
    if (i < 0 || i >= pageCount()) return nullptr;
    if (lastUse_.size() != pages_.size()) { lastUse_.assign(pages_.size(), 0); pinned_.assign(pages_.size(), 0); }
    if (!pages_[i]) {
        trimPages(i);
        pages_[i] = pdfium::api().FPDF_LoadPage(doc_, i);
        if (pages_[i] && form_) pdfium::api().FORM_OnAfterLoadPage(pages_[i], form_);
    }
    lastUse_[i] = ++useTick_;
    return pages_[i];
}

void Document::pinPage(int i) {
    if (lastUse_.size() != pages_.size()) { lastUse_.assign(pages_.size(), 0); pinned_.assign(pages_.size(), 0); }
    if (i >= 0 && i < static_cast<int>(pinned_.size())) pinned_[i] = 1;
}

void Document::closePage(int i) {
    const auto& api = pdfium::api();
    if (texts_[i]) { api.FPDFText_ClosePage(texts_[i]); texts_[i] = nullptr; }
    if (pages_[i]) {
        if (form_) api.FORM_OnBeforeClosePage(pages_[i], form_);
        api.FPDF_ClosePage(pages_[i]);
        pages_[i] = nullptr;
    }
}

// Before page `keep` is loaded: when too many pages are loaded, the ones unused longest are closed (never `keep` or a
// pinned page). Down to three quarters, so this does not run for every new page.
void Document::trimPages(int keep) {
    std::vector<std::pair<unsigned, int>> loaded;
    for (size_t k = 0; k < pages_.size(); ++k)
        if (pages_[k] && static_cast<int>(k) != keep && !pinned_[k]) loaded.push_back({lastUse_[k], static_cast<int>(k)});
    if (static_cast<int>(loaded.size()) < kKeepPages) return;
    std::sort(loaded.begin(), loaded.end());
    const size_t drop = loaded.size() - static_cast<size_t>(kKeepPages * 3 / 4);
    for (size_t k = 0; k < drop; ++k) closePage(loaded[k].second);
}

void Document::resetText(int i) {
    if (i >= 0 && i < static_cast<int>(texts_.size()) && texts_[i]) {
        pdfium::api().FPDFText_ClosePage(texts_[i]);
        texts_[i] = nullptr;
    }
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

bool Document::serialize(std::vector<unsigned char>& out, int flags) const {
    if (!doc_) return false;
    out.clear();
    Sink sink;
    sink.version = 1;
    sink.WriteBlock = writeBlock;
    sink.out = &out;
    return pdfium::api().FPDF_SaveAsCopy(doc_, &sink, static_cast<FPDF_DWORD>(flags)) != 0 && out.size() > 8;
}

SaveResult Document::writeFile(const std::wstring& path, const std::vector<unsigned char>& bytes) {
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
    return SaveResult::Ok;
}

SaveResult Document::saveAs(const std::wstring& path) {
    std::vector<unsigned char> bytes;
    if (protMode_ == Protection::Keep) {
        if (!serialize(bytes)) return SaveResult::Failed;
    } else {
        // Write the plain document first (PDFium would otherwise carry the old protection over), then protect it ourselves.
        if (!serialize(bytes, FPDF_REMOVE_SECURITY)) return SaveResult::Failed;
        if (protMode_ == Protection::Set) {
            std::vector<unsigned char> enc;
            std::wstring err;
            if (!protect::encrypt(bytes, protOpt_, enc, err)) return SaveResult::Failed;
            bytes = std::move(enc);
        }
    }
    SaveResult r = writeFile(path, bytes);
    if (r == SaveResult::Ok) { path_ = path; dirty_ = false; }
    return r;
}

void Document::trimUndo() {
    constexpr size_t kMaxSteps = 30, kMaxBytes = 300u << 20;
    while (!undo_.empty() && (undo_.size() > kMaxSteps || undoBytes_ > kMaxBytes)) {
        undoBytes_ -= undo_.front().bytes.size();
        undo_.erase(undo_.begin());
    }
}

void Document::beginChange() {
    formSnapValid_ = false;   // an ordinary change replaces any pending field snapshot
    std::vector<unsigned char> snap;
    if (serialize(snap)) {
        undoBytes_ += snap.size();
        undo_.push_back({std::move(snap), protMode_, protOpt_});
        trimUndo();
    }
    redo_.clear();
    dirty_ = true;
    ++revision_;
}

void Document::cancelChange() {
    if (!undo_.empty()) { undoBytes_ -= undo_.back().bytes.size(); undo_.pop_back(); }
    if (undo_.empty() && redo_.empty()) dirty_ = false;   // nothing else was changed in this session
}

bool Document::rollbackChange() {
    if (undo_.empty()) return false;
    Snapshot snap = std::move(undo_.back());
    undo_.pop_back();
    undoBytes_ -= snap.bytes.size();
    const bool ok = restoreFrom(snap);
    if (undo_.empty() && redo_.empty()) dirty_ = false;
    return ok;
}

bool Document::restoreFrom(const Snapshot& snap) {
    std::vector<unsigned char> copy = snap.bytes;
    if (load(std::move(copy), password_) != OpenResult::Ok) return false;
    protMode_ = snap.mode;
    protOpt_ = snap.opt;
    return true;
}

bool Document::undo() {
    if (undo_.empty()) return false;
    std::vector<unsigned char> cur;
    if (!serialize(cur)) return false;
    Snapshot snap = std::move(undo_.back());
    undo_.pop_back();
    undoBytes_ -= snap.bytes.size();
    const Snapshot now{std::move(cur), protMode_, protOpt_};
    if (!restoreFrom(snap)) { undoBytes_ += snap.bytes.size(); undo_.push_back(std::move(snap)); return false; }
    redo_.push_back(now);
    dirty_ = true;
    return true;
}

bool Document::redo() {
    if (redo_.empty()) return false;
    std::vector<unsigned char> cur;
    if (!serialize(cur)) return false;
    Snapshot snap = std::move(redo_.back());
    redo_.pop_back();
    const Snapshot now{std::move(cur), protMode_, protOpt_};
    if (!restoreFrom(snap)) { redo_.push_back(std::move(snap)); return false; }
    undoBytes_ += now.bytes.size();
    undo_.push_back(now);
    dirty_ = true;
    return true;
}

// ---- page operations ----

void Document::rebuildPages() {
    const auto& api = pdfium::api();
    int n = api.FPDF_GetPageCount(doc_);
    sizes_.assign(std::max(n, 0), PageSize{});
    for (int i = 0; i < n; ++i) {
        FS_SIZEF s{};
        if (api.FPDF_GetPageSizeByIndexF(doc_, i, &s) && s.width > 0 && s.height > 0) sizes_[i] = {s.width, s.height};
    }
    pages_.assign(std::max(n, 0), nullptr);
    texts_.assign(std::max(n, 0), nullptr);
    ++revision_;
}

bool Document::deletePages(std::vector<int> pages) {
    std::sort(pages.begin(), pages.end());
    pages.erase(std::unique(pages.begin(), pages.end()), pages.end());
    if (pages.empty() || static_cast<int>(pages.size()) >= pageCount()) return false;
    if (pages.front() < 0 || pages.back() >= pageCount()) return false;
    beginChange();
    closePages();
    for (auto it = pages.rbegin(); it != pages.rend(); ++it) pdfium::api().FPDFPage_Delete(doc_, *it);
    rebuildPages();
    return true;
}

bool Document::insertBlankPage(int after, int templatePage) {
    if (after < -1 || after >= pageCount()) return false;
    PageSize ps = pageSize(std::clamp(templatePage, 0, pageCount() - 1));
    beginChange();
    closePages();
    FPDF_PAGE p = pdfium::api().FPDFPage_New(doc_, after + 1, ps.w, ps.h);
    if (!p) { rebuildPages(); cancelChange(); return false; }
    pdfium::api().FPDF_ClosePage(p);
    rebuildPages();
    return true;
}

bool Document::rotatePages(const std::vector<int>& pages, int turns) {
    if (pages.empty()) return false;
    beginChange();
    closePages();
    const auto& api = pdfium::api();
    for (int i : pages) {
        if (i < 0 || i >= pageCount()) continue;
        FPDF_PAGE p = api.FPDF_LoadPage(doc_, i);
        if (!p) continue;
        int r = ((api.FPDFPage_GetRotation(p) + turns) % 4 + 4) % 4;
        api.FPDFPage_SetRotation(p, r);
        api.FPDF_ClosePage(p);
    }
    rebuildPages();
    return true;
}

bool Document::movePages(std::vector<int> pages, int destination) {
    std::sort(pages.begin(), pages.end());
    pages.erase(std::unique(pages.begin(), pages.end()), pages.end());
    if (pages.empty() || pages.front() < 0 || pages.back() >= pageCount()) return false;
    destination = std::clamp(destination, 0, pageCount());
    beginChange();
    closePages();
    // FPDF_MovePages places the moved pages so that the first one ends at index `dest` of the result.
    int before = 0;
    for (int p : pages) if (p < destination) ++before;
    bool ok = pdfium::api().FPDF_MovePages(doc_, pages.data(), static_cast<unsigned long>(pages.size()), destination - before) != 0;
    rebuildPages();
    if (!ok) { cancelChange(); return false; }
    return true;
}

namespace {

// The object numbers inside an array text: "[3 0 R 7 0 R]" -> 3, 7
std::vector<int> refsInArray(const std::string& a) {
    std::vector<int> out;
    size_t p = 0;
    while (p < a.size()) {
        while (p < a.size() && !pdfedit::File::isDigit(a[p])) ++p;
        if (p >= a.size()) break;
        char* x = nullptr;
        const long n = strtol(a.c_str() + p, &x, 10);
        const char* y = x;
        strtol(y, &x, 10);
        while (*x == ' ' || *x == '\r' || *x == '\n') ++x;
        if (*x == 'R' && x != y) { out.push_back(static_cast<int>(n)); p = static_cast<size_t>(x - a.c_str()) + 1; }
        else p = static_cast<size_t>(y - a.c_str());
    }
    return out;
}

// The /Annots array text of a page dictionary (following one indirect reference), and the object holding it (-1: inline)
bool annotsOf(const pdfedit::File& f, const std::string& page, std::string& array, int& holder) {
    size_t b, e;
    holder = -1;
    if (!pdfedit::valueSpan(page, "/Annots", b, e)) return false;
    array = page.substr(b, e - b);
    if (array.find('[') == std::string::npos) {
        holder = atoi(array.c_str());
        if (!f.object(holder, array)) return false;
    }
    return true;
}

// Link annotations of each page, as dictionary text that stands on its own (an indirect action is copied in)
std::map<int, std::vector<std::string>> linksOf(const std::vector<unsigned char>& pdf, int pages) {
    std::map<int, std::vector<std::string>> out;
    const std::string s(pdf.begin(), pdf.end());
    pdfedit::File f(s);
    std::string cat;
    if (!f.parseXref() || !f.object(pdfedit::refOf(f.trailer, "/Root"), cat)) return out;
    const int tree = pdfedit::refOf(cat, "/Pages");
    for (int i = 0; i < pages; ++i) {
        int idx = i;
        std::set<int> seen;
        const int pg = pdfedit::pageRef(f, tree, idx, seen);
        std::string body, arr;
        int holder;
        if (pg < 0 || !f.object(pg, body) || !annotsOf(f, body, arr, holder)) continue;
        for (int r : refsInArray(arr)) {
            std::string a;
            size_t sb, se;
            if (!f.object(r, a) || !pdfedit::valueSpan(a, "/Subtype", sb, se) || a.compare(sb, 5, "/Link") != 0) continue;
            size_t ab, ae;
            if (pdfedit::valueSpan(a, "/A", ab, ae) && a.find('<', ab) > ae) {   // "/A N 0 R"
                std::string act;
                if (f.object(atoi(a.c_str() + ab), act)) {
                    while (!act.empty() && std::isspace(static_cast<unsigned char>(act.back()))) act.pop_back();
                    size_t lead = act.find_first_not_of(" \r\n\t");
                    a = a.substr(0, ab) + " " + act.substr(lead == std::string::npos ? 0 : lead) + a.substr(ae);
                }
            }
            out[i].push_back(a);
        }
    }
    return out;
}

// Puts the links back on their pages (as new objects), as an incremental update
bool reattachLinks(std::vector<unsigned char>& pdf, const std::map<int, std::vector<std::string>>& links) {
    const std::string s(pdf.begin(), pdf.end());
    pdfedit::File f(s);
    std::string cat;
    if (!f.parseXref() || !f.object(pdfedit::refOf(f.trailer, "/Root"), cat)) return false;
    const int tree = pdfedit::refOf(cat, "/Pages");
    std::map<int, std::string> write;
    int next = f.size;
    for (const auto& kv : links) {
        int idx = kv.first;
        std::set<int> seen;
        const int pg = pdfedit::pageRef(f, tree, idx, seen);
        std::string body;
        if (pg < 0 || !f.object(pg, body)) continue;
        std::string refs;
        for (const std::string& a : kv.second) { write[next] = a; refs += std::to_string(next++) + " 0 R "; }
        std::string arr;
        int holder;
        if (annotsOf(f, body, arr, holder) && arr.rfind(']') != std::string::npos) {
            arr.insert(arr.rfind(']'), " " + refs);
            if (holder >= 0) write[holder] = arr;
            else write[pg] = pdfedit::setKey(body, "/Annots", arr.substr(arr.find('[')));
        } else write[pg] = pdfedit::setKey(body, "/Annots", "[" + refs + "]");
    }
    if (write.empty()) return true;
    pdf = pdfedit::appendIncremental(s, f, write, next);
    return true;
}

} // namespace

int Document::flattenAll() {
    // PDFium drops every annotation of a flattened page, links too; they are not part of its look, so they come back
    std::vector<unsigned char> before;
    std::map<int, std::vector<std::string>> links;
    if (serialize(before)) links = linksOf(before, pageCount());
    beginChange();
    int changed = 0;
    for (int i = 0; i < pageCount(); ++i) {
        FPDF_PAGE p = page(i);
        if (p && pdfium::api().FPDFPage_Flatten(p, FLAT_PRINT) == FLATTEN_SUCCESS) ++changed;
    }
    std::vector<unsigned char> bytes;
    if (!changed || !serialize(bytes) || (!links.empty() && !reattachLinks(bytes, links)) || load(std::move(bytes), password_) != OpenResult::Ok) { if (changed) undo(); else cancelChange(); return 0; }
    return changed;
}

bool Document::reversePages() {
    const int n = pageCount();
    if (n < 2) return false;
    std::vector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = n - 1 - i;
    beginChange();
    closePages();
    bool ok = pdfium::api().FPDF_MovePages(doc_, order.data(), static_cast<unsigned long>(n), 0) != 0;
    rebuildPages();
    if (!ok) { cancelChange(); return false; }
    return true;
}

bool Document::cropPages(const std::vector<int>& pages, double l, double b, double r, double t) {
    if (pages.empty()) return false;
    beginChange();
    closePages();
    const auto& api = pdfium::api();
    bool any = false;
    for (int i : pages) {
        FPDF_PAGE p = api.FPDF_LoadPage(doc_, i);
        if (!p) continue;
        float ml, mb, mr, mt;
        if (!api.FPDFPage_GetMediaBox(p, &ml, &mb, &mr, &mt)) { ml = 0; mb = 0; mr = pageSize(i).w; mt = pageSize(i).h; }
        float cl, cb, cr, ct;
        if (!api.FPDFPage_GetCropBox(p, &cl, &cb, &cr, &ct)) { cl = ml; cb = mb; cr = mr; ct = mt; }
        float nl = cl + static_cast<float>(l), nb = cb + static_cast<float>(b), nr = cr - static_cast<float>(r), nt = ct - static_cast<float>(t);
        if (nr - nl >= 36 && nt - nb >= 36) { api.FPDFPage_SetCropBox(p, nl, nb, nr, nt); any = true; }
        api.FPDF_ClosePage(p);
    }
    rebuildPages();
    if (!any) { cancelChange(); return false; }
    return true;
}

bool Document::importPages(Document& src, const std::vector<int>& srcPages, int at) {
    if (srcPages.empty() || !src.handle() || at < 0 || at > pageCount()) return false;
    beginChange();
    closePages();
    bool ok = pdfium::api().FPDF_ImportPagesByIndex(doc_, src.handle(), srcPages.data(),
                                                    static_cast<unsigned long>(srcPages.size()), at) != 0;
    rebuildPages();
    if (!ok) { cancelChange(); return false; }
    return true;
}

bool Document::pagesToBytes(const std::vector<int>& pages, std::vector<unsigned char>& out) const {
    out.clear();
    if (pages.empty() || !doc_) return false;
    const auto& api = pdfium::api();
    FPDF_DOCUMENT tmp = api.FPDF_CreateNewDocument();
    if (!tmp) return false;
    bool ok = false;
    if (api.FPDF_ImportPagesByIndex(tmp, doc_, pages.data(), static_cast<unsigned long>(pages.size()), 0)) {
        Sink sink;
        sink.version = 1; sink.WriteBlock = writeBlock; sink.out = &out;
        ok = api.FPDF_SaveAsCopy(tmp, &sink, 0) && out.size() > 8;
    }
    api.FPDF_CloseDocument(tmp);
    return ok;
}

SaveResult Document::extractPages(const std::vector<int>& pages, const std::wstring& path) const {
    std::vector<unsigned char> bytes;
    if (!pagesToBytes(pages, bytes)) return SaveResult::Failed;
    return writeFile(path, bytes);
}

std::vector<int> Document::parseRange(const std::wstring& text) const {
    std::vector<int> out;
    const int n = pageCount();
    size_t i = 0;
    auto skip = [&] { while (i < text.size() && (text[i] == L' ' || text[i] == L'\t')) ++i; };
    auto number = [&](int& v) {
        skip();
        if (i >= text.size() || !iswdigit(text[i])) return false;
        long long x = 0;
        while (i < text.size() && iswdigit(text[i])) { x = x * 10 + (text[i] - L'0'); if (x > 1000000) return false; ++i; }
        v = static_cast<int>(x);
        return true;
    };
    for (;;) {
        int a = 0, b = 0;
        skip();
        bool openStart = false;
        if (i < text.size() && text[i] == L'-') openStart = true; else if (!number(a)) return {};
        if (openStart) a = 1;
        skip();
        if (i < text.size() && text[i] == L'-') {
            ++i; skip();
            if (i >= text.size() || text[i] == L',' || text[i] == L';') b = n; else if (!number(b)) return {};
        } else b = a;
        if (a < 1 || b < a || b > n) return {};
        for (int p = a; p <= b; ++p) out.push_back(p - 1);
        skip();
        if (i >= text.size()) break;
        if (text[i] == L',' || text[i] == L';') { ++i; continue; }
        return {};
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

bool Document::setProtection(const protect::Options& opt) {
    if (!doc_) return false;
    beginChange();
    protMode_ = Protection::Set;
    protOpt_ = opt;
    return true;
}

bool Document::removeProtection() {
    if (!doc_ || !isProtected()) return false;
    beginChange();
    protMode_ = Protection::Remove;
    return true;
}

bool Document::blankBytes(const std::vector<std::pair<double, double>>& sizes, std::vector<unsigned char>& out) {
    out.clear();
    const auto& api = pdfium::api();
    FPDF_DOCUMENT tmp = api.FPDF_CreateNewDocument();
    if (!tmp) return false;
    for (size_t i = 0; i < sizes.size(); ++i) {
        FPDF_PAGE p = api.FPDFPage_New(tmp, static_cast<int>(i), sizes[i].first, sizes[i].second);
        if (p) api.FPDF_ClosePage(p);
    }
    Sink sink;
    sink.version = 1; sink.WriteBlock = writeBlock; sink.out = &out;
    bool ok = api.FPDF_SaveAsCopy(tmp, &sink, 0) && out.size() > 8;
    api.FPDF_CloseDocument(tmp);
    return ok;
}

bool Document::cropBoxOf(int page, double& l, double& b, double& r, double& t) {
    FPDF_PAGE p = this->page(page);
    if (!p) return false;
    const auto& api = pdfium::api();
    float ml, mb, mr, mt;
    if (!api.FPDFPage_GetMediaBox(p, &ml, &mb, &mr, &mt)) { ml = 0; mb = 0; mr = static_cast<float>(pageSize(page).w); mt = static_cast<float>(pageSize(page).h); }
    float cl, cb, cr, ct;
    if (!api.FPDFPage_GetCropBox(p, &cl, &cb, &cr, &ct)) { cl = ml; cb = mb; cr = mr; ct = mt; }
    l = cl; b = cb; r = cr; t = ct;
    return true;
}
