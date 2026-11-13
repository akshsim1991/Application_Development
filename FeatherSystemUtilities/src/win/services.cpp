#include "app.h"
#include "../core/text.h"
#include <set>
#include <algorithm>

#ifndef SERVICE_USER_SERVICE
#define SERVICE_USER_SERVICE 0x00000040
#endif
#ifndef SERVICE_USERSERVICE_INSTANCE
#define SERVICE_USERSERVICE_INSTANCE 0x00000080
#endif

namespace fsu {
using std::wstring;

bool IsCriticalService(const wstring& n) {
    static const std::set<wstring> c = {L"rpcss", L"rpceptmapper", L"dcomlaunch", L"lsm", L"samss", L"keyiso", L"cryptsvc", L"eventlog", L"eventsystem",
        L"brokerinfrastructure", L"bfe", L"mpssvc", L"dhcp", L"dnscache", L"gpsvc", L"profsvc", L"schedule", L"systemeventsbroker", L"winmgmt", L"plugplay",
        L"power", L"staterepository", L"usermanager", L"nsi", L"coremessagingregistrar", L"lanmanworkstation", L"lanmanserver", L"trustedinstaller",
        L"windefend", L"wscsvc", L"sens", L"netprofm", L"nlasvc", L"termservice", L"audiosrv", L"audioendpointbuilder", L"themes", L"timebrokersvc"};
    wstring l = Lower(n); size_t u = l.find(L'_'); if (u != wstring::npos) l = l.substr(0, u);
    return c.count(l) > 0;
}

static const wchar_t* StateName(DWORD s) {
    switch (s) { case SERVICE_STOPPED: return L"Stopped"; case SERVICE_START_PENDING: return L"Starting"; case SERVICE_STOP_PENDING: return L"Stopping";
        case SERVICE_RUNNING: return L"Running"; case SERVICE_CONTINUE_PENDING: return L"Resuming"; case SERVICE_PAUSE_PENDING: return L"Pausing"; case SERVICE_PAUSED: return L"Paused"; }
    return L"Unknown";
}
static wstring StartLabel(DWORD t, bool delayed, bool trig) {
    wstring s;
    switch (t) { case SERVICE_AUTO_START: s = L"Automatic"; break; case SERVICE_DEMAND_START: s = L"Manual"; break; case SERVICE_DISABLED: s = L"Disabled"; break;
        case SERVICE_BOOT_START: s = L"Boot"; break; case SERVICE_SYSTEM_START: s = L"System"; break; default: s = L"?"; }
    if (t == SERVICE_AUTO_START && delayed) s += L" (delayed)";
    if (trig) s += L", trigger";
    return s;
}
static wstring QueryDescription(SC_HANDLE sv) {
    DWORD n = 0; QueryServiceConfig2W(sv, SERVICE_CONFIG_DESCRIPTION, nullptr, 0, &n);
    if (!n) return L"";
    std::vector<BYTE> b(n);
    if (!QueryServiceConfig2W(sv, SERVICE_CONFIG_DESCRIPTION, b.data(), n, &n)) return L"";
    auto* d = (SERVICE_DESCRIPTIONW*)b.data();
    return d->lpDescription ? d->lpDescription : L"";
}


ServiceScan ScanServices() {
    ServiceScan out;
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT);
    if (!scm) { out.coverage = L"Cannot open the Service Control Manager: " + Win32Err(GetLastError()); return out; }
    const DWORD types = SERVICE_WIN32 | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE;
    DWORD need = 0, cnt = 0, resume = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, types, SERVICE_STATE_ALL, nullptr, 0, &need, &cnt, &resume, nullptr);
    std::vector<BYTE> buf(need + 8192); resume = 0;
    if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, types, SERVICE_STATE_ALL, buf.data(), (DWORD)buf.size(), &need, &cnt, &resume, nullptr) && GetLastError() != ERROR_MORE_DATA) {
        out.coverage = L"Enumeration failed: " + Win32Err(GetLastError()); CloseServiceHandle(scm); return out;
    }
    auto* es = (ENUM_SERVICE_STATUS_PROCESSW*)buf.data();
    int noCfg = 0, running = 0;
    for (DWORD i = 0; i < cnt; ++i) {
        const auto& s = es[i];
        ServiceEntry e; e.name = s.lpServiceName; e.display = s.lpDisplayName; e.stateCode = s.ServiceStatusProcess.dwCurrentState;
        e.state = StateName(e.stateCode); e.pid = s.ServiceStatusProcess.dwProcessId; e.controls = s.ServiceStatusProcess.dwControlsAccepted; e.svcType = s.ServiceStatusProcess.dwServiceType;
        e.critical = IsCriticalService(e.name);
        if (e.stateCode == SERVICE_RUNNING) ++running;
        if (SC_HANDLE sv = OpenServiceW(scm, e.name.c_str(), SERVICE_QUERY_CONFIG)) {
            DWORD n = 0; QueryServiceConfigW(sv, nullptr, 0, &n);
            std::vector<BYTE> cb(n ? n : 1);
            if (QueryServiceConfigW(sv, (LPQUERY_SERVICE_CONFIGW)cb.data(), n, &n)) {
                auto* c = (LPQUERY_SERVICE_CONFIGW)cb.data();
                e.startType = c->dwStartType; e.account = c->lpServiceStartName ? c->lpServiceStartName : L""; e.path = c->lpBinaryPathName ? c->lpBinaryPathName : L"";
                if (c->lpDependencies) for (const wchar_t* p = c->lpDependencies; *p; p += wcslen(p) + 1) e.deps.push_back(p);
                if (c->dwStartType == SERVICE_AUTO_START) { SERVICE_DELAYED_AUTO_START_INFO di{}; DWORD m = 0;
                    if (QueryServiceConfig2W(sv, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, (LPBYTE)&di, sizeof di, &m) && di.fDelayedAutostart) e.delayed = true; }
                DWORD tn = 0; QueryServiceConfig2W(sv, 8 /*SERVICE_CONFIG_TRIGGER_INFO*/, nullptr, 0, &tn);
                if (tn >= sizeof(DWORD)) { std::vector<BYTE> tb(tn);
                    if (QueryServiceConfig2W(sv, 8, tb.data(), tn, &tn) && *(DWORD*)tb.data() > 0) e.trigger = true; }
            } else ++noCfg;
            e.description = QueryDescription(sv);
            CloseServiceHandle(sv);
        } else ++noCfg;
        e.start = StartLabel(e.startType, e.delayed, e.trigger);
        if (e.svcType & SERVICE_USERSERVICE_INSTANCE) { e.ptype = L"Per-user instance"; e.notes = L"Per-user service instance"; }
        else if (e.svcType & SERVICE_USER_SERVICE) e.ptype = L"Per-user template";
        else if (e.svcType & SERVICE_WIN32_SHARE_PROCESS) { e.ptype = L"Shared process"; e.notes = L"Process figures cover every service in the host"; }
        else if (e.svcType & SERVICE_WIN32_OWN_PROCESS) e.ptype = L"Own process";
        // evidence about the executable (never loaded or run)
        auto pc = ParseCommand(ExpandEnv(e.path), FileExistsExpanded);
        e.program = pc.exe;
        bool firstTokenIsExe = true;
        if (!e.path.empty() && e.path[0] != L'"') { size_t sp = e.path.find(L' '); wstring tok = sp == wstring::npos ? e.path : e.path.substr(0, sp); firstTokenIsExe = Lower(tok) == Lower(pc.exe); }
        bool microsoftPath = false;
        FileInfo fi = pc.exe.empty() ? FileInfo() : InspectFile(pc.exe, false);
        e.publisher = fi.publisher; e.version = fi.version;
        microsoftPath = Lower(fi.publisher).find(L"microsoft") != wstring::npos;
        wstring warn;
        if (!pc.exe.empty() && !fi.exists && !FileExistsExpanded(pc.exe)) warn = L"File missing";
        else if (!microsoftPath && fi.exists) {
            FileInfo sg = InspectFile(pc.exe, true);
            e.signature = sg.sig == Sig::Valid ? L"Valid" + (sg.signer.empty() ? wstring() : L", signed by " + sg.signer) : sg.sig == Sig::Unsigned ? L"Not signed" : sg.sig == Sig::Invalid ? L"Not trusted (offline check)" : L"Could not check";
            if (sg.sig == Sig::Unsigned) warn = L"Unsigned"; else if (sg.sig == Sig::Invalid) warn = L"Signature not trusted";
        } else if (microsoftPath) e.signature = L"Not checked (Microsoft file version info)";
        if (!firstTokenIsExe && pc.exe.find(L' ') != wstring::npos) warn += (warn.empty() ? L"" : L" · ") + wstring(L"Unquoted path");
        e.warnings = warn;
        e.safety = e.critical ? L"Critical" : (microsoftPath ? L"Windows" : L"Third-party");
        out.entries.push_back(std::move(e));
    }
    CloseServiceHandle(scm);
    out.coverage = L"Enumerated " + std::to_wstring(cnt) + L" Win32 services (" + std::to_wstring(running) + L" running); configuration unreadable for " + std::to_wstring(noCfg) +
                   L". Drivers are not listed. Visibility is limited to what your account may query. 'Critical' comes from a built-in list and is not exhaustive.";
    return out;
}

