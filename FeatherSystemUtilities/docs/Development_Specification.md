# Feather System Utilities — Unified Application Development Specification

**Prepared:** 13 November 2026  
**Owner:** Akshaya Simha  
**Product name:** Feather System Utilities (final; formerly "Windows Startup & Services Manager").  
**Deliverable:** One installation file, one common application window, two integrated management modules.  
**Audience:** Claude Code working on the owner's existing portable applications.  
**Scope:** Research-backed feature inventory, inspection of supplied binaries, integration architecture, safety, testing, and roadmap. This document does not claim that implementation has been completed.

## 1. Direct instructions to Claude Code

Continue the owner's Startup Manager and Services Manager. Preserve useful existing behavior and native efficiency. Deliver one cohesive Windows application with Startup and Services pages inside the same window, shared settings, history, search conventions, themes, backups, and installer. A launcher with two buttons that opens the old EXEs separately does not satisfy the request. One installer may legitimately contain a UI executable plus narrow helper processes; the owner requested one setup file and common window, not necessarily one internal executable.

1. Locate the actual source repositories and their instructions. The supplied distribution directories contain binaries and one README, not the source needed for proper integration. Record source commit/version provenance before editing. Do not assume the README exactly matches the binaries.
2. Build both existing projects and map every requirement below to Existing/tested, Existing/unverified, Improve, New, Unsupported on this OS, or Deferred with reason. Preserve the original binaries and configuration before migration.
3. Prefer reuse of working native C++/Win32 modules after source inspection. Do not replace these sub-megabyte applications with a heavy web shell without a concrete justified decision.
4. Separate the reusable startup/service engines from their old main windows. Add pages to one shell; share commands, theming, logging, profiles, file inspection, and privilege handling.
5. Implement preview → backup → apply → read-back verification → result journal. Do not silently disable services or startup entries on launch/install, or claim that fewer running services necessarily make Windows faster.
6. Test mutations on disposable VMs and synthetic entries/services. An inspection of the owner's main computer is not permission to disable items, kill services, alter accounts, or reboot it.
7. Ship actual offline installer, source/build instructions, local help, migration guide, SBOM/notices, test report, and known limitations. Never mark a capability implemented solely because a control exists.

**Priority:** P0 Essential = required for first dependable unified release. P1 Recommended = professional productivity milestone. P2 Advanced = specialized later work. Safety rules apply at every priority. A long catalog is a roadmap, not a demand to expose unsafe half-working controls.

## 2. Inspection of the owner's applications

### 2.1 Inputs and provenance

| Item | Startup Manager | Services Manager |
|---|---|---|
| Supplied path | `C:\Users\AKSHSIM\OneDrive\SOFTWARE DEVELOPMENT\# Programmed\Windows based\WindowsStartupManager-x64\StartupManager.exe` | `C:\Users\AKSHSIM\OneDrive\SOFTWARE DEVELOPMENT\# Programmed\Windows based\WindowsServicesManager-x64\ServicesManager.exe` |
| File version | 2.0.0.0 | 2.2.0.0 |
| Size | 495,104 bytes | 555,520 bytes |
| Binary header | AMD64, PE32+, no CLR directory observed | AMD64, PE32+, no CLR directory observed |
| Authenticode | NotSigned | NotSigned |
| SHA-256 | `6AC9D1250892251E78F2218DE1CC21095837941D07029085C37E8D41D6720339` | `33F94CBCA071F70F69ED92C1B123ECB26F46E6B43D34FB4CCB8AB44B2F334FB6` |
| Distribution contents inspected | EXE | EXE and README.md |

These are observations of the supplied files, not proof that dependencies, security, or every feature are correct. The README describes a C++/CMake native rewrite and internal module names, but source files were not present in those distribution folders. Portable means no installation is needed; it does not guarantee settings/backups stay beside the EXE or leave no registry traces.

### 2.2 Evidence levels and findings

**Observed** means visible in the running UI or filesystem metadata. **Exercised** means a non-destructive interaction was tested. **Documented** means stated in the local README, not independently proven. **Unverified** must remain unverified until VM/source testing.

| Area | Evidence | Finding and required treatment |
|---|---|---|
| Separate windows | Observed | Two independent EXEs/windows; the requested shared shell and installer are new work. |
| Startup inventory | Observed | Registry entries, scheduled tasks, Explorer context-menu/overlay add-ons, user scope, command, location, publisher, warnings and sign-in-impact columns. Preserve these categories. |
| Startup search | Exercised | Ctrl+F focused search; typing a known item filtered the visible list to one row; Escape cleared the query. |
| Startup counts | Observed | Snapshot showed 114 total, 90 enabled, 24 disabled, 22 shown with system items hidden. Explain hidden/filter counts clearly; numbers are point-in-time, not permanent expectations. |
| Startup actions | Observed controls | Enable/Disable/Delete/Start later/Add/Restore/Undo and More menu exist. Mutation correctness was not tested. |
| Startup details | Exercised view | Details displayed source registry key, COM class for a shell extension, first-seen label, command, version, signature status, and warnings. |
| Online actions | Observed controls | Details exposes Check on VirusTotal and Search online. Neither was invoked. These require a conscious product policy, not silent migration into an offline core. |
| Startup settings | Observed, left unchanged | System/light/dark selection, refresh, confirmation option, system-item visibility, RunOnce visibility, always-admin, background watch and notifications. Background watcher performance/correctness unverified. |
| Dark-mode consistency | Observed issue | Startup main window/menu are dark, while details/settings content is light. Full consistent theming is an improvement requirement. |
| Accessibility | Observed gap | Startup search/list/headers appear in UI Automation; custom toolbar actions were absent from the captured tree. Add semantic keyboard-accessible toolbar providers. Services accessibility cannot be judged from this run because of integrity mismatch. |
| Service inventory | Observed | Dark service grid, status/start type including delayed/trigger labels, safety, PID/account/description/warnings; snapshot showed 311 services, 129 running. |
| Service controls | Observed controls | Start/Stop/Restart/Pause/Resume/Kill/Start type/Profiles/Drivers/search/filter/More visible. No service action was performed. |
| Elevated service window | Observed limitation | Window ran as Administrator; UI automation reported higher-integrity restrictions. Deep dialog and mutation testing was not completed. Do not infer accessibility failure from that restriction. |
| Service configuration/recovery/dependencies/events | Documented | README describes editable configuration/recovery and dependency/history tabs. Preserve after source/VM verification. |
| Profiles/snapshots | Documented | README describes built-in/custom profiles, automatic snapshots, settings sets and start-type undo. Broaden rollback only where recovery is genuinely possible. |
| Drivers | Documented plus visible button | README specifies read-only driver inventory. Keep read-only in the base product. |
| Remote management | Documented | README describes LAN service control and remote event logs. Keep separate and opt-in if retained; offline core must not contact hosts. |
| Advice traffic | Documented concern | README says online service advice is downloaded on first details view in a session, enabled by default, then cached. Bundle versioned local advice for the offline baseline; actual network behavior was not traced here. |
| Existing undo scope | Documented limitation | README says undo covers start-type changes, not full account/recovery/running-state restoration. UI must state scope rather than promising universal undo. |
| Packaging | Observed distributions | No installer supplied. Setup, upgrade/uninstall and old-settings migration need implementation. |

Inspection was deliberately non-destructive: no startup/service enable/disable, profile apply, account/recovery change, kill, driver edit, reboot, or online reputation submission was performed. Opening an app can update its own local caches/window preferences; this was not a zero-write forensic run. No end-to-end security audit, benchmark, network capture, source audit, or exhaustive functional test is claimed.

### 2.3 Immediate backlog before adding breadth

1. Integrate the engines into one window and remove separate-main-window dependence.
2. Finish dark styling and accessible custom toolbars.
3. Audit disable/delete/restore, delayed launch, snapshots, and start-type undo with source-specific VM fixtures.
4. Replace automatic advice downloads with bundled offline rules; explicitly handle existing online buttons.
5. Resolve identity/command parsing, script-host publisher attribution, shared-service process metrics, and false missing-file warnings.
6. Add precise change plans, durable rollback journals, and clean offline installer/migration tests.

## 3. Competition research and limits

