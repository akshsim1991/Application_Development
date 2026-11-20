#include "app.h"
#include "../core/text.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fsu {
using std::wstring;

void SetDarkTitleBar(HWND h, bool dark) { BOOL v = dark; DwmSetWindowAttribute(h, 20, &v, sizeof v); }

// ---------- shell actions ----------
static BOOL CALLBACK FindExplorerWnd(HWND h, LPARAM l) {   // bring the Explorer window we just opened to the front (it often opens behind a maximized/elevated app)
    auto* want = (std::pair<std::wstring, HWND>*)l; wchar_t cls[64], title[512]; GetClassNameW(h, cls, 64); if (wcscmp(cls, L"CabinetWClass") != 0 || !IsWindowVisible(h)) return TRUE; GetWindowTextW(h, title, 512);
    if (_wcsicmp(title, want->first.c_str()) == 0) { want->second = h; return FALSE; } return TRUE;
}
static bool ShowExplorerFor(const wstring& folderLeaf, const wstring& folderFull, int waitMs) {   // true when an Explorer window for the folder is visible; brings it to the front
    std::pair<std::wstring, HWND> w{folderLeaf, nullptr}; std::pair<std::wstring, HWND> w2{folderFull, nullptr};
    for (int i = 0; i < waitMs / 100 && !w.second && !w2.second; ++i) { Sleep(100); EnumWindows(FindExplorerWnd, (LPARAM)&w); if (!w.second) EnumWindows(FindExplorerWnd, (LPARAM)&w2); }
    HWND h = w.second ? w.second : w2.second; if (!h) return false;
    if (IsIconic(h)) ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); BringWindowToTop(h); FLASHWINFO fi{sizeof fi, h, FLASHW_ALL | FLASHW_TIMERNOFG, 3, 0}; FlashWindowEx(&fi); return true;
}
// Opens Explorer on the file's folder with the file selected. Each method is verified by looking for the Explorer window; if it did not appear the next method is tried.
bool OpenFileLocationEx(const wstring& program, wstring& err) {
    wstring p = program.find(L'%') != wstring::npos ? ExpandEnv(program) : program; while (!p.empty() && (p.back() == L'"' || (p.size() > 3 && p.back() == L'\\'))) p.pop_back(); if (!p.empty() && p.front() == L'"') p.erase(p.begin());
    std::replace(p.begin(), p.end(), L'/', L'\\'); wchar_t canon[MAX_PATH * 2]; if (!p.empty() && PathCanonicalizeW(canon, p.c_str())) p = canon;
    DWORD attr = p.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(p.c_str()); bool isDir = attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    size_t sl = p.find_last_of(L'\\'); wstring dir = isDir ? p : (sl == wstring::npos ? L"" : p.substr(0, sl)); if (dir.size() == 2 && dir[1] == L':') dir += L"\\";
    if (dir.empty() || GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES) { err = L"The file or its folder was not found:\r\n" + p; return false; }
    wstring leaf = dir.size() <= 3 ? dir : dir.substr(dir.find_last_of(L'\\') + 1); AllowSetForegroundWindow(ASFW_ANY);
    auto spawn = [](wstring cl) { STARTUPINFOW si{sizeof si}; PROCESS_INFORMATION pi{}; if (!CreateProcessW(nullptr, &cl[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return false; CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return true; };
    if (attr != INVALID_FILE_ATTRIBUTES && !isDir) {   // 1) explorer.exe /select,"file"  (needs no COM and works from any thread)
        if (spawn(L"explorer.exe /select,\"" + p + L"\"") && ShowExplorerFor(leaf, dir, 3000)) return true;
        if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(p.c_str())) { HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0); ILFree(pidl); if (SUCCEEDED(hr) && ShowExplorerFor(leaf, dir, 2500)) return true; }   // 2) shell API
    }
    if (spawn(L"explorer.exe \"" + dir + L"\"") && ShowExplorerFor(leaf, dir, 3000)) return true;   // 3) at least the folder
    if ((INT_PTR)ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32) return true;
    err = L"Explorer could not be opened for:\r\n" + dir; return false;
}
void OpenFileLocation(const wstring& program) { wstring err; if (!OpenFileLocationEx(program, err)) DarkMsg(GetActiveWindow(), L"Open file location", err, 0); }
void ShowInRegistryEditor(const wstring& loc) {
    if (loc.rfind(L"Task Scheduler:", 0) == 0) { ShellExecuteW(nullptr, L"open", L"taskschd.msc", nullptr, nullptr, SW_SHOWNORMAL); return; }
    if (loc.rfind(L"HKCU", 0) != 0 && loc.rfind(L"HKLM", 0) != 0) { ShellExecuteW(nullptr, L"open", ExpandEnv(loc).c_str(), nullptr, nullptr, SW_SHOWNORMAL); return; }   // startup folder
    wstring l = loc; bool v32 = false; size_t br = l.find(L" ["); if (br != wstring::npos) { v32 = l.find(L"32-bit") != wstring::npos; l = l.substr(0, br); }
    wstring root = l.substr(0, 4) == L"HKCU" ? L"HKEY_CURRENT_USER" : L"HKEY_LOCAL_MACHINE"; wstring rest = l.substr(4);
    if (v32 && root == L"HKEY_LOCAL_MACHINE" && rest.rfind(L"\\Software\\", 0) == 0) rest = L"\\Software\\WOW6432Node" + rest.substr(9);
    wstring full = L"Computer\\" + root + rest;
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, L"LastKey", 0, REG_SZ, (const BYTE*)full.c_str(), (DWORD)((full.size() + 1) * 2)); RegCloseKey(k); }
    ShellExecuteW(nullptr, L"open", L"regedit.exe", nullptr, nullptr, SW_SHOWNORMAL);
}
static wstring Sha256(const wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return L"";
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hh = nullptr; wstring out;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 && BCryptCreateHash(alg, &hh, nullptr, 0, nullptr, 0, 0) == 0) {
        std::vector<BYTE> buf(1 << 16); DWORD n; bool ok = true;
        while (ReadFile(f, buf.data(), (DWORD)buf.size(), &n, nullptr) && n) if (BCryptHashData(hh, buf.data(), n, 0)) { ok = false; break; }
        BYTE d[32]; if (ok && BCryptFinishHash(hh, d, 32, 0) == 0) for (BYTE b : d) { wchar_t t[3]; swprintf(t, 3, L"%02x", b); out += t; }
    }
    if (hh) BCryptDestroyHash(hh); if (alg) BCryptCloseAlgorithmProvider(alg, 0); CloseHandle(f); return out;
}
static wstring UrlEncode(const wstring& w) {
    std::string u = ToUtf8(w); wstring o;
    for (unsigned char c : u) { if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~') o += (wchar_t)c; else { wchar_t t[4]; swprintf(t, 4, L"%%%02X", c); o += t; } }
    return o;
}
void OnlineLookup(HWND owner, bool vt, const wstring& program, const wstring& name) {
    wstring url, what;
    if (vt) {
        wstring h = Sha256(ExpandEnv(program));
        if (h.empty()) { DarkMsg(owner, L"Check on VirusTotal", L"The file cannot be read, so its fingerprint cannot be computed.", 0); return; }
        url = L"https://www.virustotal.com/gui/file/" + h; what = L"This computes the file's SHA-256 here and opens VirusTotal in your web browser with that hash. The file itself is not uploaded.";
    } else { url = L"https://www.bing.com/search?q=" + UrlEncode(name.empty() ? program : name); what = L"This opens your web browser and sends the name \"" + name + L"\" to Bing."; }
    wstring msg = what + L"\r\n\r\nFeather System Utilities itself stays offline. Continue?";
    if (DarkMsg(owner, vt ? L"Check on VirusTotal" : L"Search online", msg, 2) == IDYES)
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

wstring StartupDetailsText(const StartupEntry& e) {
    wstring t = e.name + L"\r\n\r\nSTATUS\r\n" + e.state + L"\r\nStarts: " + e.starts + L"\r\n\r\nWHERE IT IS SET\r\nType: " + e.type + L"\r\nFor: " + e.scope + L"\r\n";
    if (e.type.rfind(L"Registry", 0) == 0) t += L"Registry key: " + e.location + L"\r\nValue name: " + e.valueName + L"\r\n";
    else if (e.type.rfind(L"Scheduled task", 0) == 0) t += L"Task: " + e.taskPath + L"\r\n";
    else if (e.type == L"Startup folder") t += L"Folder: " + e.location + L"\r\n";
    else t += L"Registry key: " + e.location + L"\r\nCOM class: " + e.clsid + L"\r\n";
    t += L"First seen: " + e.firstSeen + (e.managedDelay ? L"\r\nDelay: managed by Feather System Utilities (original Run entry is kept, disabled, and restored by \"Remove delay\")" : L"") + L"\r\n\r\nWHAT IT RUNS\r\nCommand: " + e.command + L"\r\nProgram: " + e.program + L"\r\nPublisher: " + e.publisher + L"\r\nVersion: " + e.version +
         L"\r\nDigital signature: " + e.signature + L"\r\n\r\nWARNINGS\r\n" + (e.warnings.empty() ? wstring(L"None.") : e.warnings) + L"\r\n";
    return t;
}

// ---------- report dialog (warm light theme, as in the original apps) ----------
struct RD { std::vector<ReportTab> tabs; std::vector<ReportBtn> btns; HWND edit = nullptr, tab = nullptr; std::vector<HWND> bs; HFONT font = nullptr; HBRUSH br = nullptr; bool done = false; int closeId = 0; };
static const COLORREF kWarmBg = RGB(0xEA, 0xDB, 0xC8), kWarmTx = RGB(0x2B, 0x22, 0x18);

static LRESULT CALLBACK RDProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    RD* d = (RD*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); break;
    case WM_SIZE: if (d && d->edit) {
        int W = LOWORD(l), H = HIWORD(l), pad = 12, bh = 28, top = pad;
        // buttons wrap onto rows by text width
        std::vector<int> bw; int rows = 1, x = pad; for (size_t i = 0; i < d->bs.size(); ++i) { wchar_t t[128]; GetWindowTextW(d->bs[i], t, 128); int w2 = 28 + 7 * (int)wcslen(t); if (w2 < 80) w2 = 80; bw.push_back(w2); }
        for (int w2 : bw) { if (x + w2 > W - pad && x > pad) { ++rows; x = pad; } x += w2 + 6; }
        int btnArea = rows * (bh + 6) + pad;
        if (d->tab) { MoveWindow(d->tab, pad, pad, W - 2 * pad, 26, TRUE); top = pad + 30; }
        MoveWindow(d->edit, pad, top, W - 2 * pad, H - top - btnArea - pad, TRUE);
        x = pad; int row = 0; for (size_t i = 0; i < d->bs.size(); ++i) { if (x + bw[i] > W - pad && x > pad) { ++row; x = pad; } MoveWindow(d->bs[i], x, H - btnArea - pad / 2 + 6 + row * (bh + 6), bw[i], bh, TRUE); x += bw[i] + 6; }
        return 0; } break;
    case WM_NOTIFY: if (d && ((NMHDR*)l)->hwndFrom == d->tab && ((NMHDR*)l)->code == TCN_SELCHANGE) { int s = TabCtrl_GetCurSel(d->tab); if (s >= 0 && s < (int)d->tabs.size()) SetWindowTextW(d->edit, d->tabs[s].text.c_str()); } return 0;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: { HDC dc = (HDC)w; SetTextColor(dc, kWarmTx); SetBkColor(dc, kWarmBg); return (LRESULT)d->br; }
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, d->br); return 1; }
    case WM_COMMAND: if (d) { int id = LOWORD(w);
        if (id == IDCANCEL) { d->done = true; return 0; }
        if (id >= 2000 && id < 2000 + (int)d->btns.size()) { wstring cur; wchar_t b[65536]; GetWindowTextW(d->edit, b, 65536); cur = b; auto fn = d->btns[id - 2000].fn; if (fn) fn(cur); if (!fn || d->btns[id - 2000].closes) d->done = true; } return 0; } break;
    case WM_CLOSE: if (d) d->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ShowReport(HWND owner, const wstring& title, const std::vector<ReportTab>& tabs, const std::vector<ReportBtn>& btns) {
    static bool reg = false;
    if (!reg) { WNDCLASSW wc{}; wc.lpfnWndProc = RDProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"FsuReportDlg"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); RegisterClassW(&wc); reg = true; }
    RD d; d.tabs = tabs; d.btns = btns; d.br = CreateSolidBrush(kWarmBg);
    UINT dpi = owner ? DpiForWindow(owner) : 96; auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    d.font = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    RECT orc{0, 0, 0, 0}; if (owner) GetWindowRect(owner, &orc);
    int W = S(780), H = S(640), x = owner ? orc.left + ((orc.right - orc.left) - W) / 2 : CW_USEDEFAULT, y = owner ? orc.top + ((orc.bottom - orc.top) - H) / 2 : CW_USEDEFAULT;
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FsuReportDlg", title.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, x, y, W, H, owner, nullptr, GetModuleHandleW(nullptr), &d);
    SetDarkTitleBar(h, true);
    if (tabs.size() > 1) { d.tab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_FOCUSNEVER, 0, 0, 10, 10, h, nullptr, nullptr, nullptr);
        for (size_t i = 0; i < tabs.size(); ++i) { TCITEMW it{}; it.mask = TCIF_TEXT; it.pszText = (LPWSTR)tabs[i].name.c_str(); SendMessageW(d.tab, TCM_INSERTITEMW, i, (LPARAM)&it); } SendMessageW(d.tab, WM_SETFONT, (WPARAM)d.font, TRUE); }
    d.edit = CreateWindowExW(0, L"EDIT", tabs.empty() ? L"" : tabs[0].text.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 10, 10, h, nullptr, nullptr, nullptr);
    SendMessageW(d.edit, WM_SETFONT, (WPARAM)d.font, TRUE);
    for (size_t i = 0; i < btns.size(); ++i) { HWND b = CreateWindowExW(0, L"BUTTON", btns[i].label.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | (i + 1 == btns.size() ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), 0, 0, 10, 10, h, (HMENU)(INT_PTR)(2000 + i), nullptr, nullptr);
        SendMessageW(b, WM_SETFONT, (WPARAM)d.font, TRUE); d.bs.push_back(b); }
    RECT cr; GetClientRect(h, &cr); SendMessageW(h, WM_SIZE, 0, MAKELPARAM(cr.right, cr.bottom));
    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(h, SW_SHOW); if (!d.bs.empty()) SetFocus(d.bs.back());
    MSG msg;
    while (!d.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { d.done = true; break; }
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    DestroyWindow(h); DeleteObject(d.font); DeleteObject(d.br);
}