// ---- control ----
static bool WaitState(SC_HANDLE sv, DWORD want, DWORD maxMs, wstring& err) {
    DWORD start = GetTickCount(), lastCheck = 0;
    for (;;) {
        SERVICE_STATUS_PROCESS ss; DWORD n = 0;
        if (!QueryServiceStatusEx(sv, SC_STATUS_PROCESS_INFO, (LPBYTE)&ss, sizeof ss, &n)) { err = Win32Err(GetLastError()); return false; }
        if (ss.dwCurrentState == want) return true;
        if (ss.dwCheckPoint != lastCheck) { lastCheck = ss.dwCheckPoint; start = GetTickCount(); }   // progress resets the wait
        if (GetTickCount() - start > maxMs) { err = L"Timed out waiting for the service (still " + wstring(StateName(ss.dwCurrentState)) + L"). The request was already accepted by Windows and was not undone."; return false; }
        Sleep(ss.dwWaitHint ? std::min<DWORD>(ss.dwWaitHint / 10, 500) + 50 : 250);
    }
}
std::vector<wstring> ActiveDependents(const wstring& name) {
    std::vector<wstring> r;
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) return r;
    if (SC_HANDLE sv = OpenServiceW(scm, name.c_str(), SERVICE_ENUMERATE_DEPENDENTS)) {
        DWORD need = 0, cnt = 0; EnumDependentServicesW(sv, SERVICE_ACTIVE, nullptr, 0, &need, &cnt);
        if (need) { std::vector<BYTE> b(need); if (EnumDependentServicesW(sv, SERVICE_ACTIVE, (LPENUM_SERVICE_STATUSW)b.data(), need, &need, &cnt))
            for (DWORD i = 0; i < cnt; ++i) r.push_back(((LPENUM_SERVICE_STATUSW)b.data())[i].lpServiceName); }
        CloseServiceHandle(sv);
    }
    CloseServiceHandle(scm); return r;
}
static bool StopOne(SC_HANDLE scm, const wstring& name, wstring& err) {
    SC_HANDLE sv = OpenServiceW(scm, name.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!sv) { err = Win32Err(GetLastError()); return false; }
    SERVICE_STATUS st; bool ok = true;
    if (!ControlService(sv, SERVICE_CONTROL_STOP, &st)) { DWORD e = GetLastError(); if (e != ERROR_SERVICE_NOT_ACTIVE) { err = Win32Err(e); ok = false; } }
    if (ok) ok = WaitState(sv, SERVICE_STOPPED, 30000, err);
    CloseServiceHandle(sv); return ok;
}
static bool StartOne(SC_HANDLE scm, const wstring& name, wstring& err) {
    SC_HANDLE sv = OpenServiceW(scm, name.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
    if (!sv) { err = Win32Err(GetLastError()); return false; }
    bool ok = true;
    if (!StartServiceW(sv, 0, nullptr)) { DWORD e = GetLastError(); if (e != ERROR_SERVICE_ALREADY_RUNNING) { err = e == ERROR_SERVICE_DISABLED ? L"The service is disabled. Change its start type first." : Win32Err(e); ok = false; } }
    if (ok) ok = WaitState(sv, SERVICE_RUNNING, 30000, err);
    CloseServiceHandle(sv); return ok;
}
bool ServiceOp(const ServiceEntry& s, SvcOp op, wstring& err) {
    static const wchar_t* names[] = {L"Start", L"Stop", L"Restart", L"Pause", L"Resume"};
    wstring opName = names[(int)op];
    if ((op == SvcOp::Stop || op == SvcOp::Restart) && s.critical) { err = L"Blocked: this service is on the critical list (Windows may become unstable)."; WriteJournal(s.name, L"SCM", s.state, s.state, L"blocked: critical", false); return false; }
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) { err = Win32Err(GetLastError()); return false; }
    bool ok = false; wstring after;
    switch (op) {
        case SvcOp::Start: ok = StartOne(scm, s.name, err); break;
        case SvcOp::Stop: case SvcOp::Restart: {
            auto deps = ActiveDependents(s.name);
            for (auto& d : deps) if (IsCriticalService(d)) { err = L"Blocked: dependent service '" + d + L"' is critical."; CloseServiceHandle(scm); return false; }
            bool good = true;
            for (auto& d : deps) if (!StopOne(scm, d, err)) { good = false; err = L"Could not stop dependent '" + d + L"': " + err; break; }
            if (good) good = StopOne(scm, s.name, err);
            if (good && op == SvcOp::Restart) { good = StartOne(scm, s.name, err);
                for (auto it = deps.rbegin(); good && it != deps.rend(); ++it) { wstring e2; StartOne(scm, *it, e2); } }   // put dependents back (best effort)
            ok = good; break; }
        case SvcOp::Pause: case SvcOp::Resume: {
            SC_HANDLE sv = OpenServiceW(scm, s.name.c_str(), SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS);
            if (!sv) { err = Win32Err(GetLastError()); break; }
            SERVICE_STATUS st; ok = ControlService(sv, op == SvcOp::Pause ? SERVICE_CONTROL_PAUSE : SERVICE_CONTROL_CONTINUE, &st) != 0;
            if (!ok) err = Win32Err(GetLastError()); else ok = WaitState(sv, op == SvcOp::Pause ? SERVICE_PAUSED : SERVICE_RUNNING, 30000, err);
            CloseServiceHandle(sv); break; }
    }
    CloseServiceHandle(scm);
    WriteJournal(s.name, L"SCM", s.state, ok ? (op == SvcOp::Stop ? L"Stopped" : op == SvcOp::Pause ? L"Paused" : L"Running") : L"unchanged/unknown", ok ? opName + L": ok" : opName + L": failed: " + err, false);
    return ok;
}

