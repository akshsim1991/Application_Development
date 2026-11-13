// Feather Clipboard Manager 2.1 - shared declarations.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <algorithm>
using std::min;
using std::max;
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <cstdint>
#include <ctime>
#include "core.h"

#define APP_NAME L"Feather Clipboard Manager"
#define APP_VERSION L"2.1.1"
#define APP_VERSION_SHORT L"2.1"
#define APP_AUTHOR L"Akshaya Simha"
#define APP_YEAR L"2026"

// ------------------------------------------------------------------ util.cpp
std::wstring Fmt(const wchar_t* f, ...);
std::wstring Num(long long v);
bool ReadFileBytes(const std::wstring& path, std::string& out);
bool WriteFileAtomic(const std::wstring& path, const void* data, size_t n);
bool FileExistsW(const std::wstring& p);
uint64_t FileSizeW(const std::wstring& p);
bool EnsureDir(const std::wstring& p);
std::wstring JoinPath(const std::wstring& a, const std::wstring& b);
std::wstring ExePath();
std::wstring ExeDir();
std::wstring KnownFolder(REFKNOWNFOLDERID id);
std::wstring ProcessNameOf(HWND h);          // e.g. "chrome.exe" ("" when unknown)
std::wstring FriendlyName(const std::wstring& exe);   // "chrome.exe" -> "chrome"
std::wstring FormatTime(time_t t);           // "Today 14:32", "Yesterday 09:10", "Mon 03 Nov, 11:20"
std::wstring FormatDateTime(time_t t);       // full
std::wstring FormatBytes(uint64_t b);
void OpenShell(HWND owner, const std::wstring& what, const wchar_t* verb = L"open");
void ShowInExplorer(const std::wstring& file);
std::wstring TextAt(HWND h);
void LogEvent(const std::wstring& message);       // local, content-free, bounded; only when logging is on
std::wstring ReadLog();
void SetFontAll(HWND parent, HFONT f);
bool GetIntReg(const wchar_t* name, int& v);
void SetIntReg(const wchar_t* name, int v);
bool GetStrReg(const wchar_t* name, std::wstring& v);
void SetStrReg(const wchar_t* name, const std::wstring& v);
bool GetBlobReg(const wchar_t* name, void* data, DWORD size);
void SetBlobReg(const wchar_t* name, const void* data, DWORD size);

// ------------------------------------------------------------------ lang.cpp
enum Lang { LANG_EN = 0, LANG_HI = 1, LANG_ES = 2, LANG_COUNT = 3 };
void SetLanguage(int l);
const wchar_t* T(const wchar_t* english);     // translation or the English text
const wchar_t* LanguageName(int l);

