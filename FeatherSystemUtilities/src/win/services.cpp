#include "app.h"
#include "../core/text.h"
#include <set>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <winevt.h>

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


ServiceScan ScanServices(bool drivers) {
    ServiceScan out;
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE | SC_MANAGER_CONNECT);
    if (!scm) { out.coverage = L"Cannot open the Service Control Manager: " + Win32Err(GetLastError()); return out; }
    const DWORD typesWanted = drivers ? (DWORD)SERVICE_DRIVER : (DWORD)(SERVICE_WIN32 | SERVICE_USER_SERVICE | SERVICE_USERSERVICE_INSTANCE);
    DWORD need = 0, cnt = 0, resume = 0; DWORD types = typesWanted;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, types, SERVICE_STATE_ALL, nullptr, 0, &need, &cnt, &resume, nullptr);
    if (GetLastError() == ERROR_INVALID_PARAMETER && !drivers) { types = SERVICE_WIN32; need = cnt = resume = 0; EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, types, SERVICE_STATE_ALL, nullptr, 0, &need, &cnt, &resume, nullptr); }   // Windows 7/8: no per-user service types
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
        e.isDriver = drivers; e.critical = !drivers && IsCriticalService(e.name);
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
        if (drivers) {   // native path forms (F148); optional/virtual drivers are not alarmed (F150)
            wstring p = e.path; if (p.rfind(L"\\??\\", 0) == 0) p = p.substr(4); if (Lower(p).rfind(L"\\systemroot\\", 0) == 0) p = L"%SystemRoot%" + p.substr(11);
            if (p.empty()) p = L"%SystemRoot%\\System32\\drivers\\" + e.name + L".sys"; else if (p.size() > 1 && p[1] != L':' && p[0] != L'%' && p[0] != L'\\') p = L"%SystemRoot%\\" + p;
            e.path = p; e.ptype = (e.svcType & SERVICE_FILE_SYSTEM_DRIVER) ? L"File system driver" : L"Kernel driver"; e.account.clear();
            if (e.startType == SERVICE_BOOT_START || e.startType == SERVICE_SYSTEM_START) e.notes = L"Boot-critical class: read-only here";
        }
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
        if (!drivers && !pc.exe.empty() && !fi.exists && !FileExistsExpanded(pc.exe)) warn = L"File missing";
        else if (drivers) { if (!fi.exists && !FileExistsExpanded(pc.exe) && (e.startType <= SERVICE_AUTO_START) && e.stateCode == SERVICE_RUNNING) warn = L"File not found"; }
        else if (!microsoftPath && fi.exists) {
            FileInfo sg = InspectFile(pc.exe, true);
            e.signature = sg.sig == Sig::Valid ? L"Valid" + (sg.signer.empty() ? wstring() : L", signed by " + sg.signer) : sg.sig == Sig::Unsigned ? L"Not signed" : sg.sig == Sig::Invalid ? L"Not trusted (offline check)" : L"Could not check";
            if (sg.sig == Sig::Unsigned) warn = L"Unsigned"; else if (sg.sig == Sig::Invalid) warn = L"Signature not trusted";
        } else if (microsoftPath) e.signature = L"Not checked (Microsoft file version info)";
        if (!firstTokenIsExe && pc.exe.find(L' ') != wstring::npos) warn += (warn.empty() ? L"" : L" · ") + wstring(L"Unquoted path");
        e.warnings = warn;
        e.safety = drivers ? ((e.startType == SERVICE_BOOT_START || e.startType == SERVICE_SYSTEM_START) ? L"Boot/System" : (microsoftPath ? L"Windows" : L"Third-party")) : e.critical ? L"Critical" : (microsoftPath ? L"Windows" : L"Third-party");
        out.entries.push_back(std::move(e));
    }
    CloseServiceHandle(scm);
    out.coverage = L"Enumerated " + std::to_wstring(cnt) + (drivers ? L" drivers (read-only, no changes offered) (" : L" Win32 services (") + std::to_wstring(running) + L" running); configuration unreadable for " + std::to_wstring(noCfg) +
                   (drivers ? L"" : L". Drivers are on the Drivers page") + L". Visibility is limited to what your account may query. 'Critical' comes from a built-in list and is not exhaustive.";
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
int SvcStopIfRunning(const wstring& name, wstring& err) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) { err = Win32Err(GetLastError()); return -1; } int r = 0;
    SC_HANDLE sv = OpenServiceW(scm, name.c_str(), SERVICE_QUERY_STATUS); SERVICE_STATUS_PROCESS ss{}; DWORD n = 0;
    if (!sv) r = GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? 0 : (err = Win32Err(GetLastError()), -1);
    else { if (QueryServiceStatusEx(sv, SC_STATUS_PROCESS_INFO, (LPBYTE)&ss, sizeof ss, &n) && ss.dwCurrentState != SERVICE_STOPPED) { wstring e2; if (IsCriticalService(name)) { err = L"critical service"; r = -1; } else { for (auto& d : ActiveDependents(name)) { if (!StopOne(scm, d, e2)) { err = L"dependent " + d + L": " + e2; r = -1; break; } } if (r == 0) r = StopOne(scm, name, err) ? 1 : -1; } } CloseServiceHandle(sv); }
    CloseServiceHandle(scm); return r;
}
bool SvcStartByName(const wstring& name, wstring& err) { SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) { err = Win32Err(GetLastError()); return false; } bool ok = StartOne(scm, name, err); CloseServiceHandle(scm); return ok; }

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