bool SetServiceStartType(const wstring& name, DWORD type, bool delayed, wstring& err, wstring& before) {
    if (type == SERVICE_DISABLED && IsCriticalService(name)) { err = L"Blocked: disabling a critical service is not allowed here."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) { err = Win32Err(GetLastError()); return false; }
    SC_HANDLE sv = OpenServiceW(scm, name.c_str(), SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG);
    if (!sv) { DWORD e = GetLastError(); err = e == ERROR_ACCESS_DENIED ? L"Access denied. Administrator rights are required (More > Restart as administrator)." : Win32Err(e); CloseServiceHandle(scm); return false; }
    auto read = [&](DWORD& t, bool& d) {
        DWORD n = 0; QueryServiceConfigW(sv, nullptr, 0, &n); std::vector<BYTE> b(n ? n : 1);
        if (!QueryServiceConfigW(sv, (LPQUERY_SERVICE_CONFIGW)b.data(), n, &n)) return false;
        t = ((LPQUERY_SERVICE_CONFIGW)b.data())->dwStartType; d = false;
        if (t == SERVICE_AUTO_START) { SERVICE_DELAYED_AUTO_START_INFO di{}; DWORD m = 0; if (QueryServiceConfig2W(sv, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, (LPBYTE)&di, sizeof di, &m)) d = di.fDelayedAutostart != 0; }
        return true; };
    DWORD oldT = 0; bool oldD = false; bool ok = read(oldT, oldD);
    before = StartLabel(oldT, oldD, false);
    if (ok) ok = ChangeServiceConfigW(sv, SERVICE_NO_CHANGE, type, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) != 0;
    if (!ok) err = Win32Err(GetLastError());
    if (ok && type == SERVICE_AUTO_START) { SERVICE_DELAYED_AUTO_START_INFO di{}; di.fDelayedAutostart = delayed; if (!ChangeServiceConfig2W(sv, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &di)) { ok = false; err = Win32Err(GetLastError()); } }
    DWORD nt = 0; bool nd = false;
    if (ok) { ok = read(nt, nd) && nt == type && (type != SERVICE_AUTO_START || nd == delayed); if (!ok && err.empty()) err = L"Read-back did not match."; }
    CloseServiceHandle(sv); CloseServiceHandle(scm);
    WriteJournal(name, L"SCM start type", before, ok ? StartLabel(type, delayed, false) : L"unchanged/unknown", ok ? L"ok" : L"failed: " + err, true);
    return ok;
}

