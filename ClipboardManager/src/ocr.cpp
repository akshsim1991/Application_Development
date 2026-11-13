// Reading the text in pictures with the OCR engine that is part of Windows 10/11 (Windows.Media.Ocr).
// It is called through the Windows PowerShell that ships with Windows, in a hidden background process, so no
// extra software is installed and nothing leaves the PC. If Windows has no OCR language the picture is simply
// kept without text.
#include "app.h"
#include <process.h>

static const char* OCR_SCRIPT = R"PS(
param([string]$Image,[string]$Out)
$ErrorActionPreference = 'Stop'
try {
  Add-Type -AssemblyName System.Runtime.WindowsRuntime
  $null = [Windows.Storage.StorageFile,Windows.Storage,ContentType=WindowsRuntime]
  $null = [Windows.Media.Ocr.OcrEngine,Windows.Foundation,ContentType=WindowsRuntime]
  $null = [Windows.Graphics.Imaging.BitmapDecoder,Windows.Foundation,ContentType=WindowsRuntime]
  $asTask = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
  function Await($op, $type) { $m = $asTask.MakeGenericMethod($type); $t = $m.Invoke($null, @($op)); $t.Wait(-1) | Out-Null; $t.Result }
  $file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($Image)) ([Windows.Storage.StorageFile])
  $stream = Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
  $dec = Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
  $bmp = Await ($dec.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
  $eng = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()
  if ($eng -eq $null) { [IO.File]::WriteAllText($Out, '', (New-Object Text.UTF8Encoding($false))); exit 0 }
  $res = Await ($eng.RecognizeAsync($bmp)) ([Windows.Media.Ocr.OcrResult])
  [IO.File]::WriteAllText($Out, [string]$res.Text, (New-Object Text.UTF8Encoding($false)))
} catch {
  [IO.File]::WriteAllText($Out, '', (New-Object Text.UTF8Encoding($false)))
}
)PS";

bool OcrSelfCheckScript(std::wstring& scriptPath) {
    scriptPath = JoinPath(g_store.dataRoot, L"ocr.ps1");
    std::string cur;
    if (ReadFileBytes(scriptPath, cur) && cur == OCR_SCRIPT) return true;
    return WriteFileAtomic(scriptPath, OCR_SCRIPT, strlen(OCR_SCRIPT));
}

struct OcrJob { HWND notify; uint64_t id; std::wstring png, script, out; };

static unsigned __stdcall OcrThread(void* p) {
    OcrJob* j = (OcrJob*)p;
    std::wstring result;
    std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + j->script + L"\" -Image \"" + j->png + L"\" -Out \"" + j->out + L"\"";
    STARTUPINFOW si = {};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS, nullptr, nullptr, &si, &pi)) {
        if (WaitForSingleObject(pi.hProcess, 90000) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        std::string raw;
        if (ReadFileBytes(j->out, raw)) result = core::FromUtf8(raw);
    }
    DeleteFileW(j->out.c_str());
    // lParam: heap OcrResult owned by the receiver
    OcrResult* r = new OcrResult{j->id, result};
    if (!PostMessageW(j->notify, WM_OCR_DONE, 0, (LPARAM)r)) delete r;
    delete j;
    return 0;
}

void OcrStart(HWND notify, uint64_t id, const std::wstring& pngPath) {
    std::wstring script;
    if (!OcrSelfCheckScript(script)) return;
    OcrJob* j = new OcrJob{notify, id, pngPath, script, JoinPath(g_store.dataRoot, L"ocr-" + core::HexId(id) + L".out")};
    uintptr_t t = _beginthreadex(nullptr, 0, OcrThread, j, 0, nullptr);
    if (!t) delete j; else CloseHandle((HANDLE)t);
}