// ---- kill process (expert, guarded) ----
bool KillServiceProcess(const ServiceEntry& s, const std::vector<ServiceEntry>& all, wstring& err) {
    if (s.isDriver) { err = L"Drivers are read-only."; return false; }
    if (s.critical) { err = L"Blocked: critical service."; return false; }
    if (!(s.svcType & SERVICE_WIN32_OWN_PROCESS) || (s.svcType & SERVICE_WIN32_SHARE_PROCESS)) { err = L"Blocked: this service shares a process with others. Stop it normally instead."; return false; }
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) { err = Win32Err(GetLastError()); return false; }
    SC_HANDLE sv = OpenServiceW(scm, s.name.c_str(), SERVICE_QUERY_STATUS); SERVICE_STATUS_PROCESS ss{}; DWORD n = 0; bool ok = false;
    if (sv && QueryServiceStatusEx(sv, SC_STATUS_PROCESS_INFO, (LPBYTE)&ss, sizeof ss, &n)) {
        if (ss.dwProcessId == 0) err = L"The service is not running.";
        else {
            for (auto& o : all) if (o.name != s.name && o.pid == ss.dwProcessId) { err = L"Blocked: process also hosts '" + o.display + L"'."; ss.dwProcessId = 0; break; }
            if (ss.dwProcessId) {
                HANDLE ph = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ss.dwProcessId);
                if (!ph) err = Win32Err(GetLastError());
                else {
                    wchar_t img[MAX_PATH * 2]; DWORD il = MAX_PATH * 2; bool same = QueryFullProcessImageNameW(ph, 0, img, &il) && Lower(img) == Lower(ExpandEnv(s.program));   // PID-reuse / identity check
                    if (!same) err = L"The process behind this PID no longer matches the service executable; not terminated.";
                    else { ok = TerminateProcess(ph, 1) != 0; if (!ok) err = Win32Err(GetLastError()); }
                    CloseHandle(ph);
                }
            }
        }
    } else err = Win32Err(GetLastError());
    if (sv) CloseServiceHandle(sv); CloseServiceHandle(scm);
    WriteJournal(s.name, L"process kill", L"running pid " + std::to_wstring(s.pid), ok ? L"terminated" : L"unchanged", ok ? L"ok (force)" : L"failed: " + err, false);
    return ok;
}

