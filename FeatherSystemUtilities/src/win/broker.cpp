// Administrator helper ("privilege broker"). The normal window runs without elevation; when the user agrees, ONE elevated copy of this exe
// (started with --broker) is launched and answers a small whitelist of explicit commands over a named pipe that only the same user can open.
// Safety: pipe DACL = current user + SYSTEM only, remote clients rejected, client PID must be the launching process, a random token must match,
// only whitelisted commands are executed (they are the same validated handlers the window uses), and the helper exits when the window closes.
#include "app.h"
#include "json.h"
#include "../core/text.h"
#include <sddl.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <mutex>
#include <atomic>

namespace fsu {
using std::wstring; using std::string;

static const char* kAllowed[] = {"startup.set", "startup.delete", "startup.delay", "startup.removeDelay", "services.op", "services.startType", "safemode.fix", "recovery.create", "drivers.install", "hotfix.uninstall", "device.fix", "clean.runById", "tasks.set", "services.profileApply"};
bool BrokerAllows(const string& cmd) { for (auto* c : kAllowed) if (cmd == c) return true; return false; }

static bool ReadExact(HANDLE h, void* buf, DWORD n) { char* p = (char*)buf; while (n) { DWORD got = 0; if (!ReadFile(h, p, n, &got, nullptr) || !got) return false; p += got; n -= got; } return true; }
static bool WriteExact(HANDLE h, const void* buf, DWORD n) { const char* p = (const char*)buf; while (n) { DWORD put = 0; if (!WriteFile(h, p, n, &put, nullptr) || !put) return false; p += put; n -= put; } return true; }
static bool SendMsg(HANDLE h, const string& s) { DWORD n = (DWORD)s.size(); return WriteExact(h, &n, 4) && WriteExact(h, s.data(), n); }
static bool RecvMsg(HANDLE h, string& s) { DWORD n = 0; if (!ReadExact(h, &n, 4) || n > 4u * 1024 * 1024) return false; s.resize(n); return n == 0 || ReadExact(h, &s[0], n); }
static wstring UserSid() { HANDLE t; wstring out; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return out; DWORD n = 0; GetTokenInformation(t, TokenUser, nullptr, 0, &n); std::vector<BYTE> b(n); if (n && GetTokenInformation(t, TokenUser, b.data(), n, &n)) { LPWSTR s = nullptr; if (ConvertSidToStringSidW(((TOKEN_USER*)b.data())->User.Sid, &s)) { out = s; LocalFree(s); } } CloseHandle(t); return out; }

// ---------------- elevated side ----------------
static DWORD WINAPI WatchParent(LPVOID p) { WaitForSingleObject((HANDLE)p, INFINITE); ExitProcess(0); return 0; }
int BrokerMain(const wstring& cmdline, const std::function<string(const string&, const string&)>& exec) {
    int argc = 0; LPWSTR* argv = CommandLineToArgvW(cmdline.c_str(), &argc); wstring pipe, token, sid; DWORD parent = 0; if (argv) { for (int i = 0; i + 4 < argc + 0; ++i) if (wcscmp(argv[i], L"--broker") == 0) { pipe = argv[i + 1]; parent = (DWORD)_wtoi(argv[i + 2]); token = argv[i + 3]; sid = argv[i + 4]; } LocalFree(argv); }
    if (pipe.empty() || !parent || token.empty() || sid.empty() || pipe.rfind(L"\\\\.\\pipe\\FeatherBroker-", 0) != 0) return 2;
    if (!IsElevated() || UserSid() != sid) return 3;   // never serve a different account: HKCU and profile paths would not match the window's
    HANDLE ph = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parent); if (!ph) return 4;
    CreateThread(nullptr, 0, WatchParent, ph, 0, nullptr);   // the window closed: leave
    wstring sddl = L"D:(A;;GA;;;" + sid + L")(A;;GA;;;SY)"; SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, FALSE}; if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sa.lpSecurityDescriptor, nullptr)) return 5;
    HANDLE srv = CreateNamedPipeW(pipe.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 65536, 65536, 0, &sa); if (srv == INVALID_HANDLE_VALUE) return 6;
    for (;;) {
        if (!ConnectNamedPipe(srv, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) { DisconnectNamedPipe(srv); continue; }
        ULONG cpid = 0; bool okClient = GetNamedPipeClientProcessId(srv, &cpid) && cpid == parent;
        while (okClient) {
            string req; if (!RecvMsg(srv, req)) break; js::V v; string resp;
            if (!js::Parse(req, v) || v.str("token") != ToUtf8(token)) resp = js::Obj().str("error", "Unauthorized.").done();
            else { string cmd = v.str("cmd"); if (cmd == "quit") { SendMsg(srv, js::Obj().raw("data", "{\"ok\":true}").done()); ExitProcess(0); }
                if (!BrokerAllows(cmd)) resp = js::Obj().str("error", "That command is not allowed through the helper.").done();
                else { string out; try { out = exec(cmd, req); } catch (...) { out = "{\"ok\":false,\"error\":\"The helper hit an unexpected error.\"}"; } resp = js::Obj().raw("data", out).done(); } }
            if (!SendMsg(srv, resp)) break; }
        DisconnectNamedPipe(srv);
    }
}

