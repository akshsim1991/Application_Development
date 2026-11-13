// Reading what was copied (text, HTML, RTF, pictures, files) and putting items back on the clipboard.
#include "app.h"

UINT g_cfHtml, g_cfRtf, g_cfPng, g_cfDropEffect, g_cfExclude, g_cfCanHistory, g_cfViewerIgnore, g_cfPngMime;
DWORD g_ignoreSeq = 0;
static ULONG_PTR g_gdipToken = 0;

bool GdiplusStart() {
    if (g_gdipToken) return true;
    Gdiplus::GdiplusStartupInput in;
    return Gdiplus::GdiplusStartup(&g_gdipToken, &in, nullptr) == Gdiplus::Ok;
}

void ClipInit() {
    g_cfHtml = RegisterClipboardFormatW(L"HTML Format");
    g_cfRtf = RegisterClipboardFormatW(L"Rich Text Format");
    g_cfPng = RegisterClipboardFormatW(L"PNG");
    g_cfPngMime = RegisterClipboardFormatW(L"image/png");
    g_cfDropEffect = RegisterClipboardFormatW(L"Preferred DropEffect");
    g_cfExclude = RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
    g_cfCanHistory = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
    g_cfViewerIgnore = RegisterClipboardFormatW(L"Clipboard Viewer Ignore");
    GdiplusStart();
}

CLSID EncoderFor(const wchar_t* mime) {
    CLSID none = {};
    UINT n = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&n, &size);
    if (!size) return none;
    std::vector<char> buf(size);
    auto* enc = (Gdiplus::ImageCodecInfo*)buf.data();
    Gdiplus::GetImageEncoders(n, size, enc);
    for (UINT i = 0; i < n; i++) if (wcscmp(enc[i].MimeType, mime) == 0) return enc[i].Clsid;
    return none;
}

bool LoadImageSize(const std::wstring& path, int& w, int& h) {
    Gdiplus::Image img(path.c_str());
    if (img.GetLastStatus() != Gdiplus::Ok) return false;
    w = (int)img.GetWidth();
    h = (int)img.GetHeight();
    return true;
}

static bool OpenCb(HWND w) {
    for (int i = 0; i < 25; i++) {
        if (OpenClipboard(w)) return true;
        Sleep(12);
    }
    return false;
}

static HGLOBAL AllocCopy(const void* p, size_t n) {
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n);
    if (!g) return nullptr;
    void* d = GlobalLock(g);
    if (!d) { GlobalFree(g); return nullptr; }
    memcpy(d, p, n);
    GlobalUnlock(g);
    return g;
}

static bool SetBytes(UINT fmt, const void* p, size_t n) {
    HGLOBAL g = AllocCopy(p, n);
    if (!g) return false;
    if (!SetClipboardData(fmt, g)) { GlobalFree(g); return false; }
    return true;
}

static bool SetWide(const std::wstring& s) {
    return SetBytes(CF_UNICODETEXT, s.c_str(), (s.size() + 1) * sizeof(wchar_t));
}

static std::string GrabBytes(UINT fmt, size_t cap) {
    std::string out;
    HANDLE h = GetClipboardData(fmt);
    if (!h) return out;
    SIZE_T n = GlobalSize(h);
    if (!n || n > cap) return out;
    const char* p = (const char*)GlobalLock(h);
    if (!p) return out;
    out.assign(p, n);
    GlobalUnlock(h);
    return out;
}

static std::string TrimPng(const std::string& b) {
    static const char iend[] = {'I', 'E', 'N', 'D', (char)0xAE, (char)0x42, (char)0x60, (char)0x82};
    size_t pos = b.rfind(std::string(iend, 8));
    if (pos == std::string::npos) return b;
    return b.substr(0, pos + 8);
}

static std::wstring TempPath() {
    return JoinPath(g_store.dir, L"incoming-" + std::to_wstring(GetTickCount64()) + L".png.tmp");
}