// ------------------------------------------------------------------ settings
struct Settings {
    int theme = 0;                 // 0 same as Windows, 1 light, 2 dark
    int hotkey = 0;                // 0 Ctrl+Shift+V, 1 Ctrl+Alt+V, 2 Win+Shift+V, 3 none
    int hotkeyPlain = 0;           // 0 Ctrl+Alt+P, 1 Ctrl+Shift+Alt+V, 2 Win+Alt+V, 3 none  (paste newest as plain text)
    bool quickNumbers = true;      // Ctrl+Alt+1..9 pastes the Nth newest item from any program
    bool pasteOnEnter = true;
    bool recordImages = true;
    bool recordFiles = true;
    bool recordRich = true;        // keep HTML / RTF formatting of copied text
    bool ocr = false;              // read text in pictures so they can be searched (an optional module: off until you switch it on)
    bool startInTray = false;
    bool closeToTray = true;
    int deleteAfterDays = 0;       // 0 = keep forever
    int maxItems = 0;              // 0 = no limit
    int maxMB = 0;                 // 0 = no limit
    int sensitive = 2;             // 0 keep, 1 do not record, 2 record but delete after expireSeconds
    int expireSeconds = 60;
    std::wstring ignoreList = L"KeePass.exe\r\nKeePassXC.exe\r\n1Password.exe\r\nBitwarden.exe\r\nDashlane.exe\r\nLastPass.exe";
    bool sound = false;
    bool toast = false;
    bool popupAtCursor = true;
    bool compact = false;
    int language = 0;
    int filter = 0;
    int listWidth = 0;
    bool askedAutostart = false;
    bool toldAboutTray = false;
    bool checkUpdates = true;      // look for a new version when the program starts (like Feather PDF)
    std::wstring skipVersion;      // "do not remind me about this version"
    int maxEntryMB = 20;           // a single copy larger than this is not kept (0 = no limit)
    bool maskPreviews = false;     // mask emails, phone numbers and tokens in the list and preview
    int fontSize = 1;              // 0 small, 1 normal, 2 large
    int accent = 0;                // highlight colour: 0 blue, 1 green, 2 purple, 3 orange, 4 red
    int previewLen = 1;            // list text length: 0 short, 1 medium, 2 long
    bool alwaysOnTop = false;
    bool logging = true;           // content-free local diagnostic log (errors only)
    std::wstring pipelines = L"Clean paste = trim | blanklines | oneline\r\nTidy JSON = json\r\nShout = upper";
    bool autostart = false;        // mirrors the Run key (read at startup)
    void Load();
    void Save() const;
};
extern Settings g_set;
bool IsIgnoredProgram(const std::wstring& exe);

// ------------------------------------------------------------------ store.cpp
enum ItemKind { K_TEXT = 0, K_PICTURE = 1, K_FILES = 2, K_SNIPPET = 3 };
enum ItemFlags { F_HTML = 1, F_RTF = 2, F_OCR = 4, F_OCRTRIED = 8 };

struct Item {
    uint64_t id = 0;
    int kind = K_TEXT;
    bool pinned = false;
    int64_t created = 0;
    int64_t deleted = 0;           // time moved to "Recently deleted" (0 = not deleted)
    int64_t expires = 0;           // sensitive items: time they delete themselves (0 = never)
    std::wstring base;             // file name without extension
    std::wstring source;           // program the item came from, e.g. chrome.exe
    std::wstring group;
    std::wstring title;            // snippet name
    uint64_t hash = 0;
    int w = 0, h = 0;
    uint64_t bytes = 0;
    unsigned flags = 0;
    int dropEffect = 1;
    int count = 0;                 // characters (text) or number of files
    int lines = 0;
    // memory only
    std::wstring head;             // single line, first ~1000 characters (text) / names (files) / OCR text (pictures)
    bool truncated = false;        // head is cut short (very long text): search also reads the file
    core::SmartType st = core::ST_NONE;
};

