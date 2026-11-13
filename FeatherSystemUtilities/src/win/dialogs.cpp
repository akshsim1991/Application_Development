#include "app.h"
#include "../core/text.h"
#include <commctrl.h>
#include <dwmapi.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <fstream>
#include <filesystem>

namespace fsu {
using std::wstring;

void SetDarkTitleBar(HWND h, bool dark) { BOOL v = dark; DwmSetWindowAttribute(h, 20, &v, sizeof v); }

// ---------- shell actions ----------
void OpenFileLocation(const wstring& program) {
    wstring p = ExpandEnv(program);
    if (p.empty() || GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) { MessageBoxW(nullptr, L"The file was not found, so its location cannot be opened.", L"Open file location", MB_OK | MB_ICONINFORMATION); return; }
    wstring args = L"/select,\"" + p + L"\"";                              // arguments array-style: quoted path, no shell interpretation of the target
    ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}
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
        if (h.empty()) { MessageBoxW(owner, L"The file cannot be read, so its fingerprint cannot be computed.", L"Check on VirusTotal", MB_OK | MB_ICONINFORMATION); return; }
        url = L"https://www.virustotal.com/gui/file/" + h; what = L"This computes the file's SHA-256 here and opens VirusTotal in your web browser with that hash. The file itself is not uploaded.";
    } else { url = L"https://www.bing.com/search?q=" + UrlEncode(name.empty() ? program : name); what = L"This opens your web browser and sends the name \"" + name + L"\" to Bing."; }
    wstring msg = what + L"\r\n\r\nFeather System Utilities itself stays offline. Continue?";
    if (MessageBoxW(owner, msg.c_str(), vt ? L"Check on VirusTotal" : L"Search online", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

wstring StartupDetailsText(const StartupEntry& e) {
    wstring t = e.name + L"\r\n\r\nSTATUS\r\n" + e.state + L"\r\nStarts: " + e.starts + L"\r\n\r\nWHERE IT IS SET\r\nType: " + e.type + L"\r\nFor: " + e.scope + L"\r\n";
    if (e.type.rfind(L"Registry", 0) == 0) t += L"Registry key: " + e.location + L"\r\nValue name: " + e.valueName + L"\r\n";
    else if (e.type == L"Scheduled task") t += L"Task: " + e.taskPath + L"\r\n";
    else if (e.type == L"Startup folder") t += L"Folder: " + e.location + L"\r\n";
    else t += L"Registry key: " + e.location + L"\r\nCOM class: " + e.clsid + L"\r\n";
    t += L"First seen: " + e.firstSeen + L"\r\n\r\nWHAT IT RUNS\r\nCommand: " + e.command + L"\r\nProgram: " + e.program + L"\r\nPublisher: " + e.publisher + L"\r\nVersion: " + e.version +
         L"\r\nDigital signature: " + e.signature + L"\r\n\r\nWARNINGS\r\n" + (e.warnings.empty() ? wstring(L"None.") : e.warnings) + L"\r\n";
    return t;
}
wstring StartupCopyText(const StartupEntry& e) { return StartupDetailsText(e); }

// ---------- report dialog (warm light theme, as in the original apps) ----------
struct RD { std::vector<ReportTab> tabs; std::vector<ReportBtn> btns; HWND edit = nullptr, tab = nullptr; std::vector<HWND> bs; HFONT font = nullptr; HBRUSH br = nullptr; bool done = false; int closeId = 0; };
static const COLORREF kWarmBg = RGB(0xEA, 0xDB, 0xC8), kWarmTx = RGB(0x2B, 0x22, 0x18);

static LRESULT CALLBACK RDProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    RD* d = (RD*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); break;
    case WM_SIZE: if (d && d->edit) {
        int W = LOWORD(l), H = HIWORD(l), pad = 12, bh = 28, top = pad;
        if (d->tab) { MoveWindow(d->tab, pad, pad, W - 2 * pad, 26, TRUE); top = pad + 30; }
        MoveWindow(d->edit, pad, top, W - 2 * pad, H - top - bh - 2 * pad, TRUE);
        int x = pad; for (size_t i = 0; i < d->bs.size(); ++i) { bool last = i + 1 == d->bs.size(); int bw = last ? 80 : 124;
            if (last) { MoveWindow(d->bs[i], W - pad - bw, H - bh - pad, bw, bh, TRUE); } else { MoveWindow(d->bs[i], x, H - bh - pad, bw, bh, TRUE); x += bw + 6; } }
        return 0; } break;
    case WM_NOTIFY: if (d && ((NMHDR*)l)->hwndFrom == d->tab && ((NMHDR*)l)->code == TCN_SELCHANGE) { int s = TabCtrl_GetCurSel(d->tab); if (s >= 0 && s < (int)d->tabs.size()) SetWindowTextW(d->edit, d->tabs[s].text.c_str()); } return 0;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: { HDC dc = (HDC)w; SetTextColor(dc, kWarmTx); SetBkColor(dc, kWarmBg); return (LRESULT)d->br; }
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, d->br); return 1; }
    case WM_COMMAND: if (d) { int id = LOWORD(w);
        if (id == IDCANCEL) { d->done = true; return 0; }
        if (id >= 2000 && id < 2000 + (int)d->btns.size()) { wstring cur; wchar_t b[65536]; GetWindowTextW(d->edit, b, 65536); cur = b; auto fn = d->btns[id - 2000].fn; if (fn) fn(cur); else d->done = true; } return 0; } break;
    case WM_CLOSE: if (d) d->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