Research date: 13 November 2026. Public official product documentation/source was reviewed; competing binaries were not installed or benchmarked. No credible finite search can prove coverage of every competing utility or every undocumented feature. This is a broad survey of directly relevant tools and Windows baselines. Features in the catalog are the proposed combined product, not assertions that every competitor implements them.

### 3.1 Verified public-documentation comparisons

| Reference | Documented capabilities relevant here | Product decision / feature groups |
|---|---|---|
| [Microsoft Autoruns/Autorunsc](https://learn.microsoft.com/en-us/sysinternals/downloads/autoruns) | Broad autostart locations, account views, signatures, source/file jumps, disable/delete, command-line exports and offline-system inspection. | Deep inventory, provenance, advanced read-only categories, local CLI. Do not inherit VirusTotal submission. F025–F072, F193–F204. |
| [Windows Startup Settings/Task Manager](https://support.microsoft.com/en-us/windows/experience/startup-boot/configure-startup-applications-in-windows) | Startup enable/disable, registered app list and impact presentation. | Familiar basics, honest impact availability and Windows state consistency. F013–F036, F073–F084. |
| [Autorun Organizer](https://www.chemtable.com/autorun-organizer.htm) | Batch actions, disabled-entry monitoring, boot-time diagrams, new-entry notifications, erroneous-record detection; also online ratings/VirusTotal/update features. | Adopt local history/monitoring and error review; omit cloud ratings/scans/automatic updates from offline core. F049–F084, F157–F180. |
| [Glary Quick Startup](https://www.glarysoft.com/quick-startup/download) and [Startup Manager documentation](https://www.glarysoft.com/kb/startup-manager/) | Entry detail/editing, launch delays, list export, startup categories and undo/backups. | Transparent delayed launch and reversible source-aware edits. Online comments/usefulness database are not copied. F037–F060, F157–F168. |
| [NirSoft WhatInStartup](https://www.nirsoft.net/utils/what_run_in_startup.html) | Startup details/control, external Windows installation inspection, persistent-disable monitoring/CLI. | Inspection and explicit opt-in change rules; no secret endless fight with installers or policies. F025–F060, F169–F180, F193–F204. |
| [CCleaner Startup Manager documentation](https://support.ccleaner.com/s/article/using-startup-manager-in-ccleaner-7?language=de) and [release notes](https://www.ccleaner.com/ccleaner/version-history/) | Startup-management pages including scheduled tasks and context-menu management; release/architecture differences exist. | Convenient categories and readable names; no registry-cleaning or broad cleaning scope. F025–F036, F061–F072. |
| [NirSoft ServiWin](https://www.nirsoft.net/utils/serviwin.html) | Services/drivers, status/start-type changes, file/publisher metadata, export/HTML, CLI and external-system views. | Rich service inspection and reporting; base driver management remains read-only. F085–F108, F145–F156, F193–F204. |
| [System Informer official source](https://github.com/winsiderss/systeminformer) and [releases](https://github.com/winsiderss/systeminformer/releases) | Resource inspection and create/edit/control services; release notes describe multiselect service operations. | Service/process correlation and batch progress; no kernel driver needed for this product's core. F085–F120, F181–F192. |
| [Easy Service Optimizer](https://www.sordum.org/8637/easy-service-optimizer-v1-2/) | Editable service lists/presets, apply/restore startup settings, portable operation and CLI. | Build/role-aware reviewed profiles, not universal Safe/Extreme promises or copied legacy defaults. F157–F168. |
| [ServiceCommander](https://www.coretechnologies.com/products/ServiceCommander/) | Tray control of selected local/remote services. | Favorites and quick local controls; remote capability separate. F169–F180, F205–F216. |
| [ServiceTray](https://www.coretechnologies.com/products/ServiceTray/) | Tray state/notifications, start/stop/restart, multiple services and process information. | One optional tray monitor with clear state/metrics and no requirement to run constantly. F169–F192. |
| [Service Trigger Editor](https://www.coretechnologies.com/products/ServiceTriggerEditor/) | View/add/edit service event triggers. | Trigger inspection and expert editing with full preservation. F121–F132. |
| [Service Security Editor](https://www.coretechnologies.com/products/ServiceSecurityEditor/) | GUI service permissions editing. | Inspect/evaluate/edit ACLs with minimal grants; do not automatically relax remote security exceptions. F133–F144. |
| [Service Scheduler](https://www.coretechnologies.com/products/ServiceScheduler/) | Scheduled start/stop/restart, task editing/running/removal. | Local scheduled operations with audit, DST/missed-run rules and managed helper lifecycle. F169–F180. |

### 3.2 Additional products checked with evidence limitations

- [HiBit Startup Manager](https://www.hibitsoft.ir/StartupManager.html): official page exposed product/version and installer/portable links, but detailed text retrieval repeatedly timed out. Keep it on the comparison shortlist; no undocumented feature parity claims are made.
- [Startup Delayer](https://startupdelayer.com/features/): the former R2 product URL redirected to a current product site/classic page. Avoid mixing classic and current edition claims. Fine-grained current parity was not established; advanced launch conditions below are proposed requirements.
- AnVir Startup Manager (`https://www.anvir.com/startup-manager.htm`) and Carifred Quick Service Manager (`https://www.carifred.com/quick_service_manager/`): direct official pages were inaccessible in this research. Do not manufacture detailed matrices from third-party download descriptions.
- [WinPatrolX](https://www.komossolutions.com/winpatrolx/index.html): official page describes monitoring startup/services/tasks/registry changes but states download is coming soon. Treat as announced, not an evaluated shipping competitor.

### 3.3 Deliberate exclusions

No cloud reputation, VirusTotal hash/file submissions, online advice downloads, telemetry, remote activation, forced updates, advertisements, RAM cleaners, indiscriminate registry cleaners, driver downloads, bootloader/BCD tweaking, rootkit removal, or automatic debloating. Signed/unsigned is evidence, not a malware verdict. LAN administration is an optional explicitly connected expert module, disabled in offline/local mode; it is not cloud management.

## 4. Feature catalog — 240 prioritized requirements

Existing evidence is in section 2; this catalog defines target behavior. Group membership does not make every item essential. Every operation needs source-specific implementation, permissions, errors, and tests.

### 4.1 Common shell and module integration

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F001 | P0 Essential | One common window | Switch Startup/Services pages without opening separate application windows. |
| F002 | P0 Essential | Shared navigation | Sidebar includes Dashboard, Startup, Services, History and Settings. |
| F003 | P0 Essential | Shared commands | Menus/toolbars/shortcuts invoke one validated command implementation. |
| F004 | P0 Essential | Single-instance activation | Relaunch brings existing shell forward and selects requested module. |
| F005 | P0 Essential | Common settings | One settings system replaces contradictory per-tool preferences. |
| F006 | P0 Essential | Per-module view memory | Preserve filters, columns, selection and scroll on page switches. |
| F007 | P0 Essential | Read-only initial access | Browse without mandatory administrator elevation. |
| F008 | P0 Essential | Unified activity center | Show background scans/actions, cancellation and detailed outcomes. |
| F009 | P0 Essential | Context-sensitive actions | Enable only actions supported by selection, rights and current state. |
| F010 | P1 Recommended | Cross-module application view | Link startup entries/tasks/services by verified path/package/publisher evidence. |
| F011 | P1 Recommended | Command palette | Find actions without memorizing toolbar locations. |
| F012 | P0 Essential | Persistent target indicator | Always show local/remote machine, account scope, and elevation state. |

### 4.2 Inventory, search, and list productivity

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F013 | P0 Essential | Complete scan accounting | Show provider completion, hidden counts, access failures and unsupported categories. |
| F014 | P0 Essential | Incremental search | Search names, descriptions, paths, publishers and source locations responsively. |
| F015 | P0 Essential | Structured filters | Combine state/type/scope/publisher/risk filters and display active restrictions. |
| F016 | P0 Essential | Sortable columns | Stable ascending/descending sorting with missing values handled explicitly. |
| F017 | P0 Essential | Configurable columns | Show/hide/reorder/resize with reset and per-module persistence. |
| F018 | P0 Essential | Multi-selection | Ctrl/Shift selection and correct select-all behavior within filtered results. |
| F019 | P0 Essential | Details pane/dialog | Inspect full values without relying on truncated grid cells. |
| F020 | P1 Recommended | Saved views | Save useful filters and column arrangements locally. |
| F021 | P1 Recommended | Tags and notes | Annotate entries without altering Windows configuration. |
| F022 | P0 Essential | Copy values/rows | Copy selected names, paths or full rows in clean Unicode format. |
| F023 | P0 Essential | Stable refresh | Preserve identity-based selection/focus/scroll and avoid flicker. |
| F024 | P1 Recommended | Clear uncertainty badges | Distinguish unknown/access denied/not measured/missing from disabled or safe. |

### 4.3 Startup discovery and Windows coverage

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F025 | P0 Essential | Current-user Run entries | Enumerate recurring sign-in startup with exact registry provenance. |
| F026 | P0 Essential | Machine Run entries | Include all-users entries and mark write permission requirements. |
| F027 | P0 Essential | RunOnce semantics | Show one-time entries separately and preserve flags/consumption behavior. |
| F028 | P0 Essential | Both registry views | Inspect applicable 32/64-bit views without alias duplication. |
| F029 | P0 Essential | User/common Startup folders | Resolve shortcuts, arguments and working directory without executing them. |
| F030 | P0 Essential | Scheduled autostarts | Enumerate boot/logon tasks, including nested folders and hidden-task permissions. |
| F031 | P1 Recommended | Packaged startup tasks | Identify supported packaged-app startup registration/state; disclose control limits. |
| F032 | P1 Recommended | Policy-managed startup | Identify policy/script-origin items and distinguish visibility from editability. |
| F033 | P1 Recommended | Other-user inspection | Explicitly selected loaded user profiles with SID/scope and access reporting. |
| F034 | P0 Essential | Source category and trigger | Explain boot vs sign-in vs Explorer/event activation accurately. |
| F035 | P0 Essential | Raw and resolved command | Preserve original command alongside parsed executable/script/module. |
| F036 | P0 Essential | Coverage summary | State which locations were scanned instead of claiming every persistence method. |

### 4.4 Startup changes and restoration

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F037 | P0 Essential | Enable/disable | Reversible provider-specific operations with read-back verification. |
| F038 | P0 Essential | Add startup entry | Separate executable/arguments/directory/scope and preview exact registration. |
| F039 | P1 Recommended | Edit startup entry | Correct names/commands using preserved original value types and metadata. |
| F040 | P0 Essential | Delete registration only | Confirm exact entry removal; never uninstall or delete its target executable. |
| F041 | P0 Essential | Restore deleted entry | Restore captured registry/task/shortcut data with conflict preview. |
| F042 | P0 Essential | Batch change plan | Show before/after, scope, skipped/protected items and per-item outcomes. |
| F043 | P0 Essential | Startup change undo | Revert supported configuration changes with explicit scope and conflict checks. |
| F044 | P1 Recommended | Redo supported edits | Reapply reverted edits only if current state still matches expectations. |
| F045 | P1 Recommended | Duplicate-entry review | Group equivalent targets while retaining differing arguments/triggers/users. |
| F046 | P0 Essential | Missing-target review | Distinguish genuinely missing files from offline drives or unresolved environment values. |
| F047 | P1 Recommended | Manual run/test | Explicitly launch selected trusted entry as the intended user, never during inspection. |
| F048 | P0 Essential | Source/file navigation | Open source details or file location without executing untrusted command strings. |

### 4.5 Delayed and conditional startup

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F049 | P1 Recommended | Fixed launch delay | Delay eligible sign-in apps with a clear managed-task migration preview. |
| F050 | P1 Recommended | Ordered launch queue | Reorder app starts without pretending registry Run ordering is guaranteed. |
| F051 | P2 Advanced | Idle-based launch | Configurable CPU/disk idle windows with maximum wait and fallback. |
| F052 | P2 Advanced | Launch dependencies | Wait for a process/service/readiness condition with cycle and timeout checks. |
| F053 | P1 Recommended | Skip/pause pending launch | Let users skip this session or pause the queue without losing settings. |
| F054 | P1 Recommended | Prevent duplicate launches | Session-aware checks avoid starting an already running eligible instance. |
| F055 | P1 Recommended | Bounded launch retry | Backoff/retry only when explicitly enabled; avoid restart storms. |
| F056 | P1 Recommended | Battery/AC conditions | Choose whether optional delayed apps wait for power changes. |
| F057 | P2 Advanced | Local connectivity condition | Observe interface availability, not ping/cloud probes; offline mode can ignore it. |
| F058 | P1 Recommended | Per-user queue isolation | Never launch user apps in SYSTEM/session 0 or another user's desktop. |
| F059 | P0 Essential | Restore original startup source | Removing a delay restores original registration without duplicates/lost arguments. |
| F060 | P0 Essential | Helper visibility/lifecycle | Clearly disclose installed tasks/helpers and remove or migrate them safely. |

### 4.6 Deep autostart and task inspection

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F061 | P1 Recommended | Explorer add-ons | Inspect context-menu, overlay and supported shell extensions by activation category. |
| F062 | P2 Advanced | Winlogon/BootExecute inspection | Read-only expert visibility into sensitive OS launch locations. |
| F063 | P2 Advanced | AppInit/IFEO inspection | Display relevant configuration and risk context without one-click modification. |
| F064 | P2 Advanced | WMI subscriptions | Read-only event-filter/consumer/binding inspection without running payloads. |
| F065 | P2 Advanced | LSA/security-provider inspection | Explain source/signature and keep protected components read-only. |
| F066 | P2 Advanced | Network/provider extensions | Inspect supported Winsock/network-provider entries with OS applicability labels. |
| F067 | P2 Advanced | Print/codec extensions | Inspect supported print monitors/codecs without implying logon execution. |
| F068 | P1 Recommended | Full task details | Show triggers/actions/principal/run level/conditions/settings and disabled state. |
| F069 | P1 Recommended | Task history/results | Display last/next run and decoded result codes with missing-log explanation. |
| F070 | P1 Recommended | Task XML backup/restore | Preserve full task definitions and ACL metadata where readable; disclose credential limits. |
| F071 | P1 Recommended | Shell-extension disable | Reversibly change supported noncritical extensions with restart/sign-out guidance. |
| F072 | P2 Advanced | Read-only external-system scan | Inspect selected offline hives with explicit paths, architecture and unsupported-provider reporting. |

### 4.7 Boot and sign-in diagnostics

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F073 | P1 Recommended | Boot/session history | Local timeline separates boot, sign-in and app-launch metrics. |
| F074 | P1 Recommended | Windows impact data | Show available OS measurements with source/timestamp, not fabricated scores. |
| F075 | P1 Recommended | Opt-in startup recorder | Explicit bounded local helper measures selected sessions with overhead disclosed. |
| F076 | P1 Recommended | Launch duration breakdown | Distinguish spawn time, readiness proxy and process lifetime. |
| F077 | P1 Recommended | Before/after comparison | Compare multiple comparable boots with reboot/Fast Startup distinctions. |
| F078 | P1 Recommended | Slowed-service events | Show relevant Diagnostics-Performance events with missing-log/admin caveats. |
| F079 | P2 Advanced | Local ETW tracing | Explicit capture to local files with bounded size and privacy warnings. |
| F080 | P1 Recommended | Impact unknown state | Show Not measured/Unavailable rather than zero impact. |
| F081 | P1 Recommended | Resource contention view | Correlate launch intervals with measured CPU/disk pressure, not causation claims. |
| F082 | P1 Recommended | Readiness threshold settings | Define what usable/sign-in complete means for repeatable comparisons. |
| F083 | P1 Recommended | Export measurement report | Local annotated CSV/JSON/HTML with measurement method and timestamps. |
| F084 | P0 Essential | No automatic reboot | Explain restart/sign-out requirements and leave execution to explicit user choice. |

### 4.8 Service inventory and details

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F085 | P0 Essential | SCM service inventory | Enumerate visible services and disclose rights-limited coverage. |
| F086 | P0 Essential | Exact service states | Running/stopped/pending/paused/resuming with text and color. |
| F087 | P0 Essential | Start configuration | Separate automatic/manual/disabled from delayed and trigger metadata. |
| F088 | P0 Essential | Stable service identity | Use service name plus target machine; display name is not the unique key. |
| F089 | P0 Essential | Process/account details | PID, account, own/shared process type and readable description. |
| F090 | P1 Recommended | Service host/module detail | Inspect hosted DLL separately from signed svchost.exe where applicable. |
| F091 | P0 Essential | Per-user services | Distinguish templates/instances and avoid unsafe suffix-based bulk edits. |
| F092 | P1 Recommended | Publisher/version/signature | Verify relevant local file evidence with clear trust-state labels. |
| F093 | P0 Essential | Critical/feature/third-party labels | Explain classification and uncertainty rather than promising safety. |
| F094 | P1 Recommended | Configuration inspector | View type/path/error control/group/tag and supported extended fields. |
| F095 | P1 Recommended | Service event history | Filter related local SCM events and explain disabled/missing logs. |
| F096 | P1 Recommended | Service notes/favorites | Pin important services and record device-specific reasons locally. |

### 4.9 Service actions and state transitions

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F097 | P0 Essential | Start | Request startup and verify resulting state rather than assuming request success. |
| F098 | P0 Essential | Stop | Check controls/dependents and confirm scope before graceful stop. |
| F099 | P0 Essential | Restart | Plan stop/start and restore previously running dependents where appropriate. |
| F100 | P0 Essential | Pause/resume | Offer only if the service accepts the relevant control. |
| F101 | P0 Essential | Pending-state progress | Show checkpoints/wait hints with bounded waits and useful errors. |
| F102 | P0 Essential | Cancel batch safely | Stop issuing future commands; never imply an accepted SCM request was undone. |
| F103 | P0 Essential | Batch results | Per-service success/failure/timeout/skipped states and retry selection. |
| F104 | P0 Essential | Disabled-service handling | Separately propose start-type change before start; never change silently. |
| F105 | P0 Essential | Protected-service respect | Report access/protection limitations without taking ownership or bypassing them. |
| F106 | P2 Advanced | Force-terminate expert action | Show all shared-process victims; block critical/protected/system shared hosts. |
| F107 | P1 Recommended | Temporary action intent | Distinguish stopping now from disabling future startup. |
| F108 | P0 Essential | Post-action verification | Requery current state and explain stale/disappeared service cases. |

### 4.10 Dependencies and configuration

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F109 | P0 Essential | Dependency tree | Show required services and reverse dependents recursively. |
| F110 | P0 Essential | Dependency impact plan | List affected services/features before stop/disable/restart. |
| F111 | P0 Essential | Ordered batch execution | Start prerequisites first and stop dependents first with cycle/error handling. |
| F112 | P0 Essential | Change start type | Use SCM APIs for automatic/manual/disabled and explicit verification. |
| F113 | P1 Recommended | Delayed automatic start | Configure supported delayed flag without conflating it with exact app delay seconds. |
| F114 | P1 Recommended | Name/description editing | Preserve stable service identity while updating readable fields. |
| F115 | P2 Advanced | Binary-path editing | Expert-only quoted executable/arguments, risk review and secured backup. |
| F116 | P2 Advanced | Dependency editing | Validate service/group dependencies and reject cycles before apply. |
| F117 | P1 Recommended | Feature impact explanations | Name affected printing/Bluetooth/network/authentication capabilities contextually. |
| F118 | P0 Essential | Configuration vs running state | Explain when changes apply only on next start/reboot. |
| F119 | P1 Recommended | Pending reboot/change indicators | Show only evidence-backed requirements, not generic reboot demands. |
| F120 | P2 Advanced | Service creation/deletion | Separate expert workflow for genuine service executables, deletion limits and recovery caveats. |

### 4.11 Triggers, recovery, and advanced service settings

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F121 | P1 Recommended | Trigger inspector | Show native start/stop trigger types, GUIDs and binary data. |
| F122 | P2 Advanced | Trigger editor | Add/edit/remove validated supported triggers with complete original backup. |
| F123 | P1 Recommended | Recovery inspector/editor | Configure first/second/subsequent failure actions and delays with preview. |
| F124 | P1 Recommended | Failure-reset policy | Expose reset interval and non-crash failure behavior accurately. |
| F125 | P2 Advanced | Recovery command/reboot actions | Expert opt-in with exact executable/action consequences and no hidden execution. |
| F126 | P1 Recommended | Recovery storm detection | Warn about rapid restart loops and offer bounded policies. |
| F127 | P2 Advanced | Required privileges/SID | Inspect and carefully edit supported settings with least-privilege rationale. |
| F128 | P2 Advanced | Preshutdown timeout | Show/change supported values with shutdown-delay warnings. |
| F129 | P2 Advanced | Protection/launch metadata | Read-only protection info; no protection-level downgrade controls. |
| F130 | P1 Recommended | Preserve unknown settings | Round-trip unsupported extended data unchanged or block unsafe rewrites. |
| F131 | P1 Recommended | Account editing | Validate intended built-in/custom identity and required rights without storing passwords. |
| F132 | P0 Essential | Session 0 explanation | Explain why services cannot provide normal interactive desktop UI. |

### 4.12 Safety, permissions, and local security inspection

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F133 | P0 Essential | Least-privilege broker | Request only the rights needed for a reviewed operation. |
| F134 | P0 Essential | Critical-service protection | Block destructive actions on critical/protected entries in normal workflows. |
| F135 | P0 Essential | Risk confirmation | List exact affected items; risky confirmations default to Cancel. |
| F136 | P1 Recommended | Service ACL inspector | Explain users/groups and relevant allowed/denied operations. |
| F137 | P2 Advanced | Service ACL editing | Use validated native security editor, backup SDDL and prevent broad unsafe grants. |
| F138 | P1 Recommended | Effective-access checks | Explain why an action is unavailable rather than disabling silently. |
| F139 | P1 Recommended | Unquoted path inspection | Evaluate actual executable ambiguity and writable candidate paths; avoid simplistic string alarms. |
| F140 | P1 Recommended | Writable target/ACL warnings | Report insecure executable/directory/service permissions as evidence, not malware proof. |
| F141 | P1 Recommended | Script/host chain inspection | Distinguish signed host publisher from trust in target script/DLL. |
| F142 | P1 Recommended | Offline signature verification | Embedded/catalog checks distinguish unsigned/tampered/untrusted/unknown freshness. |
| F143 | P0 Essential | No secret persistence | Passwords/credentials never enter profiles, logs, backups or command lines. |
| F144 | P0 Essential | Respect managed policies | Explain GPO/MDM/protection constraints; never silently bypass security ownership. |

### 4.13 Driver and specialist inventory

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F145 | P1 Recommended | Read-only driver list | Inspect kernel/filesystem drivers separately from ordinary services. |
| F146 | P1 Recommended | Boot/System labels | Explain driver-only startup types rather than offering them for Win32 services. |
| F147 | P1 Recommended | Loaded vs registered | Distinguish active drivers from installed configuration. |
| F148 | P1 Recommended | Driver paths/versions | Resolve native path forms and identify relevant image metadata. |
| F149 | P1 Recommended | Driver trust evidence | Local catalog/embedded signature state with unknown handling. |
| F150 | P1 Recommended | Driver missing-file caution | Avoid false alarms for optional/virtual/dynamically provisioned entries. |
| F151 | P0 Essential | No driver mutation in base | Do not expose stop/disable/delete/profile changes for boot-critical drivers. |
| F152 | P2 Advanced | Driver package correlation | Link image to installed device/package evidence without uninstalling it. |
| F153 | P2 Advanced | Load-order/group inspection | Read-only view of boot grouping and dependencies. |
| F154 | P1 Recommended | Driver snapshot/export | Save read-only diagnostic inventory with target/OS provenance. |
| F155 | P1 Recommended | Device Manager handoff | Open built-in diagnostics without pretending driver edits are safe service actions. |
| F156 | P0 Essential | Category-specific permissions | Disable irrelevant commands clearly when inspecting specialist categories. |

### 4.14 Backups, profiles, and rollback

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F157 | P0 Essential | Automatic pre-change backup | Durable original data before any supported mutation; abort if required backup fails. |
| F158 | P0 Essential | Unified change journal | Track target, source, before/after, time, actor, result and reversibility. |
| F159 | P0 Essential | Restore preview | Compare saved/current/proposed values and surface conflicts. |
| F160 | P1 Recommended | Full configuration snapshots | Capture readable startup/service settings with exclusions for inaccessible/secrets. |
| F161 | P1 Recommended | Custom profiles | Save selected desired settings and apply only through a reviewed plan. |
| F162 | P1 Recommended | Build/role-aware templates | Describe prerequisites and affected features; no universal Safe/Extreme guarantees. |
| F163 | P0 Essential | Cross-machine restore guard | Match OS/build/identity and default foreign profiles to preview-only. |
| F164 | P0 Essential | Partial rollback reporting | Distinguish reverted configuration from irreversible runtime side effects. |
| F165 | P1 Recommended | Snapshot comparison | Highlight new/removed/changed entries between captures. |
| F166 | P1 Recommended | Retention/backup export | Bounded history with user-controlled local export and deletion. |
| F167 | P1 Recommended | Recovery instructions | Export readable recovery steps; do not promise Windows will boot after all changes. |
| F168 | P2 Advanced | Optional restore-point request | Explain unavailable/throttled System Protection and never substitute it for item backups. |

### 4.15 Monitoring, tray controls, and schedules

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F169 | P1 Recommended | Opt-in background monitor | Observe changes only when enabled; no mandatory resident process. |
| F170 | P1 Recommended | Startup addition/change alerts | Show old/new source and user context with notification rate limits. |
| F171 | P1 Recommended | Service state alerts | Detect selected service failures/transitions using notifications where supported. |
| F172 | P1 Recommended | One tray dashboard | Favorites and status inside one optional tray icon rather than clutter. |
| F173 | P1 Recommended | Selected-service quick control | Reviewed start/stop/restart with same safety pipeline as main UI. |
| F174 | P1 Recommended | Local schedules | Schedule explicit service operations through managed Windows tasks. |
| F175 | P1 Recommended | Schedule reliability rules | Define DST/timezone, missed runs, battery, overlap and retry policy. |
| F176 | P2 Advanced | Health-check recovery | Opt-in bounded checks/actions for selected noncritical services; distinguish process alive from healthy. |
| F177 | P1 Recommended | Persistent-disable rules | Explicit opt-in alerts/enforcement for selected entries; never silently battle policy/security tools. |
| F178 | P1 Recommended | Snooze/maintenance window | Suppress expected alerts and automated recovery temporarily. |
| F179 | P0 Essential | Inspect own tasks/helpers | List all registrations created by the suite with disable/remove/migration policy. |
| F180 | P0 Essential | Safe monitor shutdown | Closing/uninstalling states whether monitoring continues and removes orphaned work. |

### 4.16 Metrics and troubleshooting

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F181 | P1 Recommended | Process CPU/memory | Label shared-host metrics as process-level, not per-service attribution. |
| F182 | P1 Recommended | Process/session uptime | Distinguish observed service transitions from process-start uptime estimates. |
| F183 | P1 Recommended | Shared-host list | Show every service associated with a PID before diagnostics/actions. |
| F184 | P1 Recommended | Resource history | Bounded local graphs with sampling interval and process identity checks. |
| F185 | P0 Essential | Friendly Win32 errors | Preserve error codes alongside useful access/state/path/timeout explanations. |
| F186 | P1 Recommended | Failure investigation view | Correlate service config/dependencies/events without automatic repairs. |
| F187 | P1 Recommended | Changed-binary detection | Invalidate file identity/hash/signature caches after replacement. |
| F188 | P0 Essential | No action on scan | Inventory/hash/signature operations never launch or load target payloads. |
| F189 | P1 Recommended | Local help links | Explain status, start types, triggers and recovery without internet. |
| F190 | P1 Recommended | Open Windows tools | Contextual Services/Event Viewer/Task Manager links with accurate target context. |
| F191 | P1 Recommended | Diagnostics package | User-initiated local redacted export, never an automatic upload. |
| F192 | P1 Recommended | Evidence-based suggestions | Explain why/how measured; no fabricated performance gain or guaranteed safety. |

### 4.17 Reports, import, and automation

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F193 | P0 Essential | CSV/JSON inventory export | Export selected/all/filter scope with schema and machine/scan provenance. |
| F194 | P1 Recommended | Offline HTML report | Self-contained readable report without external assets or active scripts. |
| F195 | P1 Recommended | Change/rollback reports | Preserve skipped/errors/conflicts so partial success is visible. |
| F196 | P1 Recommended | Safe profile import | Validate schema, bounds and targets; importing never applies or executes actions. |
| F197 | P1 Recommended | Formula-safe CSV | Offer neutralization for spreadsheet-leading formulas and proper Unicode escaping. |
| F198 | P2 Advanced | Read-only CLI | Inventory/diff/export with documented filters, JSON and exit codes. |
| F199 | P2 Advanced | Mutation CLI | Explicit target/plan/approval flags, rights checks, journal and no broad default action. |
| F200 | P1 Recommended | Schema migration | Version settings/profiles and preserve unknown or incompatible files safely. |
| F201 | P1 Recommended | Redacted export option | Remove usernames/paths/arguments/hostnames as selected with preview. |
| F202 | P2 Advanced | Baseline drift report | Compare desired/actual locally without silently enforcing a baseline. |
| F203 | P0 Essential | Atomic report/config writes | Avoid truncated backups/settings on crash or full disk. |
| F204 | P1 Recommended | Repeatable scan definitions | Save provider/filter choices for diagnostic consistency. |

### 4.18 Local privacy, optional LAN, and storage

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F205 | P0 Essential | Fully offline local mode | No network attempts from shell, helpers, advice, trust checks or installer. |
| F206 | P0 Essential | No telemetry | No analytics/crash uploads/ads/accounts/activation or silent updates. |
| F207 | P0 Essential | Bundled local advice | Versioned service explanations with source/date/OS applicability and uncertainty. |
| F208 | P0 Essential | Protected user storage | Restrict settings/history/backups appropriately and bound retention. |
| F209 | P1 Recommended | Clear privacy data | Preview/remove app-managed history/caches without altering Windows startup/services. |
| F210 | P2 Advanced | Explicit LAN connection | Optional local-network administration mode, separately enabled and unavailable in strict offline mode. |
| F211 | P2 Advanced | Target-bound remote operations | Strong machine identity/banner and per-target histories prevent wrong-host changes. |
| F212 | P2 Advanced | Secure remote authentication | Prefer Windows-integrated authentication; do not save passwords or relax firewall/ACLs. |
| F213 | P2 Advanced | Remote capability matrix | Mark unavailable process/file/signature/boot data; never substitute local data. |
| F214 | P0 Essential | Untrusted import hardening | Bound files/XML/JSON, disable external entities and reject executable directives. |
| F215 | P0 Essential | Secure local IPC | Authenticate session/user and accept only bounded allowlisted broker commands. |
| F216 | P0 Essential | Secret-free diagnostics | Redact credentials/tokens/sensitive arguments; no raw dumps by default. |

### 4.19 Themes, accessibility, and interaction

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F217 | P0 Essential | Complete light/dark themes | Include details/settings/review/error dialogs, menus and custom controls. |
| F218 | P1 Recommended | Additional themes | Midnight and Warm palettes with system-follow option and consistent tokens. |
| F219 | P1 Recommended | Colorful consistent icons | Clear vectors with labels/tooltips and high-contrast variants. |
| F220 | P0 Essential | Accessible toolbar semantics | UI Automation names/roles/states/invoke and keyboard focus for custom actions. |
| F221 | P0 Essential | Keyboard-first operation | Logical tab order, shortcuts, context menu key and reliable Escape. |
| F222 | P0 Essential | High contrast/non-color cues | Text/icons supplement risk/status colors and honor Windows contrast themes. |
| F223 | P0 Essential | Scalable text/DPI | No clipping at 200% text size or mixed-monitor scaling. |
| F224 | P0 Essential | Reliable menu interaction | No lost/double clicks, stale capture, unexpected focus or invisible blockers. |
| F225 | P1 Recommended | Adjustable density/font | Comfortable or compact lists without truncating essential actions. |
| F226 | P1 Recommended | Localizable UI | Unicode, resource-based strings and expandable layouts; RTL when translated. |
| F227 | P1 Recommended | Reduced motion | Respect OS preference and avoid distracting refresh animations. |
| F228 | P0 Essential | Clear empty/error states | Distinguish no results, scanning, provider denied, no items and failed refresh. |

### 4.20 Installation, migration, and performance

| ID | Priority | Feature | Purpose / practical benefit |
|---|---|---|---|
| F229 | P0 Essential | One offline setup file | Install both modules and required helpers with no prerequisite downloads. |
| F230 | P0 Essential | Program Files default | Architecture-appropriate protected application directory; user data stored separately. |
| F231 | P0 Essential | Simple install/launch | One primary Install action, legitimate UAC, one Start menu shortcut, Finish opens suite. |
| F232 | P0 Essential | Portable-data migration | Preview old settings/profiles/backups; copy/import without deleting originals. |
| F233 | P0 Essential | Unified upgrades/rollback | Handle locked helpers/tasks and recover cleanly from failed upgrades. |
| F234 | P0 Essential | Clean uninstall | Remove owned files/tasks/helpers; explicitly preserve/export useful rollback records. |
| F235 | P1 Recommended | Portable suite build | Same combined UI with explicit storage policy and no hidden autostart installation. |
| F236 | P0 Essential | Version/credits/build date | About shows Developed by Akshaya Simha plus actual build metadata and licenses. |
| F237 | P0 Essential | Responsive workers | Inventory, signatures and changes run off the UI thread with bounded work queues. |
| F238 | P0 Essential | Low idle overhead | Event-driven monitoring where possible, adaptive bounded polling otherwise. |
| F239 | P0 Essential | Release provenance | Checksums, source commit, dependency notices/SBOM and signatures when available. |
| F240 | P1 Recommended | Local maintenance packages | Manual verified offline upgrades; no forced network updater. |

## 5. Integration architecture for these existing apps

### 5.1 Preserve native strengths

The binaries are about 0.5 MB each and have no CLR directory. The Services README identifies C++/CMake, worker, service list, inspection, profiles, dialogs, theme and toolbar modules. This supports evaluating a native shared shell first; it does not prove implementation details without source. Reuse business logic after tests rather than rewrite by default.

Suggested source layout:

```text
src/AppShell/           Main window, navigation, commands, themes, UI Automation
src/Startup/            Provider enumeration, parsing, edit/restore adapters
src/Services/           SCM inventory, controls, dependencies, config/recovery
src/Inspection/         Files, hashes, signatures, path resolution, process evidence
src/Changes/            Plans, backups, journals, conflict handling, verification
src/Monitoring/        Opt-in watchers, local metrics, managed tasks
src/PrivilegeBroker/   Narrow elevated command handler
src/Storage/           Versioned settings, profiles, migrations, retention
data/                  Local advice/rules, translations, icons, help
installer/             Offline setup, upgrade, uninstall, owned task management
tests/                 Unit, provider fixtures, VM integration, UI/accessibility
```

One UI executable can load module classes/static libraries. An on-demand elevated broker and optional session monitor are acceptable internal processes. Do not use SetParent to embed the old applications as the final integration: independent message loops, focus, DPI, elevation and lifecycles make that fragile and it does not create shared state. If only EXEs are available, a launcher can be a clearly labeled temporary bridge, never the completed common-window deliverable. Obtain/build source or reimplement specified engines; do not claim binary wrapping equals source integration.

### 5.2 Privilege boundary

Keep the shell unelevated. Use current-user rights for ordinary reads and eligible HKCU changes; request elevation only for an explicit machine-level operation. Normal users must be able to inspect whatever Windows permits. A broker is on-demand, authenticated and short-lived; no always-running SYSTEM service is required for the core.

Use a per-user/session secured named pipe or equivalent IPC with versioned bounded messages, peer identity validation, nonce/request ID, replay protection and narrow operation schemas. The elevated endpoint independently validates target/source identity, current state, risk policy and arguments; never accept arbitrary command lines, registry paths, code or scripts as privileged requests. Keep app binaries in protected Program Files. Do not let untrusted portable files or writable task configs become elevated launch instructions. UAC cancellation must leave no partial operation.

### 5.3 Native API strategy

| Domain | Preferred Windows interfaces | Engineering rules |
|---|---|---|
| Services | OpenSCManager/OpenService, EnumServicesStatusEx, QueryServiceStatusEx, QueryServiceConfig/2, ChangeServiceConfig/2, StartService, ControlService | Minimal rights, RAII handles, Unicode, error codes, checkpoints/wait hints and final-state checks. |
| Dependencies | EnumDependentServices and queried dependency/group configuration | Graph ordering, reverse impact, cycle/unknown detection and refresh before apply. |
| Service notifications | NotifyServiceStatusChange for supported service applications | Follow threading/APC/re-registration/lifetime rules; drivers need another strategy. |
| Service security | Query/SetServiceObjectSecurity and standard security UI | Separate read vs write and explicit backed-up permission changes. |
| Registry startup | Registry APIs with explicit registry view and correct user hive | Preserve original types/bytes; do not infer all state from presence alone. |
| Startup folders | Known-folder APIs and shell-link inspection | Resolve without execution; preserve shortcut metadata and alternate user scope. |
| Tasks | Task Scheduler 2.0 COM API | Full task paths/XML/principals, conditions and permissions; no filesystem task-file hacks. |
| Files/trust | Version resources, hashing, WinVerifyTrust/catalog APIs | Offline/cache-only trust policy; do not load inspected executable/DLL into the shell. |
| Events/metrics | Windows Event Log and optional ETW/process APIs | Bounded queries, provenance, optional trace consent, shared-process attribution limits. |
| Accessibility | Native UI Automation providers/common controls | Custom drawn toolbars require providers and keyboard navigation. |

Primary API references are in section 12. Do not parse localized sc.exe, schtasks.exe or PowerShell table output as the main implementation. If a tool is used for a documented fallback, invoke via argument arrays, capture exit codes, and never concatenate untrusted command strings.

### 5.4 Stable entities and operation model

Startup identity includes target machine, provider, user SID, registry view, exact source location and value/task/shortcut identity. Same executable does not mean same entry: arguments, user, working directory and trigger matter. Service identity is machine + immutable service name; changing display name must not break history. Process identity includes creation time to prevent PID-reuse mistakes.

```text
Enumerate(scope, providers, cancel) -> Items + CoverageReport + ScanRevision
Inspect(itemId, revision) -> Properties + Evidence + Capabilities
Plan(changes, expectedRevision) -> OrderedImpactPlan + Risk + BackupRequirements
Capture(plan) -> DurableBackup + Limitations
Apply(planId, expectedState, authorization) -> PerItemResults
Verify(results) -> ActualState + Deviations
Restore(backupId, selectedFields) -> ConflictAwareNewPlan
```

Async scan replies carry generation IDs; discard stale results. UI virtualizes large lists. One command queue serializes conflicting writes, while reads and file inspections use bounded concurrency. Cancel means cancel future work; Windows cannot always recall an already accepted control request.

### 5.5 Startup state and command parsing

Source-specific enable/disable must coexist with Windows Settings/Task Manager. StartupApproved-like internal formats are not a universal supported API: isolate any compatibility adapter, test by OS build and fail read-only when unknown. Never blindly overwrite internal state blobs. RunOnce is transient installation behavior; do not recreate consumed entries during refresh/restore without clear review.

Keep original command untouched for restoration. Parse executable, arguments and working directory using Windows/source-specific rules, not splitting on first space. Handle quoted/unquoted paths, environment variables, UNC/native/SystemRoot forms, rundll32/svchost/script hosts, paths containing #, Unicode, missing/offline drives and multiple actions. Inspect the actual payload as well as host metadata. Never execute a command to discover its target.

For delay conversion, first capture the complete original registration, create a secured correctly scoped managed task, verify it, then disable the original; compensate if any step fails. Use a durable migration journal so a crash cannot orphan or double-run entries. Preserve user identity and session behavior. Administrative launch requires explicit authorization at configuration time, never an undocumented elevation bypass. Exact service delay seconds are not equivalent to Windows delayed-auto-start semantics.

## 6. Safety and recovery semantics

Service/startup tools can disrupt networking, sign-in, security, hardware, CAD dependencies, printing or installed software. A static critical-service list is useful but incomplete. Combine Windows access/protection, dependency evidence, local role/device context and reviewed rules. Unknown means unknown; unsigned/third-party/manual/stopped does not mean unnecessary or malicious.

### 6.1 Change transaction

1. Re-read identities/current values; reject stale plans or externally changed targets.
2. Build ordered dependency/impact plan with exact old/new fields, running dependents, rights, restarts and reversibility.
3. Persist protected backup and journal before mutation. If capture is incomplete, explain which operations cannot be safely restored and block normal bulk apply.
4. Apply individual changes through supported APIs. Journal each outcome immediately. Windows operations are not one atomic transaction; do not label them atomic.
5. Requery actual configuration and runtime state. Success requires matching intended postconditions, not merely no exception.
6. Offer compensating rollback for supported fields; verify again. Preserve partial-failure details and never overwrite a later external change without review.

Do not promise that undo restarts preserve unsaved application data or network sessions. Custom account passwords cannot be read back or captured for rollback. Account changes and some deleted service/task definitions cannot be perfectly restored without fresh credentials/installer data; mark these limited or non-reversible. Do not automatically grant Log on as a service or change service ACLs as an invisible side effect. Show a separate permission-change plan where required.

Force termination is P2, not a normal response to timeout. Recheck PID + creation time and shared victims immediately before action. Do not kill services.exe, critical/protected processes or shared system hosts. Drivers remain read-only. Never take TrustedInstaller ownership, disable protection or weaken security automatically to make a button succeed.

### 6.2 Profiles and advice

Replace downloaded blanket verdicts with local versioned guidance. Each rule includes service/source ID, OS build/edition range, applicable role/device features, explanation, evidence source/date, risk/uncertainty, proposed change and restore method. Validate imported rules; treat them as data, never executable instructions. A label such as No printer must disclose impact on print-to-PDF and related features. Do not import old optimizer defaults as universal current Windows truth.

Default profiles are plans with all changes visible, not automatic optimizers. Exclude security, boot/sign-in, networking and remote-access dependencies from broad presets. Managed PCs may have additional dependencies not visible locally. Never claim a service is safe solely because others disable it.

### 6.3 Storage and privacy

Installed binaries: Program Files. User preferences and notes: per-user LocalAppData. Machine-level broker journals/tasks: protected ProgramData if required, with explicit ACL ownership. Never execute elevated instructions read from ordinary user-writable profile files without fresh broker validation. A portable-mode marker can select a writable adjacent Data folder, but secrets still cannot be stored and privileged helpers cannot trust that folder.

Do not log passwords, authentication tokens or full sensitive command arguments. Profile import is preview-only and schema-limited. Validate XML without external entities; escape report HTML/CSV; reject traversal and oversized/cyclic structures. Clear controls affect app-managed data, not underlying Windows registrations. Forensic erasure on SSDs/backups cannot be guaranteed.

Offline mode makes zero network attempts, including trust-chain/revocation/AIA retrieval, advice, help, updates and thumbnails/icons. Unknown offline certificate freshness is not invalid or malware. Existing VirusTotal/search buttons should be omitted from strict offline builds or isolated as explicitly disabled external integrations; no silent hash/file transmission. LAN mode, if retained, is separately enabled with a persistent target banner and no cloud services. Never autochange firewall rules, remote-access exemptions, credential policy or DACLs to connect.

## 7. Installer, migration, and packaging contract

One distributable setup file installs the combined shell and owned internal helpers. Default to `%ProgramFiles%\Windows Startup & Services Manager` with one Start menu entry; final branding may change the folder. A single primary Install action accepts documented defaults; optional advanced destination/shortcut settings are available. UAC and OS security prompts remain legitimate. Do not force autostart monitoring, desktop shortcuts, admin-at-launch or online features by default. Finish opens the common Dashboard, not both old windows.

Inspect existing settings before migration. The Services README identifies `HKCU\Software\WindowsServicesManager`, `%LOCALAPPDATA%\WindowsServicesManager\Profiles` and `...\Snapshots`; verify actual schemas/content. Startup settings/storage locations require source confirmation. Import by copying after preview, version schemas, preserve originals, and retain standalone rollback compatibility where possible. Never delete either portable directory automatically. Record migration version so subsequent launches do not repeatedly overwrite new preferences.

Upgrade must coordinate running shell/broker/monitor and owned scheduled tasks, preserve backups, roll back failed binaries/config changes and avoid mixed helper versions. Uninstall lists its owned tasks/registrations. Delayed app registrations must be restored or clearly handed back after review; uninstall must not silently strand the user's startup apps. Offer export/retention of rollback history before deleting user data. Never uninstall unrelated apps or remove registrations that merely share a display name.

About/help show `Developed by Akshaya Simha`, version, source commit when available and actual build date/time. Preserve existing owner copyright. Code-sign when a real certificate is available; otherwise disclose unsigned status without pretending SmartScreen reputation can be guaranteed. Provide checksums and self-contained offline prerequisites. A portable combined build is optional P1 alongside, not instead of, the requested installer.

## 8. Dependencies and licensing decisions

Audit source first. Preferred core: C++/Win32, CMake and Windows SDK; existing native controls/worker modules are strong reuse candidates. Prefer OS APIs for services/registry/tasks/security/events rather than bundling other competitors' binaries. Common controls/DirectWrite/Direct2D may improve UI while maintaining native size. Any third-party UI/JSON/SQLite library is optional and must justify maintenance, footprint and licensing.

Do not redistribute Autoruns, NirSoft, Glary or other freeware tools without their exact redistribution permission; free download is not an open-source license. System Informer repository declares MIT, but any reused file/subcomponent needs its own notice review; do not add its kernel driver merely for parity. Do not copy proprietary risk/advice databases or trademarks. Record original local explanations with cited public technical evidence. Installer tool choice (WiX/Inno Setup/NSIS or another) requires current exact tool/output licensing and commercial-term review before selection. No paid SDK is currently needed by this plan.

Pin compiler/tool/library versions, save lockfiles/checksums and ship notices/SBOM. Keep signing secrets outside source control. Reproducible unsigned binaries and timestamped signed packages are tracked separately. Production runtime does not access package registries or update services.

## 9. Performance budgets

These are proposed targets, not observed benchmarks. Reference test machine: Windows 11 x64, 4 physical cores, 8 GB RAM, SSD, 1080p at 100/150% scaling. Record actual CPU/OS/build, item counts, signatures-cache state, release build and security scanning. Include helper processes in resource measurements.

| Metric | Target / method |
|---|---|
| Shell first interaction | p95 ≤ 500 ms warm, ≤ 1.5 s cold; inventory continues asynchronously. |
| Basic service inventory | p95 ≤ 2 s for 500 visible services excluding slow optional metadata. |
| Basic startup inventory | p95 ≤ 3 s for 1,000 mixed records; deep/WMI/offline scans have separate progress/budgets. |
| Search/filter response | p95 ≤ 100 ms for 10,000 cached rows; no whole rescan per keystroke. |
| Visible action feedback | p95 ≤ 100 ms; actual SCM completion depends on service. |
| Idle CPU | Average < 0.2% total-machine CPU over 60 s with monitoring off; explain baseline. |
| Memory | Initial shell/basic inventories target ≤ 80 MB aggregate private bytes; expanded tracing/cache budgets separately bounded. |
| Monitoring CPU | Target < 0.5% average under static selected monitoring; record service/task scope. |
| Package size | Aim core installer ≤ 20 MB and installed core ≤ 50 MB; exceptions need measured justification. |
| Cancellation | UI acknowledges ≤ 100 ms; stop queued work promptly; disclose already accepted OS operations. |
| Long-session stability | No growing handles or private bytes across 500 refreshes/100 page switches. |

Use virtual lists, incremental provider results, cached immutable file evidence keyed to identity/version, bounded background workers and debounced events. Do not hash every binary every 5 seconds. Service notifications are suitable for supported services, not all driver changes. Poll only with explicit limits and adaptive intervals. Do not block list interaction while slow network paths, signatures or event logs time out. In strict local mode do not resolve/open network target paths during inspection.

## 10. Acceptance tests and release gates

Run mutation tests in disposable Windows VMs with snapshots, standard/admin accounts and synthetic startup entries/test services. Include long/Unicode/# paths, both registry views, RunOnce flags, shortcuts, packaged-task limits, nested tasks, multiple task actions, missing/offline drives, hosted scripts/DLLs, shared processes, pending states, dependencies, per-user instances and malformed imports. No testing by disabling arbitrary real services on the owner's computer.

| Test | Scope | Pass criteria |
|---|---|---|
| AT01 Common window | F001–F012 | One installed shortcut/window switches modules and preserves state; no old standalone windows appear. |
| AT02 Baseline parity | Section 2 | Verified existing behaviors survive source integration; documented-only items receive tests or explicit limitations. |
| AT03 Inventory coverage | F013/F025–F036 | Match synthetic provider fixtures; denied/hidden/unsupported categories visible; filtered/total counts accurate. |
| AT04 Windows coexistence | F037/F087 | Enable/disable agrees with supported Windows UI on tested builds; unknown internal formats fail read-only. |
| AT05 Command parsing | F035/F139/F141 | Quoted/Unicode/#/environment/script-host/native/UNC fixtures resolve safely; no inspected target runs or loads. |
| AT06 Startup round-trip | F037–F044 | Enable/edit/delete/restore preserves exact original data/shortcut/task semantics; external conflicts surfaced. |
| AT07 RunOnce | F027 | Refresh never recreates consumed entries; flags/data survive deliberate backup/restore preview. |
| AT08 Delay conversion | F049–F060 | Correct user/session, no duplicate launches, original source preserved; injected crash at each stage recovers safely. |
| AT09 Dependency changes | F097–F118 | Topological ordering and reverse impacts correct; cycle/missing/access errors leave explicit results. |
| AT10 Pending/timeout | F101–F108 | Checkpoints/wait hints respected; UI responsive; timeout/cancel never falsely claims state reversal. |
| AT11 Protection | F105/F133–F144 | Critical/protected/managed entries reject unsafe changes; no ownership/security downgrade. |
| AT12 Shared hosts | F090/F106/F181–F183 | Metrics labeled process-wide; PID reuse rechecked; force action blocked on prohibited hosts. |
| AT13 Forms of service config | F112–F131 | Delayed/trigger/recovery/unknown fields round-trip as supported; failed writes detected by read-back. |
| AT14 Credentials | F131/F143/F216 | Account workflow never writes secrets to disk/logs/arguments; irreversible rollback gaps clearly shown. |
| AT15 Drivers | F145–F156 | Read-only commands enforced, accurate Boot/System labels, optional absent drivers not blanket false alarms. |
| AT16 Journal/rollback | F157–F168 | Crash/full-disk/failure injection preserves durable pre-state and partial results; no overwrite of later external edits. |
| AT17 Profiles | F161–F163 | Foreign OS/build/scope imports default preview-only; missing entries skip explicitly; no universal safety claims. |
| AT18 Monitoring/schedules | F169–F180 | Opt-in only, rate-limited notifications, bounded retries, DST/missed/overlap behavior and no orphan helpers. |
| AT19 Reporting/import | F193–F204/F214 | Unicode/CSV formulas/HTML escaping/XML external-entity/oversized/path-traversal fixtures handled safely; imports never apply automatically. |
| AT20 IPC/elevation | F133/F215 | Standard-user spoofing/replay/arbitrary targets rejected; canceled UAC leaves no changes; broker rights minimal. |
| AT21 Offline | F205–F207 | Deny outbound and trace shell/helpers/installer through scans/details/trust/help/mutations; zero connection attempts in local offline mode. |
| AT22 Optional LAN | F210–F213 | Local/remote never confused; no local metrics substituted; connection loss produces known/unknown outcomes and no auto security relaxation. |
| AT23 Themes/accessibility | F217–F228 | Full dialogs/menus themed; Narrator/keyboard operate custom toolbars; no clipping at 200% text/mixed DPI; non-color state cues. |
| AT24 Menu reliability | F224 | 100 mouse/keyboard menu actions per module execute once; no lost commands or stale focus/capture. |
| AT25 Installer/migration | F229–F236 | Clean offline VM installs one suite; imports old data without deletion; Finish opens common window; build/credits correct. |
| AT26 Upgrade/uninstall | F233/F234/F179 | Interrupted upgrade recovers; uninstall handles owned tasks and delayed app restoration without damaging unrelated registrations. |
| AT27 Portable mode | F235/F208 | Storage policy explicit, no hidden installed tasks/autostart, no secrets beside EXE; common UI matches installed build. |
| AT28 Performance | F237/F238 | Publish p50/p95 and aggregate memory/CPU/package results against section 9; disclose missed budgets. |
| AT29 Source/licensing | F239 | Pinned build, source provenance, hashes, notices/SBOM and actual signing state included. |
| AT30 Long-session reliability | F023/F237/F238 | 500 refreshes/100 switches show stable selection, no unbounded handles/memory, no cross-target histories. |

Release requires all P0 IDs tested and critical defects resolved: wrong-target mutation, boot/security disruption from normal workflows, secret leakage, unrequested traffic, lost rollback data, corrupted settings, privilege escalation or stranded startup registrations. P1/P2 functions not ready are absent or clearly unavailable, never fake buttons. A successful UI action is not enough: verify actual registry/task/SCM state independently. Source/API tests, UI tests and VM mutation tests answer different questions.

QA report: source commit/build, OS editions/builds, target architecture, rights, corpus IDs, expected/actual results, network trace method, benchmarks, failures/waivers and reproducible steps. Do not claim exhaustive Windows coverage from one machine.

## 11. Phased implementation roadmap

| Phase | Work | Exit evidence |
|---|---|---|
| 0 Audit | Find source, build old tools, establish feature/evidence matrix, source licenses and VM fixtures. | Both builds reproducible; no unsupported existing-feature claims; recorded architecture decision. |
| 1 Shared shell | Integrate module views/engines, settings/search/history/theme/commands and accessible toolbars. | AT01/02/23/24; truly common window, native footprint retained. |
| 2 Safe core | Provider-aware startup edits, SCM actions/dependencies, broker, backup/journal/restore and offline advice. | AT03–07/09–16/20/21 pass for shipped scope. |
| 3 Installed P0 release | One setup, migration/upgrade/uninstall/help/credit/build, performance and complete P0 coverage. | AT25/26/28–30 and every P0 ID tested; old portable folders preserved. |
| 4 Professional workflows | Delays, monitoring, boot metrics, richer profiles, reviews/reports, advanced recovery inspection and driver inventory. | Relevant P1 tests; all promised P1 behaviors verified, local mode still makes no requests. |
| 5 Advanced expert modules | Deep/offline scan, trigger/ACL/creation/deletion experts, ETW, CLI and optional LAN. | Separate capability/permission/safety tests and explicit scope; no silent security bypass. |

Do not prioritize broad specialist persistence editing over reliable rollback and integration. Each phase ends with a usable build and updated requirement-to-test matrix. Estimates follow source inspection; the current executable audit cannot establish integration effort.

## 12. Primary Windows engineering references

These developer links are references, not runtime network dependencies. Product/vendor research sources are linked in section 3.

- [Service configuration](https://learn.microsoft.com/en-us/windows/win32/services/service-configuration): supported query/change interfaces and extended configuration.
- [Service security/access rights](https://learn.microsoft.com/en-us/windows/win32/services/service-security-and-access-rights): SCM/service rights and privilege implications. Enumeration may omit inaccessible services, so do not claim complete visibility without qualification.
- [NotifyServiceStatusChangeW](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-notifyservicestatuschangew): service notifications, calling/lifetime requirements and driver limitation.
- [Windows service API header reference](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/): status, triggers, failure actions and extended settings.
- [Per-user services](https://learn.microsoft.com/en-us/windows/application-management/per-user-services-in-windows): template/instance behavior.
- [Run/RunOnce keys](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys): user/machine semantics, one-time flags and non-guaranteed timing/order.
- [Task Scheduler developer documentation](https://learn.microsoft.com/en-us/windows/win32/taskschd/task-scheduler-start-page): supported tasks/triggers and COM APIs.

## 13. Final handoff checklist

- One combined-window application and one offline setup file; portable suite optional.
- Existing-feature evidence matrix and all 240 IDs mapped to status/tests.
- Verified source/build, architecture/privilege decisions and complete owner attribution.
- Source-aware backups, durable journal, honest restoration limits and migration guide.
- Local advice/help; explicit policy for old online/remote features.
- Clean-VM installation/upgrade/uninstall and safety/accessibility/performance reports.
- Dependency licenses/notices/SBOM, checksums and known limitations.

**Implementation principle:** Give the user clear evidence and reversible configuration choices. Never trade Windows reliability, privilege boundaries or accurate reporting for a larger feature count.