class Store {
public:
    std::vector<Item> items;       // oldest first
    std::wstring dir;              // ...\Clipboard contents
    std::wstring dataRoot;         // ...\ClipboardManager
    std::wstring note;             // problem text for the user when the store could not open
    bool Init();
    void Save();                                            // compact rewrite of the whole journal
    void Touch(const Item& it);                             // append one item's current state
    void Tombstone(const std::vector<uint64_t>& ids);       // append deletion marks
    static bool ParseIndexLine(const std::wstring& line, Item& it);
    Item* Find(uint64_t id);
    std::wstring PathFor(const Item& it, const wchar_t* ext) const;       // dir\base+ext
    std::wstring MainPath(const Item& it) const;
    std::wstring ReadText(const Item& it) const;
    std::vector<std::wstring> ReadFiles(const Item& it) const;
    std::wstring ReadOcr(const Item& it) const;
    std::string ReadSide(const Item& it, const wchar_t* ext) const;
    // returns id; *isNew false when an identical newest item was only bumped.
    uint64_t AddText(const std::wstring& text, const std::string& html, const std::string& rtf, const std::wstring& source, int64_t expires, bool* isNew = nullptr);
    uint64_t AddPictureFile(const std::wstring& tmpPng, int w, int h, uint64_t hash, const std::wstring& source, bool* isNew = nullptr);
    uint64_t AddFiles(const std::vector<std::wstring>& paths, int dropEffect, const std::wstring& source, bool* isNew = nullptr);
    uint64_t AddSnippet(const std::wstring& name, const std::wstring& text);
    bool UpdateText(uint64_t id, const std::wstring& text);
    bool SetOcr(uint64_t id, const std::wstring& text);
    void SetPinned(uint64_t id, bool p);
    void SetGroup(uint64_t id, const std::wstring& g);
    void SoftDelete(const std::vector<uint64_t>& ids);
    void Restore(const std::vector<uint64_t>& ids);
    void Purge(const std::vector<uint64_t>& ids);          // delete for good, with files
    void PurgeAllDeleted();
    void DeleteAllUnpinned();
    int CountUnpinned() const;
    bool CheckIntegrity(std::wstring& report, bool repair);   // missing files, stray files; repair drops records without a file
    bool Cleanup();                                        // age, count, size, expiry, old trash. true when something changed
    std::vector<std::wstring> Groups() const;
    uint64_t TotalBytes() const;
    uint64_t NextId();
    std::wstring NewBase();
    void RemoveFiles(const Item& it);
    bool Matches(const Item& it, const core::Query& q) const;   // words, "phrases", ~fuzzy, re:regex, app: type: tag: date:
    uint64_t lastId = 0;
    bool dirty = false;
};
extern Store g_store;
std::wstring ItemTitle(const Item& it);                    // line shown in the list

// ------------------------------------------------------------------ clip.cpp
struct Captured {
    int kind = -1;                  // K_TEXT, K_PICTURE, K_FILES or -1 when nothing to keep
    std::wstring text;
    std::string html, rtf;
    std::vector<std::wstring> files;
    int dropEffect = 1;
    std::wstring tmpPng;            // picture written to a temp file
    int w = 0, h = 0;
    uint64_t hash = 0;
};
extern UINT g_cfHtml, g_cfRtf, g_cfPng, g_cfDropEffect, g_cfExclude, g_cfCanHistory, g_cfViewerIgnore, g_cfPngMime;
void ClipInit();
bool ClipCapture(HWND owner, Captured& out, bool wantImages, bool wantFiles, bool wantRich, bool& privacyFlagged);
// Put an item (or several joined text items) on the clipboard. plain = text only. Returns 0 ok or a message id.
enum PutResult { PUT_OK = 0, PUT_FAILED, PUT_PICTURE_ONE, PUT_TEXT_ONLY, PUT_FILES_MISSING };
PutResult ClipPutItem(HWND owner, const Item& it, bool plain);
PutResult ClipPutText(HWND owner, const std::wstring& text);
PutResult ClipPutBitmapFile(HWND owner, const std::wstring& pngPath);
bool PngToSizedCopy(const std::wstring& srcPng, const std::wstring& dstPath, const wchar_t* mime, int percent);
bool GdiplusStart();
CLSID EncoderFor(const wchar_t* mime);
bool LoadImageSize(const std::wstring& path, int& w, int& h);
extern DWORD g_ignoreSeq;                       // clipboard sequence number of our own last write

// ------------------------------------------------------------------ backup.cpp
bool BackupExport(const std::wstring& file, const std::wstring& password, std::wstring& err);
bool BackupImport(const std::wstring& file, const std::wstring& password, int& added, std::wstring& err);