// ---- configuration edits ----
bool EditServiceNames(const ServiceEntry& s, const wstring& display, const wstring& desc, wstring& err) {
    if (display.empty() || display.size() > 256 || desc.size() > 2000) { err = L"Name must be 1-256 characters; description up to 2000."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) { err = Win32Err(GetLastError()); return false; }
    SC_HANDLE sv = OpenServiceW(scm, s.name.c_str(), SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG); bool ok = false;
    if (!sv) { DWORD e = GetLastError(); err = e == ERROR_ACCESS_DENIED ? L"Access denied. Administrator rights are required." : Win32Err(e); }
    else {
        ok = ChangeServiceConfigW(sv, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, display.c_str()) != 0;
        if (ok) { SERVICE_DESCRIPTIONW d{}; wstring dd = desc; d.lpDescription = (LPWSTR)dd.c_str(); ok = ChangeServiceConfig2W(sv, SERVICE_CONFIG_DESCRIPTION, &d) != 0; }
        if (!ok) err = Win32Err(GetLastError());
        else if (QueryDescription(sv) != desc) { ok = false; err = L"Read-back of the description did not match."; }
        CloseServiceHandle(sv);
    }
    CloseServiceHandle(scm);
    WriteJournal(s.name, L"name/description", s.display, ok ? display : L"unchanged", ok ? L"ok" : L"failed: " + err, false);
    return ok;
}
bool SetServiceAccount(const ServiceEntry& s, int which, wstring& err) {
    if (s.critical) { err = L"Blocked: critical service."; return false; }
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    static const wchar_t* names[] = {L"LocalSystem", L"NT AUTHORITY\\LocalService", L"NT AUTHORITY\\NetworkService"};
    if (which < 0 || which > 2) { err = L"Unsupported account."; return false; }
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) { err = Win32Err(GetLastError()); return false; }
    SC_HANDLE sv = OpenServiceW(scm, s.name.c_str(), SERVICE_CHANGE_CONFIG); bool ok = false;
    if (!sv) { DWORD e = GetLastError(); err = e == ERROR_ACCESS_DENIED ? L"Access denied. Administrator rights are required." : Win32Err(e); }
    else { ok = ChangeServiceConfigW(sv, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, names[which], nullptr, nullptr) != 0; if (!ok) err = Win32Err(GetLastError()); CloseServiceHandle(sv); }
    CloseServiceHandle(scm);
    WriteJournal(s.name, L"log-on account", s.account, ok ? names[which] : L"unchanged", ok ? L"ok (applies at next start; not undoable)" : L"failed: " + err, false);
    return ok;
}
bool GetRecovery(const wstring& name, Recovery& r) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) return false; bool ok = false;
    if (SC_HANDLE sv = OpenServiceW(scm, name.c_str(), SERVICE_QUERY_CONFIG)) {
        DWORD n = 0; QueryServiceConfig2W(sv, SERVICE_CONFIG_FAILURE_ACTIONS, nullptr, 0, &n); std::vector<BYTE> b(n ? n : 1);
        if (QueryServiceConfig2W(sv, SERVICE_CONFIG_FAILURE_ACTIONS, b.data(), n, &n)) {
            auto* fa = (SERVICE_FAILURE_ACTIONSW*)b.data(); r = Recovery(); r.resetDays = fa->dwResetPeriod == INFINITE ? 0 : fa->dwResetPeriod / 86400; r.cmd = fa->lpCommand ? fa->lpCommand : L"";
            for (DWORD i = 0; i < 3 && i < fa->cActions && fa->lpsaActions; ++i) { auto t = fa->lpsaActions[i].Type; r.act[i] = t == SC_ACTION_RESTART ? 1 : t == SC_ACTION_REBOOT ? 2 : t == SC_ACTION_RUN_COMMAND ? 3 : 0; r.delayMin[i] = fa->lpsaActions[i].Delay / 60000; }
            SERVICE_FAILURE_ACTIONS_FLAG fl{}; DWORD m = 0; if (QueryServiceConfig2W(sv, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, (LPBYTE)&fl, sizeof fl, &m)) r.onNonCrash = fl.fFailureActionsOnNonCrashFailures != 0;
            ok = true;
        }
        CloseServiceHandle(sv);
    }
    CloseServiceHandle(scm); return ok;
}
bool SetRecovery(const wstring& name, const Recovery& r, wstring& err) {
    if (!JournalWritable()) { err = L"Cannot write the change journal; nothing was changed."; return false; }
    for (int i = 0; i < 3; ++i) if (r.act[i] == 2) { err = L"'Restart the computer' is not offered here."; return false; }
    if (IsCriticalService(name)) { err = L"Blocked: critical service."; return false; }
    Recovery before; GetRecovery(name, before);
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (!scm) { err = Win32Err(GetLastError()); return false; }
    SC_HANDLE sv = OpenServiceW(scm, name.c_str(), SERVICE_CHANGE_CONFIG | SERVICE_QUERY_CONFIG); bool ok = false;
    if (!sv) { DWORD e = GetLastError(); err = e == ERROR_ACCESS_DENIED ? L"Access denied. Administrator rights are required." : Win32Err(e); }
    else {
        SC_ACTION acts[3]; for (int i = 0; i < 3; ++i) { acts[i].Type = r.act[i] == 1 ? SC_ACTION_RESTART : r.act[i] == 3 ? SC_ACTION_RUN_COMMAND : SC_ACTION_NONE; acts[i].Delay = r.delayMin[i] * 60000; }
        wstring cmd = r.cmd; SERVICE_FAILURE_ACTIONSW fa{}; fa.dwResetPeriod = r.resetDays * 86400; fa.lpCommand = cmd.empty() ? nullptr : (LPWSTR)cmd.c_str(); fa.cActions = 3; fa.lpsaActions = acts;
        ok = ChangeServiceConfig2W(sv, SERVICE_CONFIG_FAILURE_ACTIONS, &fa) != 0;
        if (ok) { SERVICE_FAILURE_ACTIONS_FLAG fl{}; fl.fFailureActionsOnNonCrashFailures = r.onNonCrash; ok = ChangeServiceConfig2W(sv, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &fl) != 0; }
        if (!ok) err = Win32Err(GetLastError());
        else { Recovery chk; if (!GetRecovery(name, chk) || chk.act[0] != r.act[0] || chk.act[1] != r.act[1] || chk.act[2] != r.act[2]) { ok = false; err = L"Read-back did not match."; } }
        CloseServiceHandle(sv);
    }
    CloseServiceHandle(scm);
    auto d = [](const Recovery& x) { return std::to_wstring(x.act[0]) + L"/" + std::to_wstring(x.act[1]) + L"/" + std::to_wstring(x.act[2]); };
    WriteJournal(name, L"recovery", L"actions " + d(before), ok ? L"actions " + d(r) : L"unchanged", ok ? L"ok" : L"failed: " + err, false);
    return ok;
}

