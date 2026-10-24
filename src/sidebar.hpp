#pragma once
#include <windows.h>
#include <commctrl.h>
#include <map>
#include "document.hpp"

// Left-hand panel with two views of the open document: bookmarks (outline) and page thumbnails.
class Sidebar {
public:
    enum class Mode { Bookmarks, Pages };
    static constexpr const wchar_t* kClass = L"FeatherSidebar";
    static constexpr const wchar_t* kThumbClass = L"FeatherThumbs";
    static constexpr int kHeaderH = 30;

    static bool registerClasses(HINSTANCE inst);
    HWND create(HWND parent, HINSTANCE inst);
    HWND hwnd() const { return hwnd_; }

    void setDocument(Document* d);
    void setMode(Mode m);
    Mode mode() const { return mode_; }
    void refresh();                                      // the document changed: redraw thumbnails and bookmarks
    void setCurrentPage(int page);                       // highlights it in the thumbnails
    void setOnNavigate(void (*cb)(void*, int), void* ctx) { onNav_ = cb; onNavCtx_ = ctx; }

private:
    static LRESULT CALLBACK proc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK thumbProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(UINT, WPARAM, LPARAM);
    LRESULT handleThumbs(UINT, WPARAM, LPARAM);
    void layout();
    void fillTree();
    void addTreeItems(HTREEITEM parent, const std::vector<Bookmark>& list);
    void thumbLayout();
    void thumbPaint();
    void clearThumbs();
    void navigate(int page) { if (onNav_ && page >= 0) onNav_(onNavCtx_, page); }

    HWND hwnd_ = nullptr, tree_ = nullptr, thumbs_ = nullptr;
    Document* doc_ = nullptr;
    Mode mode_ = Mode::Bookmarks;
    int hasBookmarks_ = 0;
    int current_ = 0;
    // thumbnails
    std::vector<int> tops_, heights_;
    int totalH_ = 0, scroll_ = 0;
    std::map<int, HBITMAP> thumbCache_;
    void (*onNav_)(void*, int) = nullptr;
    void* onNavCtx_ = nullptr;
    HFONT font_ = nullptr;
    static constexpr int kThumbW = 140, kPad = 12, kLabelH = 18;
};
