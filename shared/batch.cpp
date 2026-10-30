#include "batch.hpp"
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <filesystem>
#include <map>
#endif
#include <algorithm>
#include <cwctype>
#include "platform.hpp"
#include "compress.hpp"
#include "document.hpp"
#include "ocr.hpp"
#include "pageobjects.hpp"
#include "protect.hpp"
#include "sign.hpp"

namespace batch {
namespace {

bool sameText(const std::wstring& a, const std::wstring& b) {   // ignoring case
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (std::towlower(a[i]) != std::towlower(b[i])) return false;
    return true;
}
bool endsWith(const std::wstring& s, const std::wstring& tail) { return s.size() >= tail.size() && sameText(s.substr(s.size() - tail.size()), tail); }
const std::wstring kSep(1, platform::pathSeparator());

#ifdef _WIN32
void listPdfs(const std::wstring& dir, bool recurse, std::vector<std::wstring>& out, int depth) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (recurse && depth < 8 && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) listPdfs(dir + L"\\" + name, recurse, out, depth + 1);
        } else if (name.size() > 4 && _wcsicmp(name.c_str() + name.size() - 4, L".pdf") == 0) out.push_back(dir + L"\\" + name);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

void makeDirs(const std::wstring& path) {
    for (size_t p = path.find(L'\\', 3); p != std::wstring::npos; p = path.find(L'\\', p + 1)) CreateDirectoryW(path.substr(0, p).c_str(), nullptr);
    CreateDirectoryW(path.c_str(), nullptr);
}

std::wstring read(const std::wstring& ini, const wchar_t* key, const wchar_t* def = L"") {
    wchar_t buf[2048];
    GetPrivateProfileStringW(L"batch", key, def, buf, 2048, ini.c_str());
    return buf;
}
void put(const std::wstring& ini, const wchar_t* key, const std::wstring& v) { WritePrivateProfileStringW(L"batch", key, v.c_str(), ini.c_str()); }
bool exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }
#else
namespace fs = std::filesystem;
void listPdfs(const std::wstring& dir, bool recurse, std::vector<std::wstring>& out, int depth) {
    std::error_code ec;
    for (fs::directory_iterator it(platform::utf8(dir), ec), end; !ec && it != end; it.increment(ec)) {
        const std::wstring name = platform::wide(it->path().filename().string());
        if (name.empty() || name[0] == L'.') continue;
        if (it->is_directory(ec) && !it->is_symlink(ec)) { if (recurse && depth < 8) listPdfs(dir + L"/" + name, recurse, out, depth + 1); }
        else if (endsWith(name, L".pdf")) out.push_back(dir + L"/" + name);
    }
}
void makeDirs(const std::wstring& path) { std::error_code ec; fs::create_directories(platform::utf8(path), ec); }