// ---- profiles & snapshots ----
struct BuiltinItem { const wchar_t* svc; DWORD type; };
struct BuiltinDef { const wchar_t* id; const wchar_t* label; const wchar_t* impact; std::vector<BuiltinItem> items; };
static const std::vector<BuiltinDef>& Builtins() {
    static const std::vector<BuiltinDef> b = {
        {L"b.privacy", L"Privacy: reduce telemetry", L"Stops the diagnostics/telemetry upload services. Windows keeps working; some feedback and diagnostics features stop reporting.", {{L"DiagTrack", SERVICE_DISABLED}, {L"dmwappushservice", SERVICE_DISABLED}}},
        {L"b.light", L"Lighter: fewer background services", L"Sets rarely-used optional services to Manual so they only start when needed. Maps downloads, retail-demo, fax, media sharing and location services may not start automatically.", {{L"MapsBroker", SERVICE_DEMAND_START}, {L"RetailDemo", SERVICE_DISABLED}, {L"Fax", SERVICE_DISABLED}, {L"WMPNetworkSvc", SERVICE_DEMAND_START}, {L"lfsvc", SERVICE_DEMAND_START}}},
        {L"b.xbox", L"No Xbox (not a gamer)", L"Xbox sign-in, game-save sync and accessory services start only on demand. Xbox app, Game Bar sign-in and cloud saves may not work until the services are started.", {{L"XblAuthManager", SERVICE_DEMAND_START}, {L"XblGameSave", SERVICE_DEMAND_START}, {L"XboxGipSvc", SERVICE_DEMAND_START}, {L"XboxNetApiSvc", SERVICE_DEMAND_START}}},
        {L"b.printer", L"No printer or scanner", L"Disables the print spooler and scanner service. ALL printing stops, including Microsoft Print to PDF and XPS, until you re-enable the Print Spooler.", {{L"Spooler", SERVICE_DISABLED}, {L"PrintNotify", SERVICE_DEMAND_START}, {L"stisvc", SERVICE_DEMAND_START}}},
        {L"b.security", L"Security hardening", L"Disables remote registry editing and UPnP/SSDP discovery. Network device discovery and UPnP media/devices stop working; remote registry tools will fail.", {{L"RemoteRegistry", SERVICE_DISABLED}, {L"SSDPSRV", SERVICE_DISABLED}, {L"upnphost", SERVICE_DISABLED}}}};
    return b;
}
static wstring ProfDir() { return SubDir(L"profiles"); }
std::vector<ProfileInfo> ListProfiles() {
    std::vector<ProfileInfo> v;
    for (auto& b : Builtins()) { ProfileInfo p; p.id = b.id; p.label = b.label; p.impact = b.impact; p.builtin = true; v.push_back(p); }
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((ProfDir() + L"\\*.fsuprofile").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) { do { ProfileInfo p; p.path = ProfDir() + L"\\" + fd.cFileName; p.id = p.path; p.label = fd.cFileName; if (p.label.size() > 11) p.label.resize(p.label.size() - 11);
        std::ifstream f(std::filesystem::path(p.path), std::ios::binary); std::string ln; for (int i = 0; i < 4 && std::getline(f, ln); ++i) { auto w = FromUtf8(ln); if (!w.empty() && w.back() == L'\r') w.pop_back(); if (w.rfind(L"name=", 0) == 0) p.label = w.substr(5); }
        v.push_back(p); } while (FindNextFileW(h, &fd)); FindClose(h); }
    return v;
}
static wstring OsBuild() { wchar_t b[64] = L"?"; DWORD n = sizeof b; HKEY k; if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) { RegQueryValueExW(k, L"CurrentBuildNumber", nullptr, nullptr, (LPBYTE)b, &n); RegCloseKey(k); } return b; }
bool SaveProfile(const wstring& label, const std::vector<ServiceEntry>& items, wstring& path, wstring& err) {
    if (items.empty()) { err = L"Nothing selected."; return false; }
    wstring clean = label; for (auto& c : clean) if (wcschr(L"\\/:*?\"<>|\r\n\t", c)) c = L'_'; if (clean.empty() || clean.size() > 80) { err = L"Enter a name (up to 80 characters)."; return false; }
    wstring body = L"FSUPROFILE1\nname=" + clean + L"\nos=" + OsBuild() + L"\ntime=" + NowIsoUtc() + L"\n";
    for (auto& e : items) if (!e.isDriver && e.startType >= 2 && e.startType <= 4) body += L"svc=" + e.name + L"\t" + std::to_wstring(e.startType) + L"\t" + (e.delayed ? L"1" : L"0") + L"\n";   // no secrets, only start types
    path = ProfDir() + L"\\" + clean + L".fsuprofile";
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) path = ProfDir() + L"\\" + clean + L" " + std::to_wstring(GetTickCount() % 10000) + L".fsuprofile";
    if (!AtomicWrite(path, ToUtf8(body))) { err = L"Could not write the profile file."; return false; } return true;
}
static wstring TypeLabel(DWORD t, bool d) { return t == SERVICE_AUTO_START ? (d ? L"Automatic (delayed)" : L"Automatic") : t == SERVICE_DEMAND_START ? L"Manual" : t == SERVICE_DISABLED ? L"Disabled" : L"?"; }
bool BuildPlan(const ProfileInfo& p, const std::vector<ServiceEntry>& cur, std::vector<PlanItem>& plan, wstring& note, wstring& err) {
    std::map<wstring, const ServiceEntry*> by; for (auto& e : cur) by[Lower(e.name)] = &e;
    std::vector<std::pair<wstring, std::pair<DWORD, bool>>> want;
    if (p.builtin) { for (auto& b : Builtins()) if (p.id == b.id) { note = b.impact; for (auto& i : b.items) want.push_back({i.svc, {i.type, false}}); } }
    else {
        std::ifstream f(std::filesystem::path(p.path), std::ios::binary); if (!f) { err = L"Cannot open the profile."; return false; }
        std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()); if (all.size() > 2000000) { err = L"Profile is unexpectedly large; refusing."; return false; }   // F214
        size_t pos = 0; int lines = 0; bool first = true; wstring os;
        while (pos < all.size() && ++lines < 6000) {
            size_t nl = all.find('\n', pos); wstring w = FromUtf8(all.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos)); pos = nl == std::string::npos ? all.size() : nl + 1; if (!w.empty() && w.back() == L'\r') w.pop_back();
            if (first) { if (w != L"FSUPROFILE1") { err = L"Not a valid profile file."; return false; } first = false; continue; }
            if (w.rfind(L"os=", 0) == 0) os = w.substr(3);
            else if (w.rfind(L"svc=", 0) == 0) { size_t a = w.find(L'\t'), b = w.rfind(L'\t'); if (a == wstring::npos || a == b) continue;
                wstring nm = w.substr(4, a - 4); int t = _wtoi(w.substr(a + 1, b - a - 1).c_str()); if (nm.empty() || nm.size() > 256 || t < 2 || t > 4) continue; want.push_back({nm, {(DWORD)t, w.substr(b + 1) == L"1"}}); }
        }
        if (first) { err = L"Empty profile."; return false; }
        note = L"Custom profile. " + (os.empty() || os == OsBuild() ? wstring(L"Review the list before applying.") : L"Saved on Windows build " + os + L"; this PC is build " + OsBuild() + L". Review carefully - services may differ.");
    }
    for (auto& w : want) {
        PlanItem it; it.name = w.first; it.type = w.second.first; it.delayed = w.second.second; it.to = TypeLabel(it.type, it.delayed);
        auto f = by.find(Lower(w.first)); if (f == by.end()) { it.display = w.first; it.skip = true; it.why = L"not present on this PC"; plan.push_back(it); continue; }
        auto* e = f->second; it.name = e->name; it.display = e->display; it.from = TypeLabel(e->startType, e->delayed); it.oldType = e->startType; it.oldDelayed = e->delayed;
        if (e->startType == it.type && e->delayed == it.delayed) { it.skip = true; it.why = L"already set"; }
        else if (e->critical && it.type == SERVICE_DISABLED) { it.skip = true; it.why = L"critical service - never disabled by a profile"; }
        else if (e->startType == SERVICE_BOOT_START || e->startType == SERVICE_SYSTEM_START) { it.skip = true; it.why = L"driver-class start type"; }
        plan.push_back(it);
    }
    return true;
}
wstring PlanText(const std::vector<PlanItem>& plan) {
    wstring o; for (auto& p : plan) { o += (p.skip ? L"  skip   " : L"  change ") + p.display + L" (" + p.name + L")"; if (p.skip) o += L"  - " + p.why; else o += L":  " + p.from + L"  ->  " + p.to; o += L"\r\n"; } return o;
}