// ---- Properties text ----
static wstring RecoveryText(const wstring& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) return L"Cannot read: " + Win32Err(GetLastError());
    SC_HANDLE sv = OpenServiceW(scm, name.c_str(), SERVICE_QUERY_CONFIG); wstring r;
    if (!sv) r = L"Cannot read: " + Win32Err(GetLastError());
    else {
        DWORD n = 0; QueryServiceConfig2W(sv, SERVICE_CONFIG_FAILURE_ACTIONS, nullptr, 0, &n); std::vector<BYTE> b(n ? n : 1);
        if (QueryServiceConfig2W(sv, SERVICE_CONFIG_FAILURE_ACTIONS, b.data(), n, &n)) {
            auto* fa = (SERVICE_FAILURE_ACTIONSW*)b.data();
            auto an = [](SC_ACTION_TYPE t) { return t == SC_ACTION_RESTART ? L"Restart the service" : t == SC_ACTION_REBOOT ? L"Restart the computer" : t == SC_ACTION_RUN_COMMAND ? L"Run a program" : L"Take no action"; };
            const wchar_t* lab[] = {L"First failure", L"Second failure", L"Later failures"};
            r = L"What Windows does when the service fails:\r\n\r\n";
            for (DWORD i = 0; i < 3; ++i) { SC_ACTION_TYPE t = (fa->cActions > i && fa->lpsaActions) ? fa->lpsaActions[i].Type : SC_ACTION_NONE;
                DWORD d = (fa->cActions > i && fa->lpsaActions) ? fa->lpsaActions[i].Delay : 0; r += wstring(lab[i]) + L": " + an(t) + (t == SC_ACTION_RESTART ? L" after " + std::to_wstring(d / 60000) + L" min" : L"") + L"\r\n"; }
            r += L"\r\nReset the failure count after: " + std::to_wstring(fa->dwResetPeriod / 86400) + L" days\r\nProgram to run: " + (fa->lpCommand && *fa->lpCommand ? fa->lpCommand : L"(none)") + L"\r\n";
        } else r = L"Recovery settings are not available: " + Win32Err(GetLastError());
        CloseServiceHandle(sv);
    }
    CloseServiceHandle(scm); return r;
}
wstring ServiceTabText(const ServiceEntry& s, int tab) {
    wstring t;
    if (tab == 0) {
        t = s.display + L"\r\n" + (s.description.empty() ? L"(no description)" : s.description) + L"\r\n\r\n";
        t += L"CAN IT BE STOPPED OR DISABLED?\r\n";
        t += s.critical ? L"This service is on the built-in critical list. Stopping or disabling it can break sign-in, networking, security or Windows itself. Feather System Utilities blocks it.\r\n"
                        : L"Not on the critical list. That is not a guarantee it is safe to stop: dependent services and installed software may rely on it. Check the Dependencies tab first.\r\n";
        t += L"\r\nSTATUS\r\nStatus: " + s.state + (s.pid ? L"    Process ID: " + std::to_wstring(s.pid) : wstring()) + L"\r\n\r\nSTART-UP\r\nStart type: " + s.start + L"\r\n\r\n";
        t += L"PROGRAM\r\nCommand line: " + s.path + L"\r\nFile: " + s.program + L"\r\nVersion: " + s.version + L"\r\nCompany: " + s.publisher + L"\r\nDigital signature: " + (s.signature.empty() ? L"(not checked)" : s.signature) +
             L"\r\nLog on as: " + (s.account.empty() ? L"(unknown)" : s.account) + L"\r\nService name: " + s.name + L"    Runs in: " + s.ptype + L"\r\n";
        if (!s.notes.empty()) t += L"Note: " + s.notes + L"\r\n";
        t += L"\r\nWARNINGS\r\n" + (s.warnings.empty() ? wstring(L"None.") : s.warnings) + L"\r\n";
    } else if (tab == 1) {
        t = L"Display name: " + s.display + L"\r\nService name: " + s.name + L"\r\nStart type: " + s.start + L"\r\nLog on as: " + s.account + L"\r\nType: " + s.ptype +
            L"\r\n\r\nChange the start type from the right-click menu (Start type). Editing names, accounts and recovery here is not available in this version.\r\nAccount/name changes would take effect at the next start; custom account passwords are never stored.";
    } else if (tab == 2) t = RecoveryText(s.name);
    else {
        t = L"REQUIRES (this service needs):\r\n"; if (s.deps.empty()) t += L"  (nothing)\r\n"; for (auto& d : s.deps) t += L"  " + d + L"\r\n";
        t += L"\r\nREQUIRED BY (these need this service):\r\n";
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); bool any = false;
        if (scm) { if (SC_HANDLE sv = OpenServiceW(scm, s.name.c_str(), SERVICE_ENUMERATE_DEPENDENTS)) {
            DWORD need = 0, cnt = 0; EnumDependentServicesW(sv, SERVICE_STATE_ALL, nullptr, 0, &need, &cnt);
            if (need) { std::vector<BYTE> b(need); if (EnumDependentServicesW(sv, SERVICE_STATE_ALL, (LPENUM_SERVICE_STATUSW)b.data(), need, &need, &cnt))
                for (DWORD i = 0; i < cnt; ++i) { auto& d = ((LPENUM_SERVICE_STATUSW)b.data())[i]; t += wstring(L"  ") + d.lpServiceName + L" (" + StateName(d.ServiceStatus.dwCurrentState) + L")\r\n"; any = true; } }
            CloseServiceHandle(sv); } CloseServiceHandle(scm); }
        if (!any) t += L"  (nothing)\r\n";
    }
    return t;
}

} // namespace fsu