void ShowReport(HWND owner, const wstring& title, const std::vector<ReportTab>& tabs, const std::vector<ReportBtn>& btns) {
    static bool reg = false;
    if (!reg) { WNDCLASSW wc{}; wc.lpfnWndProc = RDProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"FsuReportDlg"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); RegisterClassW(&wc); reg = true; }
    RD d; d.tabs = tabs; d.btns = btns; d.br = CreateSolidBrush(kWarmBg);
    UINT dpi = owner ? GetDpiForWindow(owner) : 96; auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    d.font = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    RECT orc{0, 0, 0, 0}; if (owner) GetWindowRect(owner, &orc);
    int W = S(740), H = S(600), x = owner ? orc.left + ((orc.right - orc.left) - W) / 2 : CW_USEDEFAULT, y = owner ? orc.top + ((orc.bottom - orc.top) - H) / 2 : CW_USEDEFAULT;
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

// ---------- two-field input ----------
struct AD { wstring l1, l2, v1, v2; HWND e1, e2; bool ok = false, done = false; HBRUSH br; };
static LRESULT CALLBACK ADProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    AD* d = (AD*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_NCCREATE: SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); break;
    case WM_CTLCOLORSTATIC: { SetTextColor((HDC)w, kWarmTx); SetBkColor((HDC)w, kWarmBg); return (LRESULT)d->br; }
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, d->br); return 1; }
    case WM_COMMAND: if (LOWORD(w) == IDOK) { wchar_t a[2048], b[2048]; GetWindowTextW(d->e1, a, 2048); GetWindowTextW(d->e2, b, 2048); d->v1 = a; d->v2 = b; d->ok = d->done = true; return 0; }
        if (LOWORD(w) == IDCANCEL) { d->done = true; return 0; } break;
    case WM_CLOSE: d->done = true; return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
bool AskTwo(HWND owner, const wstring& title, const wstring& l1, const wstring& l2, wstring& v1, wstring& v2) {
    static bool reg = false;
    if (!reg) { WNDCLASSW wc{}; wc.lpfnWndProc = ADProc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"FsuAskDlg"; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); RegisterClassW(&wc); reg = true; }
    AD d; d.br = CreateSolidBrush(kWarmBg);
    UINT dpi = owner ? GetDpiForWindow(owner) : 96; auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    HFONT f = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    RECT orc{}; if (owner) GetWindowRect(owner, &orc); int W = S(560), H = S(210);
    HWND h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"FsuAskDlg", title.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU, orc.left + ((orc.right - orc.left) - W) / 2, orc.top + ((orc.bottom - orc.top) - H) / 2, W, H, owner, nullptr, GetModuleHandleW(nullptr), &d);
    SetDarkTitleBar(h, true);
    auto mk = [&](const wchar_t* cls, const wstring& t, DWORD st, int x, int y, int w, int hh, int id) { HWND c = CreateWindowExW(0, cls, t.c_str(), WS_CHILD | WS_VISIBLE | st, S(x), S(y), S(w), S(hh), h, (HMENU)(INT_PTR)id, nullptr, nullptr); SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE); return c; };
    mk(L"STATIC", l1, 0, 14, 14, 520, 18, 0); d.e1 = mk(L"EDIT", v1, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 14, 34, 520, 24, 0);
    mk(L"STATIC", l2, 0, 14, 68, 520, 18, 0); d.e2 = mk(L"EDIT", v2, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 14, 88, 520, 24, 0);
    mk(L"BUTTON", L"Add", BS_DEFPUSHBUTTON | WS_TABSTOP, 360, 130, 84, 28, IDOK); mk(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, 450, 130, 84, 28, IDCANCEL);
    if (owner) EnableWindow(owner, FALSE); ShowWindow(h, SW_SHOW); SetFocus(d.e1);
    MSG msg; while (!d.done && GetMessageW(&msg, nullptr, 0, 0)) { if (!IsDialogMessageW(h, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    DestroyWindow(h); DeleteObject(f); DeleteObject(d.br);
    if (d.ok) { v1 = d.v1; v2 = d.v2; } return d.ok;
}

} // namespace fsu