// ---------- message box (replaces MessageBoxW so every dialog matches the app) ----------
struct MD { HBRUSH br; bool done = false; int result = IDNO; int kind; };
static LRESULT CALLBACK MDProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    MD* d = (MD*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); break;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: SetTextColor((HDC)w, kWarmTx); SetBkColor((HDC)w, kWarmBg); return (LRESULT)d->br;
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, d->br); return 1; }
    case WM_COMMAND: { int id = LOWORD(w); if (id == IDOK || id == IDYES || id == IDNO || id == IDCANCEL) { d->result = id == IDCANCEL ? (d->kind == 2 ? IDNO : IDOK) : id; d->done = true; return 0; } break; }
    case WM_CLOSE: d->result = d->kind == 2 ? IDNO : IDOK; d->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
int DarkMsg(HWND owner, const wstring& title, const wstring& text, int kind) {
    static bool reg = false;
    if (!reg) { WNDCLASSW wc{}; wc.lpfnWndProc = MDProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"FsuMsgDlg"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); RegisterClassW(&wc); reg = true; }
    MD d; d.br = CreateSolidBrush(kWarmBg); d.kind = kind;
    UINT dpi = owner ? DpiForWindow(owner) : 96; auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    HFONT f = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    int tw = S(460); HDC dc = GetDC(nullptr); HFONT of = (HFONT)SelectObject(dc, f); RECT cr{0, 0, tw, 0}; DrawTextW(dc, text.c_str(), -1, &cr, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX); SelectObject(dc, of); ReleaseDC(nullptr, dc);
    int th = std::min<int>(cr.bottom, S(420)); int W = tw + S(48), H = th + S(120);
    RECT orc{}; if (owner) GetWindowRect(owner, &orc); else { orc = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)}; }
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FsuMsgDlg", title.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU, orc.left + ((orc.right - orc.left) - W) / 2, orc.top + ((orc.bottom - orc.top) - H) / 2, W, H, owner, nullptr, GetModuleHandleW(nullptr), &d);
    SetDarkTitleBar(h, true);
    HWND st = CreateWindowExW(0, L"STATIC", text.c_str(), WS_CHILD | WS_VISIBLE | SS_NOPREFIX, S(20), S(18), tw, th + S(4), h, nullptr, nullptr, nullptr); SendMessageW(st, WM_SETFONT, (WPARAM)f, TRUE);
    auto btn = [&](const wchar_t* t, int id, int x, bool def) { HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (def ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON), x, th + S(46), S(90), S(28), h, (HMENU)(INT_PTR)id, nullptr, nullptr); SendMessageW(b, WM_SETFONT, (WPARAM)f, TRUE); return b; };
    HWND focus;
    if (kind == 2) { btn(L"Yes", IDYES, W - S(20) - S(190) - S(8), false); focus = btn(L"No", IDNO, W - S(20) - S(100) - S(8) + S(0), true); } else focus = btn(L"OK", IDOK, W - S(20) - S(100) - S(8), true);
    if (owner) EnableWindow(owner, FALSE); ShowWindow(h, SW_SHOW); SetFocus(focus);
    MSG msg; while (!d.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { d.result = kind == 2 ? IDNO : IDOK; break; }
        if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    DestroyWindow(h); DeleteObject(f); DeleteObject(d.br); return d.result;
}

