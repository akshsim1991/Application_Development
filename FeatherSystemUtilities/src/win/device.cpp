// Device diagnosis and repair: explains WHY a device reports a problem (problem code, Windows' own failure status, driver service state,
// Kernel-PnP log entries) and offers safe, journaled repair actions (rescan, restart device, enable, enable driver service, reinstall).
#include "app.h"
#include "../core/text.h"
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpropdef.h>
#include <devpkey.h>
#include <winevt.h>
#include <algorithm>

namespace fsu {
using std::wstring;

static wstring Hex32(unsigned v) { wchar_t b[16]; swprintf(b, 16, L"0x%08X", v); return b; }
const wchar_t* NtStatusText(unsigned s) {
    switch (s) { case 0: return L"No failure status was recorded.";
    case 0xC0000001: return L"STATUS_UNSUCCESSFUL: the driver reported a general failure without a specific reason.";
    case 0xC000000D: return L"STATUS_INVALID_PARAMETER: the driver received a setting it cannot use.";
    case 0xC0000010: return L"STATUS_INVALID_DEVICE_REQUEST: the device or driver does not support the request Windows made.";
    case 0xC0000017: return L"STATUS_NO_MEMORY: the driver could not get enough memory.";
    case 0xC0000022: return L"STATUS_ACCESS_DENIED: the driver was not allowed to use a resource it needs.";
    case 0xC0000034: return L"STATUS_OBJECT_NAME_NOT_FOUND: a file or registry entry the driver needs is missing.";
    case 0xC000009A: return L"STATUS_INSUFFICIENT_RESOURCES: not enough system resources for the device.";
    case 0xC000009D: return L"STATUS_DEVICE_NOT_CONNECTED: the device is not connected or is not responding.";
    case 0xC00000BB: return L"STATUS_NOT_SUPPORTED: the device, firmware or driver does not support this operation.";
    case 0xC00000E5: return L"STATUS_INTERNAL_ERROR: the driver hit an internal error.";
    case 0xC0000185: return L"STATUS_IO_DEVICE_ERROR: the device reported an I/O error (possible hardware fault).";
    case 0xC0000225: return L"STATUS_NOT_FOUND: something the driver looks for (firmware entry, resource or file) was not found.";
    case 0xC00002B6: return L"STATUS_DEVICE_POWER_FAILURE: the device did not respond to a power request.";
    case 0xC0000428: return L"STATUS_INVALID_IMAGE_HASH: the driver's digital signature could not be verified.";
    default: return L"Windows recorded this status but Feather has no plain-language text for it. Search the code on Microsoft's site."; }
}
static bool ProtectedClass(const wstring& cls) { static const wchar_t* p[] = {L"system", L"hdc", L"scsiadapter", L"diskdrive", L"volume", L"volumesnapshot", L"processor", L"computer", L"keyboard", L"mouse", L"hidclass", L"usb", L"usbhub"}; wstring l = cls; for (auto& c : l) c = (wchar_t)towlower(c); for (auto* x : p) if (l == x) return true; return false; }
bool DeviceActionAllowed(const wstring& cls, const std::string& action) { if (action == "rescan" || action == "enableService") return true; return !ProtectedClass(cls); }

static bool Locate(const wstring& id, DEVINST& inst) { return CM_Locate_DevNodeW(&inst, (DEVINSTID_W)id.c_str(), CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS; }
static wstring RegProp(DEVINST inst, ULONG prop) { wchar_t b[1024] = {}; ULONG n = sizeof b - 2, ty = 0; if (CM_Get_DevNode_Registry_PropertyW(inst, prop, &ty, b, &n, 0) == CR_SUCCESS) return b; return L""; }
static wstring Tag(const std::string& xml, const char* name) { std::string key = std::string("Name='") + name + "'>"; size_t p = xml.find(key); if (p == std::string::npos) return L""; p += key.size(); size_t e = xml.find('<', p); return e == std::string::npos ? L"" : FromUtf8(xml.substr(p, e - p)); }

DeviceDiag DiagnoseDevice(const wstring& instanceId) {
    DeviceDiag d; d.instanceId = instanceId; DEVINST inst; if (!Locate(instanceId, inst)) return d; d.found = true;
    ULONG st = 0, pr = 0; if (CM_Get_DevNode_Status(&st, &pr, inst, 0) == CR_SUCCESS) { d.status = st; d.problem = (int)pr; }
    d.started = (st & DN_STARTED) != 0; d.driverLoaded = (st & DN_DRIVER_LOADED) != 0; d.hasProblem = (st & DN_HAS_PROBLEM) != 0; d.needsRestart = (st & DN_NEED_RESTART) != 0; d.disableable = (st & DN_DISABLEABLE) != 0;
    DEVPROPTYPE ty = 0; ULONG ps = 0, sz = sizeof ps; if (CM_Get_DevNode_PropertyW(inst, &DEVPKEY_Device_ProblemStatus, &ty, (PBYTE)&ps, &sz, 0) == CR_SUCCESS && sz == sizeof ps) { d.hasNtStatus = true; d.ntStatus = ps; }
    d.ntText = d.hasNtStatus ? NtStatusText(d.ntStatus) : L""; d.service = RegProp(inst, CM_DRP_SERVICE); d.desc = RegProp(inst, CM_DRP_DEVICEDESC); d.cls = RegProp(inst, CM_DRP_CLASS); d.upperFilters = RegProp(inst, CM_DRP_UPPERFILTERS); d.lowerFilters = RegProp(inst, CM_DRP_LOWERFILTERS);
    { wchar_t inf[512] = {}; sz = sizeof inf - 2; if (CM_Get_DevNode_PropertyW(inst, &DEVPKEY_Device_DriverInfPath, &ty, (PBYTE)inf, &sz, 0) == CR_SUCCESS) d.infPath = inf; }
    ULONG cf = 0; sz = sizeof cf; ULONG t2 = 0; if (CM_Get_DevNode_Registry_PropertyW(inst, CM_DRP_CONFIGFLAGS, &t2, &cf, &sz, 0) == CR_SUCCESS) d.disabledByUser = (cf & 0x1) != 0;
    if (!d.service.empty()) { SC_HANDLE m = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT); if (m) { SC_HANDLE s = OpenServiceW(m, d.service.c_str(), SERVICE_QUERY_CONFIG | SERVICE_QUERY_STATUS); if (s) { d.serviceFound = true; DWORD n = 0; QueryServiceConfigW(s, nullptr, 0, &n); std::vector<BYTE> b(n ? n : 1); if (QueryServiceConfigW(s, (LPQUERY_SERVICE_CONFIGW)b.data(), n, &n)) { auto* c = (LPQUERY_SERVICE_CONFIGW)b.data(); d.serviceStartType = c->dwStartType; d.servicePath = c->lpBinaryPathName ? c->lpBinaryPathName : L""; }
            SERVICE_STATUS ss{}; if (QueryServiceStatus(s, &ss)) d.serviceState = ss.dwCurrentState; CloseServiceHandle(s); } CloseServiceHandle(m); }
        if (!d.servicePath.empty()) { wstring p = d.servicePath; if (p.rfind(L"\\SystemRoot\\", 0) == 0) { wchar_t w[MAX_PATH]; GetWindowsDirectoryW(w, MAX_PATH); p = wstring(w) + p.substr(11); } else if (p.rfind(L"\\??\\", 0) == 0) p = p.substr(4); p = ExpandEnv(p); size_t sp = p.find(L".sys"); if (sp != wstring::npos) p = p.substr(0, sp + 4); d.serviceFileExists = GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; } }
    // Kernel-PnP: "Device ... had a problem starting" (event 411) names the exact failure Windows saw
    EVT_HANDLE q = EvtQuery(nullptr, L"Microsoft-Windows-Kernel-PnP/Configuration", L"*[System[(EventID=411 or EventID=410 or EventID=442 or EventID=441)]]", EvtQueryChannelPath | EvtQueryReverseDirection);
    if (q) { EVT_HANDLE evs[16]; DWORD got = 0; int scanned = 0; wstring want = instanceId; for (auto& c : want) c = (wchar_t)towlower(c);
        while (scanned < 600 && d.events.size() < 6 && EvtNext(q, 16, evs, 1000, 0, &got)) { for (DWORD i = 0; i < got; ++i) { ++scanned; DWORD need = 0, props = 0; EvtRender(nullptr, evs[i], EvtRenderEventXml, 0, nullptr, &need, &props);
                if (need && need < 100000 && d.events.size() < 6) { std::vector<wchar_t> x(need / 2 + 2); if (EvtRender(nullptr, evs[i], EvtRenderEventXml, need, x.data(), &need, &props)) { std::string xml = ToUtf8(x.data()); wstring dev = Tag(xml, "DeviceInstanceId"); for (auto& c : dev) c = (wchar_t)towlower(c);
                        if (dev == want) { DeviceEvent e; std::string idS = xml.substr(xml.find("<EventID") == std::string::npos ? 0 : xml.find("<EventID")); size_t gt = idS.find('>'), lt = idS.find('<', gt == std::string::npos ? 0 : gt); e.id = gt != std::string::npos && lt != std::string::npos ? atoi(idS.substr(gt + 1, lt - gt - 1).c_str()) : 0;
                            size_t tp = xml.find("SystemTime='"); if (tp != std::string::npos) { std::string t = xml.substr(tp + 12, 19); for (auto& c : t) if (c == 'T') c = ' '; e.time = FromUtf8(t); } e.problem = Tag(xml, "Problem"); e.status = Tag(xml, "Status"); e.driver = Tag(xml, "DriverName"); e.service = Tag(xml, "ServiceName"); d.events.push_back(e); } } }
                EvtClose(evs[i]); } }
        EvtClose(q); }
    return d;
}
static bool PropChange(const wstring& id, DWORD stateChange, wstring& err) {
    HDEVINFO h = SetupDiCreateDeviceInfoList(nullptr, nullptr); if (h == INVALID_HANDLE_VALUE) { err = Win32Err(GetLastError()); return false; } SP_DEVINFO_DATA did{sizeof did}; bool ok = false;
    if (!SetupDiOpenDeviceInfoW(h, id.c_str(), nullptr, 0, &did)) err = Win32Err(GetLastError());
    else { SP_PROPCHANGE_PARAMS p{}; p.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER); p.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE; p.StateChange = stateChange; p.Scope = DICS_FLAG_GLOBAL; p.HwProfile = 0;
        if (!SetupDiSetClassInstallParamsW(h, &did, &p.ClassInstallHeader, sizeof p) || !SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, h, &did)) err = Win32Err(GetLastError()); else ok = true; }
    SetupDiDestroyDeviceInfoList(h); return ok;
}
static bool Rescan(wstring& err) { DEVINST root; if (CM_Locate_DevNodeW(&root, nullptr, CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) { err = L"Could not reach the device tree."; return false; } CONFIGRET r = CM_Reenumerate_DevNode(root, CM_REENUMERATE_SYNCHRONOUS); if (r != CR_SUCCESS) { err = L"Windows could not rescan hardware (code " + std::to_wstring(r) + L")."; return false; } return true; }
static bool RemoveDevice(const wstring& id, wstring& err) {
    HDEVINFO h = SetupDiCreateDeviceInfoList(nullptr, nullptr); if (h == INVALID_HANDLE_VALUE) { err = Win32Err(GetLastError()); return false; } SP_DEVINFO_DATA did{sizeof did}; bool ok = false;
    if (!SetupDiOpenDeviceInfoW(h, id.c_str(), nullptr, 0, &did)) err = Win32Err(GetLastError());
    else { SP_REMOVEDEVICE_PARAMS rp{}; rp.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER); rp.ClassInstallHeader.InstallFunction = DIF_REMOVE; rp.Scope = DI_REMOVEDEVICE_GLOBAL; rp.HwProfile = 0;
        if (!SetupDiSetClassInstallParamsW(h, &did, &rp.ClassInstallHeader, sizeof rp) || !SetupDiCallClassInstaller(DIF_REMOVE, h, &did)) err = Win32Err(GetLastError()); else ok = true; }   // removes the device node only; the driver package stays in the driver store
    SetupDiDestroyDeviceInfoList(h); return ok;
}
DeviceFixResult FixDevice(const wstring& instanceId, const wstring& cls, const wstring& name, const std::string& action) {
    DeviceFixResult r; DeviceDiag before = DiagnoseDevice(instanceId); r.problemBefore = before.problem; wstring err;
    if (!DeviceActionAllowed(cls, action)) { r.message = L"Feather will not " + FromUtf8(action) + L" a " + cls + L" device: doing so could stop Windows from starting or reaching its disk."; return r; }
    if (!IsElevated()) { r.message = L"This repair needs administrator rights."; return r; }
    bool ok = false;
    if (action == "rescan") ok = Rescan(err);
    else if (action == "enable") ok = PropChange(instanceId, DICS_ENABLE, err);
    else if (action == "restart") { ok = PropChange(instanceId, DICS_DISABLE, err); Sleep(800); if (ok || true) { wstring e2; bool on = PropChange(instanceId, DICS_ENABLE, e2); if (!on && err.empty()) err = e2; ok = ok && on; } }
    else if (action == "enableService") { if (before.service.empty()) err = L"This device has no driver service to enable."; else if (before.serviceStartType != SERVICE_DISABLED) err = L"The driver service is not disabled (it is already set to start normally)."; else { wstring bf, e2; ok = SetServiceStartType(before.service, SERVICE_DEMAND_START, false, e2, bf); if (!ok) err = e2; else { wstring e3; PropChange(instanceId, DICS_DISABLE, e3); Sleep(500); PropChange(instanceId, DICS_ENABLE, e3); } } }
    else if (action == "reinstall") { ok = RemoveDevice(instanceId, err); if (ok) { Sleep(500); wstring e2; Rescan(e2); } }
    else err = L"Unknown repair action.";
    Sleep(1500); DeviceDiag after = DiagnoseDevice(instanceId); r.found = after.found; r.problemAfter = after.found ? after.problem : -1; r.ok = ok;
    r.fixed = after.found && after.problem == 0 && after.started; r.needsRestart = after.needsRestart;
    if (!ok) r.message = L"Windows refused the step: " + err; else if (r.fixed) r.message = L"Fixed: the device now starts and reports no problem."; else if (!after.found) r.message = L"The step ran but the device is no longer listed. Windows may need a moment to detect it again; refresh the list."; else r.message = L"The step ran, but Windows still reports code " + std::to_wstring(after.problem) + L" (" + ProblemText(after.problem) + L").";
    if (r.needsRestart) r.message += L" Windows says a restart is needed to finish.";
    WriteJournal(name, L"device repair: " + FromUtf8(action), L"code " + std::to_wstring(r.problemBefore), r.problemAfter < 0 ? L"device not listed" : L"code " + std::to_wstring(r.problemAfter), r.ok ? (r.fixed ? L"ok (fixed)" : L"ran, problem remains") : L"failed: " + err, false);
    return r;
}
}