// The preset file: "key=value" lines under [batch], as the Windows app writes them.
std::map<std::wstring, std::wstring> readIni(const std::wstring& ini) {
    std::map<std::wstring, std::wstring> m;
    const std::wstring text = platform::readTextFile(ini);
    size_t p = 0;
    while (p < text.size()) {
        size_t e = text.find(L'\n', p);
        std::wstring line = text.substr(p, e == std::wstring::npos ? std::wstring::npos : e - p);
        p = e == std::wstring::npos ? text.size() : e + 1;
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ')) line.pop_back();
        const size_t eq = line.find(L'=');
        if (eq != std::wstring::npos && line[0] != L'[' && line[0] != L';') m[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return m;
}
std::wstring read(const std::wstring& ini, const wchar_t* key, const wchar_t* def = L"") {
    const auto m = readIni(ini);
    auto it = m.find(key);
    return it != m.end() ? it->second : def;
}
void put(const std::wstring& ini, const wchar_t* key, const std::wstring& v) {
    auto m = readIni(ini);
    m[key] = v;
    std::string text = "[batch]\n";
    for (const auto& kv : m) text += platform::utf8(kv.first) + "=" + platform::utf8(kv.second) + "\n";
    platform::writeFile(ini, std::vector<unsigned char>(text.begin(), text.end()));
}
bool exists(const std::wstring& path) { std::error_code ec; return fs::exists(platform::utf8(path), ec); }
#endif

} // namespace

bool load(const std::wstring& ini, Job& j) {
    if (!exists(ini)) return false;
    j.inputDir = read(ini, L"input"); j.outputDir = read(ini, L"output");
    j.subfolders = read(ini, L"subfolders", L"1") == L"1";
    j.ocr = read(ini, L"ocr") == L"1"; j.ocrLanguage = read(ini, L"ocrLanguage", L"eng");
    j.watermark = read(ini, L"watermark");
    j.compress = static_cast<int>(wcstol(read(ini, L"compress", L"0").c_str(), nullptr, 10));
    j.greyscale = read(ini, L"greyscale") == L"1";
    j.userPassword = read(ini, L"userPassword"); j.ownerPassword = read(ini, L"ownerPassword");
    j.certificate = read(ini, L"certificate"); j.certificatePassword = read(ini, L"certificatePassword");
    j.signReason = read(ini, L"signReason", L"Approved");
    return true;
}

bool save(const std::wstring& ini, const Job& j) {
    put(ini, L"input", j.inputDir); put(ini, L"output", j.outputDir);
    put(ini, L"subfolders", j.subfolders ? L"1" : L"0");
    put(ini, L"ocr", j.ocr ? L"1" : L"0"); put(ini, L"ocrLanguage", j.ocrLanguage);
    put(ini, L"watermark", j.watermark);
    put(ini, L"compress", std::to_wstring(j.compress));
    put(ini, L"greyscale", j.greyscale ? L"1" : L"0");
    put(ini, L"userPassword", j.userPassword); put(ini, L"ownerPassword", j.ownerPassword);
    put(ini, L"certificate", j.certificate); put(ini, L"certificatePassword", j.certificatePassword);
    put(ini, L"signReason", j.signReason);
    return exists(ini);
}

Result run(const Job& job, const std::function<bool(int, int, const std::wstring&)>& progress) {
    Result r;
    std::vector<std::wstring> files;
    listPdfs(job.inputDir, job.subfolders, files, 0);
    std::sort(files.begin(), files.end());
    const bool sameFolder = job.outputDir.empty() || sameText(job.outputDir, job.inputDir);
    // never process the results of an earlier run again
    files.erase(std::remove_if(files.begin(), files.end(), [&](const std::wstring& f) { return sameFolder && endsWith(f, L" (batch).pdf"); }), files.end());
    r.total = static_cast<int>(files.size());

#ifdef _WIN32
    HCERTSTORE store = nullptr;
    PCCERT_CONTEXT cert = nullptr;
    if (!job.certificate.empty()) {
        std::wstring err;
        cert = sign::certFromPfx(job.certificate, job.certificatePassword, store, err);
        if (!cert) { r.problems.push_back(L"The certificate could not be opened: " + err); r.failed = r.total; return r; }
    }
#else
    sign::Signer signer;
    bool cert = false;
    if (!job.certificate.empty()) {
        std::wstring err = L"signing is not available in this build";
        cert = sign::system().signerFromFile && sign::system().signerFromFile(job.certificate, job.certificatePassword, signer, err);
        if (!cert) { r.problems.push_back(L"The certificate could not be opened: " + err); r.failed = r.total; return r; }
    }
#endif
    std::wstring ocrWhy;
    const bool ocrReady = !job.ocr || ocr::available(ocrWhy);
    if (!ocrReady) r.problems.push_back(L"Text recognition is not available: " + ocrWhy);

    int index = 0;
    for (const std::wstring& in : files) {
        if (progress && !progress(index++, r.total, in)) { r.cancelled = true; break; }
        const std::wstring name = in.substr(in.find_last_of(L"\\/") + 1);
        Document d;
        if (d.open(in) != OpenResult::Ok) { ++r.failed; r.problems.push_back(name + L": could not be opened (damaged or password-protected)"); continue; }
        std::vector<int> all(d.pageCount());
        for (int i = 0; i < d.pageCount(); ++i) all[i] = i;
        std::wstring problem;
        if (job.ocr && ocrReady) {
            ocr::Result o = ocr::run(d, all, job.ocrLanguage, true, nullptr);
            if (!o.error.empty()) problem = L"text recognition: " + o.error;
        }
        if (problem.empty() && !job.watermark.empty()) {
            pageobj::WatermarkStyle st;
            if (!pageobj::isLatin(job.watermark) || !pageobj::addWatermark(d, all, job.watermark, st)) problem = L"the watermark could not be added";
        }
        if (problem.empty() && (job.compress > 0 || job.greyscale)) {
            compress::Options o;
            static const int dpis[] = {150, 150, 220, 96}, quals[] = {80, 80, 88, 70};
            o.dpi = job.compress > 0 ? dpis[job.compress] : 300;
            o.jpegQuality = job.compress > 0 ? quals[job.compress] : 88;
            o.greyscale = job.greyscale;
            compress::run(d, o);   // nothing to reduce is not an error
        }
        const bool wantPassword = !job.userPassword.empty() || !job.ownerPassword.empty();
        if (problem.empty() && wantPassword && !cert) {
            protect::Options po;
            po.userPassword = job.userPassword;
            po.ownerPassword = job.ownerPassword.empty() ? job.userPassword : job.ownerPassword;
            if (!d.setProtection(po)) problem = L"the password could not be set";
        }
        // where the result goes
        std::wstring out;
        if (sameFolder) out = in.substr(0, in.size() - 4) + L" (batch).pdf";
        else {
            std::wstring rel = in.substr(std::min(job.inputDir.size() + 1, in.size()));
            out = job.outputDir + kSep + rel;
            makeDirs(out.substr(0, out.find_last_of(L"\\/")));
            if (sameText(out, in)) out = out.substr(0, out.size() - 4) + L" (batch).pdf";
        }
        if (problem.empty()) {
            if (cert) {
                std::vector<unsigned char> plain, signedBytes;
                std::wstring err;
                sign::Options so; so.reason = job.signReason;
#ifdef _WIN32
                const bool signedOk = d.serialize(plain, FPDF_REMOVE_SECURITY) && sign::signPdf(plain, cert, so, signedBytes, err);
#else
                const bool signedOk = d.serialize(plain, FPDF_REMOVE_SECURITY) && sign::signPdf(plain, signer, so, signedBytes, err);
#endif
                if (!signedOk) problem = L"signing: " + (err.empty() ? std::wstring(L"failed") : err);
                else if (Document::writeFile(out, signedBytes) != SaveResult::Ok) problem = L"the result could not be written";
            } else if (d.saveAs(out) != SaveResult::Ok) problem = L"the result could not be written";
        }
        if (problem.empty()) ++r.done; else { ++r.failed; r.problems.push_back(name + L": " + problem); }
    }
#ifdef _WIN32
    if (cert) CertFreeCertificateContext(cert);
    if (store) CertCloseStore(store, 0);
#endif
    return r;
}

} // namespace batch
