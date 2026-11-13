#!/usr/bin/env python3
"""Feather PDF runs on Windows 7 and later: no program file may import a function that Windows 7 SP1 does not have
(Windows would refuse to start it, "The procedure entry point ... could not be located"). Functions of newer Windows
must be looked up while running (GetProcAddress) with a way round when missing.

Usage: tests/win7check.py file.exe|file.dll ...   (exit 1 when one imports a function of Windows 8 or later)"""
import re, subprocess, sys

# Functions added after Windows 7 in the libraries Feather PDF and its parts use (from Microsoft's documentation,
# "Minimum supported client"). Not complete, but it holds the ones a Windows program is likely to reach for.
NEWER = {
    "bcrypt.dll": "BCryptHash BCryptKeyDerivation BCryptCreateMultiHash BCryptProcessMultiOperations",
    "kernel32.dll": """GetSystemTimePreciseAsFileTime CreateFile2 GetCurrentThreadStackLimits SetThreadDescription GetThreadDescription
        SetDefaultDllDirectories AddDllDirectory RemoveDllDirectory WaitOnAddress WakeByAddressSingle WakeByAddressAll
        InitializeSynchronizationBarrier EnterSynchronizationBarrier DeleteSynchronizationBarrier PrefetchVirtualMemory
        VirtualAlloc2 MapViewOfFile3 GetPackageFamilyName GetCurrentPackageFullName GetCurrentPackageId CreateRemoteThreadEx2
        SetProcessMitigationPolicy GetProcessMitigationPolicy SetThreadInformation GetThreadInformation SetProcessInformation
        GetProcessInformation QueryUnbiasedInterruptTimePrecise GetOsSafeBootMode CompareObjectHandles DiscardVirtualMemory
        OfferVirtualMemory ReclaimVirtualMemory GetFirmwareType SetFileInformationByHandleEx CopyFile2 CreateFileMapping2
        GetSystemCpuSetInformation SetThreadSelectedCpuSets GetIntegratedDisplaySize""",
    "user32.dll": """GetDpiForWindow GetDpiForSystem SetThreadDpiAwarenessContext GetThreadDpiAwarenessContext SetProcessDpiAwarenessContext
        AdjustWindowRectExForDpi GetSystemMetricsForDpi SystemParametersInfoForDpi EnableNonClientDpiScaling GetWindowDpiAwarenessContext
        AreDpiAwarenessContextsEqual GetAwarenessFromDpiAwarenessContext IsValidDpiAwarenessContext GetDpiFromDpiAwarenessContext
        LogicalToPhysicalPointForPerMonitorDPI PhysicalToLogicalPointForPerMonitorDPI GetPointerType GetPointerInfo GetPointerPenInfo
        GetPointerTouchInfo GetPointerFrameInfo EnableMouseInPointer IsMouseInPointerEnabled RegisterPointerInputTarget
        InitializeTouchInjection InjectTouchInput GetDisplayAutoRotationPreferences SetDisplayAutoRotationPreferences
        GetCurrentInputMessageSource GetCIMSSM SetCoalescableTimer IsImmersiveProcess GetDpiForShellUIComponent
        OpenThreadDesktop SetDialogDpiChangeBehavior SetDialogControlDpiChangeBehavior GetSystemDpiForProcess""",
    "gdi32.dll": "GetFontFileData GetFontFileInfo",
    "shell32.dll": "SHGetPropertyStoreForWindow2 GetCurrentProcessExplicitAppUserModelID2",
    "shcore.dll": "*",                    # the whole library is Windows 8.1 and later
    "api-ms-win-core-path-l1-1-0.dll": "*",
    "combase.dll": "*",
    "dwmapi.dll": "DwmGetUnmetTabRequirements DwmSetIconicLivePreviewBitmap2",
    "uxtheme.dll": "",
    "advapi32.dll": "",
    "winhttp.dll": "WinHttpWebSocketCompleteUpgrade WinHttpWebSocketSend WinHttpWebSocketReceive WinHttpWebSocketClose",
}
NEWER = {k: set(v.split()) for k, v in NEWER.items()}

def imports(path):
    out = subprocess.run(["x86_64-w64-mingw32-objdump", "-p", path], capture_output=True, text=True).stdout
    dll, found = None, []
    for line in out.splitlines():
        m = re.match(r"\s*DLL Name: (\S+)", line)
        if m: dll = m.group(1).lower(); continue
        m = re.match(r"\t[0-9a-f]+\t +\d+  (\S+)", line)
        if m and dll: found.append((dll, m.group(1)))
    return found

bad = 0
for path in sys.argv[1:]:
    imp = imports(path)
    newer = [(d, f) for d, f in imp if d in NEWER and ("*" in NEWER[d] or f in NEWER[d])]
    newer += [(d, "(the library)") for d in sorted({d for d, _ in imp}) if d.startswith("api-ms-win-") and d not in NEWER]
    for d, f in newer: print(f"  {path}: {d} {f} is not in Windows 7")
    print(f"{path}: {len(imp)} functions imported, {len(newer)} newer than Windows 7")
    bad += len(newer)
sys.exit(1 if bad else 0)
