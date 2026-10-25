#pragma once
// Batch mode: the same steps (text recognition, watermark, compress, password, signature) for every PDF of a folder.
#include <functional>
#include <string>
#include <vector>

namespace batch {

struct Job {
    std::wstring inputDir, outputDir;     // empty outputDir: next to each original, named "<name> (batch).pdf"
    bool subfolders = true;
    bool ocr = false;
    std::wstring ocrLanguage = L"eng";
    std::wstring watermark;               // empty: none
    int compress = 0;                     // 0 none, 1 balanced, 2 high quality, 3 smallest
    bool greyscale = false;
    std::wstring userPassword, ownerPassword;   // both empty: no password
    std::wstring certificate, certificatePassword;   // path of a .pfx file; empty: do not sign
    std::wstring signReason = L"Approved";
};

struct Result {
    int total = 0, done = 0, failed = 0;
    std::vector<std::wstring> problems;   // one line per file that was skipped or failed
    bool cancelled = false;
};

bool load(const std::wstring& iniPath, Job& job);
bool save(const std::wstring& iniPath, const Job& job);

// `progress(done, total, file)` returns false to stop.
Result run(const Job& job, const std::function<bool(int, int, const std::wstring&)>& progress);

} // namespace batch