// ------------------------------------------------------------------ updater.cpp
// Like Feather PDF: when the program starts it reads latest.json from the public releases repository, and if a newer
// version is out it asks "Update now" or later. The installer is downloaded, checked (SHA-256), and run.
// Only that one small file is read; nothing about you or your history is ever sent. It can be switched off in Settings.
#define UPDATE_LATEST_URL "https://raw.githubusercontent.com/akshsim1991/FeatherClipboardManager-releases/main/latest.json"
#define UPDATE_RELEASES_PAGE L"https://github.com/akshsim1991/FeatherClipboardManager-releases"
struct UpdateInfo {
    core::Latest rel;
    std::wstring problem;          // set instead when the version file could not be read: why
    std::string via;               // the server that answered; the download tries it first
};
void UpdateCheckAsync(HWND notify, bool manual);     // posts WM_UPDATE_RESULT (wParam: manual, lParam: new UpdateInfo*, receiver deletes it)
bool UpdateDownload(const std::string& url, std::string& out, std::wstring& err, const std::string& via, const std::function<bool(long long, long long)>& progress);
bool UpdatePrompt(HWND parent, const UpdateInfo& u, bool& skipThis);          // true: update now
bool DownloadWithProgress(HWND parent, const std::wstring& title, const std::string& url, const std::string& via, std::string& out, std::wstring& err);

// ------------------------------------------------------------------ ocr.cpp
struct OcrResult { uint64_t id; std::wstring text; };
void OcrStart(HWND notify, uint64_t id, const std::wstring& pngPath);   // posts WM_OCR_DONE (lParam: heap OcrResult*, receiver deletes it)
bool OcrSelfCheckScript(std::wstring& scriptPath);

// ------------------------------------------------------------------ ui
struct Theme {
    bool dark = false;
    COLORREF bg, panel, text, sub, border, accent, sel, selText, btn, btnHot, edit;
};
extern Theme g_th;
void ApplyTheme(bool force = false);
bool SystemUsesDark();
extern HFONT g_font, g_fontBold, g_fontSmall, g_fontIcon;
extern HINSTANCE g_inst;
extern HWND g_main;

enum { CMD_EXPORT = 1950, CMD_IMPORT = 1951, CMD_UPDATE = 1952 };   // sent by the settings dialog to the main window

enum {
    WM_TRAY = WM_APP + 1,
    WM_OCR_DONE = WM_APP + 2,
    WM_UPDATE_RESULT = WM_APP + 5,
    WM_SHOW_REQUEST = WM_APP + 3,
    WM_EXIT_REQUEST = WM_APP + 4,
    WM_PASTE_NOW = WM_APP + 6,
};

int RunApp(HINSTANCE inst, const std::wstring& cmdline);
int RunBench(int n, const std::wstring& outFile);   // --bench N file: speed of the journal and of search with N synthetic items (needs FCM_DATA_ROOT)
int RunSeedDemo();                                // --seed-demo (needs FCM_DATA_ROOT): sample items for screenshots
int RunSelfTest(const std::wstring& outFile);     // --selftest <file>: checks storage, search, backup and clipboard code; exit code 0 = all passed
void ShowSettingsDialog(HWND parent);
bool EditTextDialog(HWND parent, const wchar_t* title, std::wstring* name, std::wstring& text, bool multiline);
void ShowImageEditor(HWND parent, const Item& it);
void ShowAbout(HWND parent);
void ShowClipboardFormats(HWND parent);
void ShowShortcuts(HWND parent);
void SetDialogLabels(const wchar_t* l1, const wchar_t* l2);   // labels for the next EditTextDialog call
void ShowStats(HWND parent);
void MsgInfo(HWND parent, const std::wstring& text, UINT icon = MB_ICONINFORMATION);
bool MsgAsk(HWND parent, const std::wstring& text);
HBRUSH ThemeBrush(int which);                    // 0 window background, 1 panel, 2 edit
bool PasswordPrompt(HWND parent, const wchar_t* title, std::wstring& pw);
void TextBoxDialog(HWND parent, const wchar_t* title, const std::wstring& text);
LRESULT DrawThemedButton(const DRAWITEMSTRUCT* d, bool glyphFont);
void SettingsChanged();                          // implemented by the main window