static bool SavePngFromBitmap(Gdiplus::Bitmap& bmp, const std::wstring& path) {
    CLSID png = EncoderFor(L"image/png");
    return bmp.Save(path.c_str(), &png, nullptr) == Gdiplus::Ok;
}

bool ClipCapture(HWND owner, Captured& out, bool wantImages, bool wantFiles, bool wantRich, bool& privacyFlagged) {
    out = Captured();
    privacyFlagged = false;
    if (!OpenCb(owner)) return false;
    struct Closer { ~Closer() { CloseClipboard(); } } closer;

    if (IsClipboardFormatAvailable(g_cfExclude) || IsClipboardFormatAvailable(g_cfViewerIgnore)) privacyFlagged = true;
    if (IsClipboardFormatAvailable(g_cfCanHistory)) {
        std::string v = GrabBytes(g_cfCanHistory, 16);
        if (v.size() >= 4 && *(const DWORD*)v.data() == 0) privacyFlagged = true;
    }
    if (privacyFlagged) return true;

    if (IsClipboardFormatAvailable(CF_HDROP)) {
        if (!wantFiles) return true;
        HDROP d = (HDROP)GetClipboardData(CF_HDROP);
        if (d) {
            UINT n = DragQueryFileW(d, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n && i < 5000; i++) {
                UINT len = DragQueryFileW(d, i, nullptr, 0);
                std::wstring p(len + 1, L'\0');
                DragQueryFileW(d, i, &p[0], len + 1);
                p.resize(len);
                if (!p.empty()) out.files.push_back(p);
            }
            if (IsClipboardFormatAvailable(g_cfDropEffect)) {
                std::string de = GrabBytes(g_cfDropEffect, 16);
                if (de.size() >= 4) out.dropEffect = (int)*(const DWORD*)de.data();
            }
            if (!out.files.empty()) out.kind = K_FILES;
        }
        return true;
    }

    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h) {
            const wchar_t* p = (const wchar_t*)GlobalLock(h);
            if (p) {
                size_t maxChars = GlobalSize(h) / sizeof(wchar_t);
                size_t n = 0;
                while (n < maxChars && p[n]) n++;
                out.text.assign(p, n);
                GlobalUnlock(h);
            }
        }
        if (!out.text.empty()) {
            out.kind = K_TEXT;
            if (wantRich) {
                if (IsClipboardFormatAvailable(g_cfHtml)) out.html = GrabBytes(g_cfHtml, 4 << 20);
                if (IsClipboardFormatAvailable(g_cfRtf)) out.rtf = GrabBytes(g_cfRtf, 4 << 20);
                while (!out.html.empty() && out.html.back() == '\0') out.html.pop_back();
                while (!out.rtf.empty() && out.rtf.back() == '\0') out.rtf.pop_back();
            }
            return true;
        }
    }

    if (!wantImages) return true;
    std::string png;
    if (IsClipboardFormatAvailable(g_cfPng)) png = TrimPng(GrabBytes(g_cfPng, 256u << 20));
    else if (IsClipboardFormatAvailable(g_cfPngMime)) png = TrimPng(GrabBytes(g_cfPngMime, 256u << 20));
    std::wstring tmp = TempPath();
    if (png.size() > 16 && WriteFileAtomic(tmp, png.data(), png.size())) {
        // WriteFileAtomic leaves tmp+".tmp" renamed to tmp, fine
        int w = 0, h = 0;
        if (LoadImageSize(tmp, w, h) && w > 0 && h > 0) {
            out.kind = K_PICTURE;
            out.tmpPng = tmp;
            out.w = w;
            out.h = h;
            out.hash = core::Fnv64(png.data(), png.size());
            return true;
        }
        DeleteFileW(tmp.c_str());
    }
    if (IsClipboardFormatAvailable(CF_DIB)) {
        HANDLE h = GetClipboardData(CF_DIB);
        if (h) {
            BITMAPINFO* bi = (BITMAPINFO*)GlobalLock(h);
            if (bi) {
                DWORD hdr = bi->bmiHeader.biSize;
                DWORD colors = bi->bmiHeader.biClrUsed;
                if (!colors && bi->bmiHeader.biBitCount <= 8) colors = 1u << bi->bmiHeader.biBitCount;
                if (bi->bmiHeader.biCompression == BI_BITFIELDS && hdr == sizeof(BITMAPINFOHEADER)) hdr += 12;
                BYTE* bits = (BYTE*)bi + hdr + colors * sizeof(RGBQUAD);
                Gdiplus::Bitmap bmp(bi, bits);
                if (bmp.GetLastStatus() == Gdiplus::Ok && bmp.GetWidth() > 0) {
                    out.w = (int)bmp.GetWidth();
                    out.h = (int)bmp.GetHeight();
                    if (SavePngFromBitmap(bmp, tmp)) {
                        std::string back;
                        ReadFileBytes(tmp, back);
                        out.kind = K_PICTURE;
                        out.tmpPng = tmp;
                        out.hash = core::Fnv64(back.data(), back.size());
                    }
                }
                GlobalUnlock(h);
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------- putting things back
static bool SetPictureFormats(const std::wstring& pngPath) {
    std::string png;
    if (!ReadFileBytes(pngPath, png) || png.empty()) return false;
    Gdiplus::Bitmap src(pngPath.c_str());
    if (src.GetLastStatus() != Gdiplus::Ok) return false;
    int w = (int)src.GetWidth(), h = (int)src.GetHeight();
    // 24-bit DIB over white, bottom-up, for programs that do not know PNG
    Gdiplus::Bitmap flat(w, h, PixelFormat24bppRGB);
    {
        Gdiplus::Graphics g(&flat);
        g.Clear(Gdiplus::Color(255, 255, 255, 255));
        g.DrawImage(&src, 0, 0, w, h);
    }
    Gdiplus::BitmapData bd;
    Gdiplus::Rect r(0, 0, w, h);
    if (flat.LockBits(&r, Gdiplus::ImageLockModeRead, PixelFormat24bppRGB, &bd) != Gdiplus::Ok) return false;
    size_t stride = ((size_t)w * 3 + 3) & ~(size_t)3;
    std::string dib(sizeof(BITMAPINFOHEADER) + stride * h, '\0');
    BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)&dib[0];
    bih->biSize = sizeof(BITMAPINFOHEADER);
    bih->biWidth = w;
    bih->biHeight = h;
    bih->biPlanes = 1;
    bih->biBitCount = 24;
    bih->biCompression = BI_RGB;
    bih->biSizeImage = (DWORD)(stride * h);
    for (int y = 0; y < h; y++) {
        const BYTE* srow = (const BYTE*)bd.Scan0 + (ptrdiff_t)y * bd.Stride;
        memcpy(&dib[sizeof(BITMAPINFOHEADER) + stride * (h - 1 - y)], srow, (size_t)w * 3);
    }
    flat.UnlockBits(&bd);
    bool ok = SetBytes(g_cfPng, png.data(), png.size());
    ok = SetBytes(CF_DIB, dib.data(), dib.size()) && ok;
    return ok;
}

static PutResult Finish(bool ok) {
    CloseClipboard();
    g_ignoreSeq = GetClipboardSequenceNumber();
    return ok ? PUT_OK : PUT_FAILED;
}

PutResult ClipPutText(HWND owner, const std::wstring& text) {
    if (!OpenCb(owner)) return PUT_FAILED;
    EmptyClipboard();
    return Finish(SetWide(text));
}

PutResult ClipPutBitmapFile(HWND owner, const std::wstring& pngPath) {
    if (!OpenCb(owner)) return PUT_FAILED;
    EmptyClipboard();
    return Finish(SetPictureFormats(pngPath));
}

PutResult ClipPutItem(HWND owner, const Item& it, bool plain) {
    if (it.kind == K_TEXT || it.kind == K_SNIPPET) {
        std::wstring text = g_store.ReadText(it);
        std::string html, rtf;
        if (!plain && (it.flags & F_HTML)) html = g_store.ReadSide(it, L".html");
        if (!plain && (it.flags & F_RTF)) rtf = g_store.ReadSide(it, L".rtf");
        if (!OpenCb(owner)) return PUT_FAILED;
        EmptyClipboard();
        bool ok = SetWide(text);
        if (ok && !html.empty()) { html.push_back('\0'); SetBytes(g_cfHtml, html.data(), html.size()); }
        if (ok && !rtf.empty()) { rtf.push_back('\0'); SetBytes(g_cfRtf, rtf.data(), rtf.size()); }
        return Finish(ok);
    }
    if (it.kind == K_PICTURE) {
        if (!OpenCb(owner)) return PUT_FAILED;
        EmptyClipboard();
        return Finish(SetPictureFormats(g_store.MainPath(it)));
    }
    if (it.kind == K_FILES) {
        auto files = g_store.ReadFiles(it);
        std::vector<std::wstring> existing;
        for (auto& f : files) if (FileExistsW(f)) existing.push_back(f);
        if (plain) {
            std::wstring names = core::Join(files, L"\r\n");
            return ClipPutText(owner, names);
        }
        if (existing.empty()) return PUT_FILES_MISSING;
        size_t chars = 1;
        for (auto& f : existing) chars += f.size() + 1;
        std::string blob(sizeof(DROPFILES) + chars * sizeof(wchar_t), '\0');
        DROPFILES* df = (DROPFILES*)&blob[0];
        df->pFiles = sizeof(DROPFILES);
        df->fWide = TRUE;
        wchar_t* p = (wchar_t*)(&blob[0] + sizeof(DROPFILES));
        for (auto& f : existing) { memcpy(p, f.c_str(), f.size() * sizeof(wchar_t)); p += f.size() + 1; }
        if (!OpenCb(owner)) return PUT_FAILED;
        EmptyClipboard();
        bool ok = SetBytes(CF_HDROP, blob.data(), blob.size());
        DWORD effect = 1;   // always "copy": pasting a restored list never moves or deletes the originals
        SetBytes(g_cfDropEffect, &effect, sizeof effect);
        PutResult r = Finish(ok);
        if (r == PUT_OK && existing.size() < files.size()) return PUT_FILES_MISSING;
        return r;
    }
    return PUT_FAILED;
}

bool PngToSizedCopy(const std::wstring& srcPng, const std::wstring& dstPath, const wchar_t* mime, int percent) {
    Gdiplus::Bitmap src(srcPng.c_str());
    if (src.GetLastStatus() != Gdiplus::Ok) return false;
    int w = std::max(1, (int)src.GetWidth() * percent / 100), h = std::max(1, (int)src.GetHeight() * percent / 100);
    Gdiplus::Bitmap dst(w, h, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&dst);
        if (wcscmp(mime, L"image/jpeg") == 0 || wcscmp(mime, L"image/bmp") == 0) g.Clear(Gdiplus::Color(255, 255, 255, 255));
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(&src, Gdiplus::Rect(0, 0, w, h), 0, 0, (INT)src.GetWidth(), (INT)src.GetHeight(), Gdiplus::UnitPixel);
    }
    CLSID clsid = EncoderFor(mime);
    Gdiplus::EncoderParameters ep;
    ULONG q = 90;
    ep.Count = 1;
    ep.Parameter[0].Guid = Gdiplus::EncoderQuality;
    ep.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    ep.Parameter[0].NumberOfValues = 1;
    ep.Parameter[0].Value = &q;
    return dst.Save(dstPath.c_str(), &clsid, wcscmp(mime, L"image/jpeg") == 0 ? &ep : nullptr) == Gdiplus::Ok;
}