// ---------- generic form (edit / combo / checkbox) ----------
struct FD { std::vector<FormField>* fields; std::vector<HWND> ctl; HBRUSH br; bool ok = false, done = false; };
static LRESULT CALLBACK FDProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    FD* d = (FD*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); break;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: SetTextColor((HDC)w, kWarmTx); SetBkColor((HDC)w, kWarmBg); return (LRESULT)d->br;
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, d->br); return 1; }
    case WM_COMMAND:
        if (LOWORD(w) == IDOK) { for (size_t i = 0; i < d->ctl.size(); ++i) { auto& f = (*d->fields)[i];
                if (f.type == 1) f.value = std::to_wstring((int)SendMessageW(d->ctl[i], CB_GETCURSEL, 0, 0)); else if (f.type == 2) f.value = SendMessageW(d->ctl[i], BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0";
                else { wchar_t b[4096]; GetWindowTextW(d->ctl[i], b, 4096); f.value = b; } }
            d->ok = d->done = true; return 0; }
        if (LOWORD(w) == IDCANCEL) { d->done = true; return 0; } break;
    case WM_CLOSE: d->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
bool FormDialog(HWND owner, const wstring& title, const wstring& okText, std::vector<FormField>& fields, const wstring& note) {
    static bool reg = false;
    if (!reg) { WNDCLASSW wc{}; wc.lpfnWndProc = FDProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"FsuFormDlg"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); RegisterClassW(&wc); reg = true; }
    FD d; d.fields = &fields; d.br = CreateSolidBrush(kWarmBg);
    UINT dpi = owner ? DpiForWindow(owner) : 96; auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    HFONT f = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    int W = S(584), y = S(14), nlines = note.empty() ? 0 : (int)(note.size() / 80) + 2;
    RECT orc{}; if (owner) GetWindowRect(owner, &orc); else orc = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FsuFormDlg", title.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU, 0, 0, W, S(200), owner, nullptr, GetModuleHandleW(nullptr), &d);
    SetDarkTitleBar(h, true);
    auto mk = [&](const wchar_t* cls, const wstring& t, DWORD st, int x, int yy, int w, int hh, DWORD ex = 0) { HWND c = CreateWindowExW(ex, cls, t.c_str(), WS_CHILD | WS_VISIBLE | st, x, yy, w, hh, h, nullptr, nullptr, nullptr); SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE); return c; };
    if (!note.empty()) { mk(L"STATIC", note, SS_NOPREFIX, S(18), y, W - S(52), S(16) * nlines); y += S(16) * nlines + S(8); }
    for (auto& fl : fields) {
        HWND c;
        if (fl.type == 2) { c = mk(L"BUTTON", fl.label, BS_AUTOCHECKBOX | WS_TABSTOP, S(18), y, W - S(52), S(22)); SendMessageW(c, BM_SETCHECK, fl.value == L"1" ? BST_CHECKED : BST_UNCHECKED, 0); y += S(30); }
        else { mk(L"STATIC", fl.label, SS_NOPREFIX, S(18), y, W - S(52), S(18)); y += S(20);
            if (fl.type == 1) { c = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, S(18), y, W - S(52), S(300)); for (auto& o : fl.options) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)o.c_str()); SendMessageW(c, CB_SETCURSEL, _wtoi(fl.value.c_str()), 0); }
            else c = mk(L"EDIT", fl.value, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, S(18), y, W - S(52), S(24));
            y += S(34); }
        d.ctl.push_back(c);
    }
    HWND ok = mk(L"BUTTON", okText, BS_DEFPUSHBUTTON | WS_TABSTOP, W - S(18) - S(190) - S(8), y + S(6), S(90), S(28)); SetWindowLongPtrW(ok, GWLP_ID, IDOK);
    HWND cn = mk(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, W - S(18) - S(100) - S(8), y + S(6), S(90), S(28)); SetWindowLongPtrW(cn, GWLP_ID, IDCANCEL);
    RECT wr{0, 0, W, y + S(60)}; AdjustWindowRectEx(&wr, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int ww = wr.right - wr.left, hh = wr.bottom - wr.top; SetWindowPos(h, nullptr, orc.left + ((orc.right - orc.left) - ww) / 2, orc.top + ((orc.bottom - orc.top) - hh) / 2, ww, hh, SWP_NOZORDER);
    if (owner) EnableWindow(owner, FALSE); ShowWindow(h, SW_SHOW); if (!d.ctl.empty()) SetFocus(d.ctl[0]);
    MSG msg; while (!d.done && GetMessageW(&msg, nullptr, 0, 0)) { if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) break; if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    DestroyWindow(h); DeleteObject(f); DeleteObject(d.br); return d.ok;
}

} // namespace fsu