// ---- history (Service Control Manager events, bounded) ----
static wstring EventsFor(const ServiceEntry& s) {
    wstring out; EVT_HANDLE q = EvtQuery(nullptr, L"System", L"*[System[Provider[@Name='Service Control Manager']]]", EvtQueryChannelPath | EvtQueryReverseDirection);
    if (!q) return L"Event history is not available (" + Win32Err(GetLastError()) + L"). The System event log may be disabled or access denied.";
    int scanned = 0, shown = 0; EVT_HANDLE hs[32]; DWORD got = 0; wstring dn = L">" + s.display + L"<", nm = L">" + s.name + L"<";
    while (scanned < 2500 && shown < 40 && EvtNext(q, 32, hs, 1000, 0, &got)) {
        for (DWORD i = 0; i < got; ++i) {
            ++scanned; DWORD need = 0, props = 0; EvtRender(nullptr, hs[i], EvtRenderEventXml, 0, nullptr, &need, &props);
            if (need && need < 200000) { std::vector<wchar_t> xml(need / 2 + 2); if (EvtRender(nullptr, hs[i], EvtRenderEventXml, need, xml.data(), &need, &props)) {
                wstring x(xml.data());
                if (shown < 40 && (x.find(dn) != wstring::npos || x.find(nm) != wstring::npos)) {
                    auto between = [&](const wchar_t* a, const wchar_t* b) { size_t p = x.find(a); if (p == wstring::npos) return wstring(); p += wcslen(a); size_t e = x.find(b, p); return e == wstring::npos ? wstring() : x.substr(p, e - p); };
                    wstring id = between(L"<EventID", L"</EventID>"); size_t gt = id.find(L'>'); if (gt != wstring::npos) id = id.substr(gt + 1);
                    wstring t = between(L"SystemTime='", L"'"); if (t.size() >= 19) t = t.substr(0, 10) + L" " + t.substr(11, 8);
                    int n = _wtoi(id.c_str()); const wchar_t* what = n == 7036 ? L"state change" : n == 7034 ? L"terminated unexpectedly" : n == 7031 ? L"terminated, recovery action taken" : n == 7045 ? L"installed" : n == 7040 ? L"start type changed" : n == 7000 ? L"failed to start" : n == 7009 ? L"start timed out" : n == 7023 ? L"terminated with error" : n == 7024 ? L"terminated with service error" : L"event";
                    wstring d; size_t p = 0; int c = 0; while (c < 4 && (p = x.find(L"<Data", p)) != wstring::npos) { size_t g = x.find(L'>', p); size_t e = x.find(L"</Data>", g); if (g == wstring::npos || e == wstring::npos) break; if (c) d += L" "; d += x.substr(g + 1, e - g - 1); p = e; ++c; }
                    out += t + L"   [" + id + L"] " + what + L"   " + d + L"\r\n"; ++shown;
                } } }
            EvtClose(hs[i]);
        }
    }
    EvtClose(q);
    if (out.empty()) out = L"No recent Service Control Manager events mention this service (scanned the latest " + std::to_wstring(scanned) + L" events). The log may be disabled or cleared.";
    else out = L"Most recent events from the System log (Service Control Manager), newest first:\r\n\r\n" + out;
    return out;
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
    else if (tab == 4) t = EventsFor(s);
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
