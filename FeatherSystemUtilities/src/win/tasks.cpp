// Scheduled tasks manager: lists every Task Scheduler task (not only startup ones) and enables/disables it.
// Disabling goes through SetStartupEnabled so it is journaled and reversible exactly like startup tasks.
#include "app.h"
#include <taskschd.h>
#include <comdef.h>
#include <oleauto.h>
namespace fsu {
using std::wstring;

static wstring LowerW(wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static wstring DateText(DATE d) { if (d < 1.0) return L""; SYSTEMTIME st; if (!VariantTimeToSystemTime(d, &st) || st.wYear < 1990) return L""; wchar_t b[40]; swprintf(b, 40, L"%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute); return b; }
// Windows' own tasks stay protected, except a short list of telemetry / feedback groups that users commonly switch off.
static bool ProtectedTask(const wstring& path) {
    wstring p = LowerW(path);
    if (p.rfind(L"\\microsoft\\windows\\", 0) != 0) return false;
    static const wchar_t* ok[] = {L"\\microsoft\\windows\\customer experience improvement program\\", L"\\microsoft\\windows\\application experience\\", L"\\microsoft\\windows\\feedback\\", L"\\microsoft\\windows\\windows error reporting\\", L"\\microsoft\\windows\\diskdiagnostic\\microsoft-windows-diskdiagnosticdatacollector", L"\\microsoft\\windows\\autochk\\proxy", L"\\microsoft\\windows\\device information\\"};
    for (auto* o : ok) if (p.rfind(o, 0) == 0) return false;
    return true;
}
static const wchar_t* TrigName(TASK_TRIGGER_TYPE2 t) {
    switch (t) { case TASK_TRIGGER_EVENT: return L"On an event"; case TASK_TRIGGER_TIME: return L"One time"; case TASK_TRIGGER_DAILY: return L"Daily"; case TASK_TRIGGER_WEEKLY: return L"Weekly"; case TASK_TRIGGER_MONTHLY: case TASK_TRIGGER_MONTHLYDOW: return L"Monthly";
        case TASK_TRIGGER_IDLE: return L"When idle"; case TASK_TRIGGER_REGISTRATION: return L"When created"; case TASK_TRIGGER_BOOT: return L"At startup"; case TASK_TRIGGER_LOGON: return L"At sign-in"; case TASK_TRIGGER_SESSION_STATE_CHANGE: return L"On session change"; default: return L"Other"; }
}
static void Walk(ITaskFolder* f, std::vector<TaskInfo>& out, int depth, int& denied) {
    if (!f || depth > 10 || out.size() > 4000) return;
    IRegisteredTaskCollection* tc = nullptr;
    if (SUCCEEDED(f->GetTasks(TASK_ENUM_HIDDEN, &tc)) && tc) {
        LONG n = 0; tc->get_Count(&n);
        for (LONG i = 1; i <= n; ++i) {
            IRegisteredTask* t = nullptr; VARIANT v; VariantInit(&v); v.vt = VT_I4; v.lVal = i;
            if (FAILED(tc->get_Item(v, &t)) || !t) continue;
            TaskInfo ti; BSTR nm = nullptr, path = nullptr; VARIANT_BOOL en = VARIANT_TRUE; TASK_STATE ts = TASK_STATE_UNKNOWN; DATE lr = 0, nr = 0; LONG res = 0;
            t->get_Name(&nm); t->get_Path(&path); t->get_Enabled(&en); t->get_State(&ts); t->get_LastRunTime(&lr); t->get_NextRunTime(&nr); t->get_LastTaskResult(&res);
            ti.name = nm ? nm : L""; ti.path = path ? path : L""; ti.enabled = en != VARIANT_FALSE; ti.lastResult = res; ti.lastRun = DateText(lr); ti.nextRun = ti.enabled ? DateText(nr) : L"";
            size_t sl = ti.path.find_last_of(L'\\'); ti.folder = sl == wstring::npos || sl == 0 ? L"\\" : ti.path.substr(0, sl);
            ti.state = !ti.enabled ? L"Disabled" : ts == TASK_STATE_RUNNING ? L"Running" : ts == TASK_STATE_QUEUED ? L"Queued" : L"Ready";
            ti.microsoft = LowerW(ti.path).rfind(L"\\microsoft\\", 0) == 0; ti.canToggle = !ProtectedTask(ti.path);
            ITaskDefinition* def = nullptr;
            if (SUCCEEDED(t->get_Definition(&def)) && def) {
                IRegistrationInfo* ri = nullptr; if (SUCCEEDED(def->get_RegistrationInfo(&ri)) && ri) { BSTR a = nullptr, d = nullptr; ri->get_Author(&a); ri->get_Description(&d); if (a) { ti.author = a; SysFreeString(a); } if (d) { ti.why = d; SysFreeString(d); } ri->Release(); }
                ITaskSettings* se = nullptr; if (SUCCEEDED(def->get_Settings(&se)) && se) { VARIANT_BOOL h = VARIANT_FALSE; se->get_Hidden(&h); ti.hidden = h != VARIANT_FALSE; se->Release(); }
                ITriggerCollection* trs = nullptr;
                if (SUCCEEDED(def->get_Triggers(&trs)) && trs) {
                    LONG tn = 0; trs->get_Count(&tn);
                    for (LONG k = 1; k <= tn && k <= 6; ++k) { ITrigger* tr = nullptr; if (FAILED(trs->get_Item(k, &tr)) || !tr) continue; TASK_TRIGGER_TYPE2 ty; if (SUCCEEDED(tr->get_Type(&ty))) { if (ty == TASK_TRIGGER_LOGON || ty == TASK_TRIGGER_BOOT) ti.startup = true; wstring nmx = TrigName(ty); if (ti.triggers.find(nmx) == wstring::npos) ti.triggers += (ti.triggers.empty() ? L"" : L", ") + nmx; } tr->Release(); }
                    if (tn == 0) ti.triggers = L"Run manually"; trs->Release();
                }
                IActionCollection* acs = nullptr;
                if (SUCCEEDED(def->get_Actions(&acs)) && acs) { LONG an = 0; acs->get_Count(&an);
                    for (LONG k = 1; k <= an; ++k) { IAction* ac = nullptr; if (FAILED(acs->get_Item(k, &ac)) || !ac) continue; IExecAction* ex = nullptr;
                        if (SUCCEEDED(ac->QueryInterface(IID_IExecAction, (void**)&ex)) && ex) { BSTR p = nullptr, a = nullptr; ex->get_Path(&p); ex->get_Arguments(&a); wstring cmd = p ? p : L""; if (a && *a) cmd += wstring(L" ") + a; if (ti.command.empty()) ti.command = cmd; else ti.command += L"  (+ more actions)"; if (p) SysFreeString(p); if (a) SysFreeString(a); ex->Release(); }
                        else if (ti.command.empty()) ti.command = L"(not a program: e-mail, message or COM handler)";
                        ac->Release(); }
                    acs->Release(); }
                def->Release();
            } else ++denied;
            if (nm) SysFreeString(nm); if (path) SysFreeString(path); t->Release(); out.push_back(std::move(ti));
        }
        tc->Release();
    }
    ITaskFolderCollection* fc = nullptr;
    if (SUCCEEDED(f->GetFolders(0, &fc)) && fc) { LONG n = 0; fc->get_Count(&n);
        for (LONG i = 1; i <= n; ++i) { ITaskFolder* sub = nullptr; VARIANT v; VariantInit(&v); v.vt = VT_I4; v.lVal = i; if (SUCCEEDED(fc->get_Item(v, &sub)) && sub) { Walk(sub, out, depth + 1, denied); sub->Release(); } }
        fc->Release(); }
}
std::vector<TaskInfo> ListAllTasks(wstring& err) {
    std::vector<TaskInfo> out; ITaskService* svc = nullptr; int denied = 0;
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (void**)&svc)) || !svc) { err = L"Task Scheduler is not available."; return out; }
    VARIANT e; VariantInit(&e);
    if (SUCCEEDED(svc->Connect(e, e, e, e))) { ITaskFolder* root = nullptr; BSTR r = SysAllocString(L"\\"); if (SUCCEEDED(svc->GetFolder(r, &root)) && root) { Walk(root, out, 0, denied); root->Release(); } else err = L"Could not open the Task Scheduler library."; SysFreeString(r); }
    else err = L"Cannot connect to Task Scheduler (it is not started in Safe Mode, or the service is stopped).";
    svc->Release(); if (denied && err.empty()) err = std::to_wstring(denied) + L" task(s) could not be read (administrator rights may be needed for details)."; return out;
}
bool SetTaskEnabledByPath(const wstring& path, bool enable, wstring& err) {
    wstring e; auto all = ListAllTasks(e); for (auto& t : all) if (_wcsicmp(t.path.c_str(), path.c_str()) == 0) {
        if (!enable && !t.canToggle) { err = L"This is one of Windows' own maintenance tasks, so Feather leaves it alone."; return false; }
        StartupEntry se; se.name = t.name; se.taskPath = t.path; se.location = L"Task Scheduler: " + t.path; se.type = L"Scheduled task"; se.toggle = Toggle::Task; se.key = L"task|" + LowerW(t.path);
        return SetStartupEnabled(se, enable, err);
    }
    err = L"That task no longer exists."; return false;
}
}