// ---------------- window side ----------------
static std::mutex g_bm; static HANDLE g_pipe = INVALID_HANDLE_VALUE, g_proc = nullptr; static string g_token;
bool BrokerConnected() { std::lock_guard<std::mutex> l(g_bm); if (g_pipe == INVALID_HANDLE_VALUE) return false; if (g_proc && WaitForSingleObject(g_proc, 0) == WAIT_OBJECT_0) { CloseHandle(g_pipe); g_pipe = INVALID_HANDLE_VALUE; return false; } return true; }
bool BrokerStart(wstring& err) {
    if (BrokerConnected()) return true; if (IsElevated()) { err = L"Feather is already running as administrator."; return false; }
    BYTE rnd[16]; if (BCryptGenRandom(nullptr, rnd, sizeof rnd, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) { err = L"Could not create a secure token."; return false; }
    char hex[40]; for (int i = 0; i < 16; ++i) snprintf(hex + i * 2, 3, "%02x", rnd[i]); string tok = hex; wstring name = L"\\\\.\\pipe\\FeatherBroker-" + FromUtf8(tok.substr(0, 12));
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH); wstring args = L"--broker " + name + L" " + std::to_wstring(GetCurrentProcessId()) + L" " + FromUtf8(tok) + L" " + UserSid();
    SHELLEXECUTEINFOW sei{sizeof sei}; sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NO_CONSOLE; sei.lpVerb = L"runas"; sei.lpFile = exe; sei.lpParameters = args.c_str(); sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei) || !sei.hProcess) { err = L"The administrator prompt was declined or could not be shown."; return false; }
    HANDLE p = INVALID_HANDLE_VALUE; for (int i = 0; i < 120 && p == INVALID_HANDLE_VALUE; ++i) { if (WaitForSingleObject(sei.hProcess, 0) == WAIT_OBJECT_0) { DWORD code = 0; GetExitCodeProcess(sei.hProcess, &code); CloseHandle(sei.hProcess); err = code == 3 ? L"The helper was started by a different administrator account than yours, so it was stopped for safety. Use \"Restart as administrator\" instead." : L"The administrator helper stopped unexpectedly (code " + std::to_wstring(code) + L")."; return false; }
        p = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr); if (p == INVALID_HANDLE_VALUE) Sleep(250); }
    if (p == INVALID_HANDLE_VALUE) { TerminateProcess(sei.hProcess, 1); CloseHandle(sei.hProcess); err = L"The administrator helper did not respond."; return false; }
    DWORD mode = PIPE_READMODE_BYTE; SetNamedPipeHandleState(p, &mode, nullptr, nullptr); std::lock_guard<std::mutex> l(g_bm); g_pipe = p; g_proc = sei.hProcess; g_token = tok; return true;
}
bool BrokerCall(const string& cmd, const string& argsJson, string& result, wstring& err) {
    if (!BrokerAllows(cmd)) { err = L"That action cannot run through the helper."; return false; } std::lock_guard<std::mutex> l(g_bm); if (g_pipe == INVALID_HANDLE_VALUE) { err = L"The administrator helper is not running."; return false; }
    string req = "{\"token\":" + js::Q(g_token) + ",\"cmd\":" + js::Q(cmd) + ",\"args\":" + (argsJson.empty() ? "{}" : argsJson) + "}"; string resp;
    if (!SendMsg(g_pipe, req) || !RecvMsg(g_pipe, resp)) { CloseHandle(g_pipe); g_pipe = INVALID_HANDLE_VALUE; err = L"The administrator helper stopped responding."; return false; }
    js::V v; if (!js::Parse(resp, v)) { err = L"The helper sent an unreadable answer."; return false; } if (auto* e = v.get("error")) { err = FromUtf8(e->s); return false; }
    // the helper answers with the handler's own JSON; re-serialise it by slicing the "data" member
    size_t p = resp.find("\"data\":"); result = p == string::npos ? "{\"ok\":true}" : resp.substr(p + 7, resp.size() - p - 8); return true;
}
void BrokerStop() { std::lock_guard<std::mutex> l(g_bm); if (g_pipe != INVALID_HANDLE_VALUE) { string req = "{\"token\":" + js::Q(g_token) + ",\"cmd\":\"quit\"}"; SendMsg(g_pipe, req); CloseHandle(g_pipe); g_pipe = INVALID_HANDLE_VALUE; } if (g_proc) { CloseHandle(g_proc); g_proc = nullptr; } }
}
