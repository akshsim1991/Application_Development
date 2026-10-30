#pragma once
// Full-text search across all PDFs in a folder. The text of each file is cached (keyed by path, size and time), so
// repeated searches only read the files that changed.
#include <functional>
#include <string>
#include <vector>

namespace foldersearch {

struct Hit { std::wstring file; int page = 0; std::wstring snippet; };

struct Stats { int files = 0, scanned = 0, cached = 0, skipped = 0; };

// `progress(done, total, file)` returns false to cancel. The cache lives in `cacheDir` (created when needed).
std::vector<Hit> search(const std::wstring& folder, const std::wstring& query, bool matchCase, const std::wstring& cacheDir,
                        const std::function<bool(int, int, const std::wstring&)>& progress, Stats& stats, size_t maxHits = 2000);

} // namespace foldersearch
