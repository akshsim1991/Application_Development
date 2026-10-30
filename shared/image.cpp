#include "image.hpp"
#include "platform.hpp"
#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#include <objbase.h>
#include <propidl.h>
#include <algorithm>
using std::min; using std::max;
#include <gdiplus.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#else
#include <cstdio>
#include <png.h>
#endif

namespace image {

#if defined(_WIN32)

namespace {
// GDI+ is started once, the first time a picture is needed, and stays for the life of the program.
bool gdiplus() {
    static ULONG_PTR token = 0;
    static const bool ok = [] { Gdiplus::GdiplusStartupInput in; return Gdiplus::GdiplusStartup(&token, &in, nullptr) == Gdiplus::Ok; }();
    return ok;
}

bool encoder(const wchar_t* mime, CLSID& out) {
    UINT n = 0, size = 0;
    if (Gdiplus::GetImageEncodersSize(&n, &size) != Gdiplus::Ok || !size) return false;
    std::vector<unsigned char> buf(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    if (Gdiplus::GetImageEncoders(n, size, codecs) != Gdiplus::Ok) return false;
    for (UINT i = 0; i < n; ++i) if (wcscmp(codecs[i].MimeType, mime) == 0) { out = codecs[i].Clsid; return true; }
    return false;
}

bool pixelsOf(Gdiplus::Bitmap& bmp, Picture& out) {
    out.w = static_cast<int>(bmp.GetWidth());
    out.h = static_cast<int>(bmp.GetHeight());
    if (out.w <= 0 || out.h <= 0) return false;
    out.px.assign(static_cast<size_t>(out.w) * out.h * 4, 0);
    Gdiplus::BitmapData bd;
    Gdiplus::Rect all(0, 0, out.w, out.h);
    if (bmp.LockBits(&all, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) return false;
    for (int y = 0; y < out.h; ++y)
        memcpy(out.px.data() + static_cast<size_t>(y) * out.w * 4, static_cast<unsigned char*>(bd.Scan0) + static_cast<size_t>(y) * bd.Stride, static_cast<size_t>(out.w) * 4);
    bmp.UnlockBits(&bd);
    return true;
}
} // namespace

bool encode(const unsigned char* bgra, int w, int h, int stride, Format f, int quality, bool alpha, std::vector<unsigned char>& out) {
    CLSID clsid;
    if (!gdiplus() || !encoder(f == Format::Png ? L"image/png" : L"image/jpeg", clsid)) return false;
    Gdiplus::Bitmap bmp(w, h, stride, alpha ? PixelFormat32bppARGB : PixelFormat32bppRGB, const_cast<unsigned char*>(bgra));
    IStream* st = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &st))) return false;
    Gdiplus::EncoderParameters params{};
    ULONG q = static_cast<ULONG>(quality);
    params.Count = 1;
    params.Parameter[0].Guid = Gdiplus::EncoderQuality;
    params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    params.Parameter[0].NumberOfValues = 1;
    params.Parameter[0].Value = &q;
    bool ok = bmp.Save(st, &clsid, f == Format::Jpeg ? &params : nullptr) == Gdiplus::Ok;
    if (ok) {
        STATSTG stat{};
        st->Stat(&stat, STATFLAG_NONAME);
        LARGE_INTEGER z{};
        st->Seek(z, STREAM_SEEK_SET, nullptr);
        out.resize(static_cast<size_t>(stat.cbSize.QuadPart));
        ULONG got = 0;
        ok = SUCCEEDED(st->Read(out.data(), static_cast<ULONG>(out.size()), &got)) && got == out.size();
    }
    st->Release();
    return ok;
}

bool load(const std::wstring& path, Picture& out) {
    if (!gdiplus()) return false;
    Gdiplus::Bitmap bmp(path.c_str());
    return bmp.GetLastStatus() == Gdiplus::Ok && pixelsOf(bmp, out);
}

bool sizeOf(const std::wstring& path, int& w, int& h) {
    if (!gdiplus()) return false;
    Gdiplus::Bitmap bmp(path.c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok || bmp.GetWidth() == 0 || bmp.GetHeight() == 0) return false;
    w = static_cast<int>(bmp.GetWidth());
    h = static_cast<int>(bmp.GetHeight());
    return true;
}

bool scale(const unsigned char* bgra, int w, int h, int stride, int w2, int h2, Picture& out) {
    if (!gdiplus()) return false;
    Gdiplus::Bitmap in(w, h, stride, PixelFormat32bppARGB, const_cast<unsigned char*>(bgra));
    Gdiplus::Bitmap scaled(w2, h2, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&scaled);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.DrawImage(&in, Gdiplus::Rect(0, 0, w2, h2), 0, 0, w, h, Gdiplus::UnitPixel);
    }
    return pixelsOf(scaled, out);
}

#elif defined(__APPLE__)

namespace {
template <typename T> struct Ref {   // releases a Core Foundation object
    T p = nullptr;
    explicit Ref(T x = nullptr) : p(x) {}
    ~Ref() { if (p) CFRelease(p); }
    Ref(const Ref&) = delete;
    Ref& operator=(const Ref&) = delete;
    operator T() const { return p; }
};

// A CGImage of straight-alpha BGRA pixels (copied).
CGImageRef imageOf(const unsigned char* bgra, int w, int h, int stride, bool alpha) {
    Ref<CFDataRef> data(CFDataCreate(nullptr, bgra, static_cast<CFIndex>(stride) * h));
    Ref<CGDataProviderRef> prov(CGDataProviderCreateWithCFData(data));
    Ref<CGColorSpaceRef> cs(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    const CGBitmapInfo info = static_cast<CGBitmapInfo>(alpha ? kCGImageAlphaFirst : kCGImageAlphaNoneSkipFirst) | kCGBitmapByteOrder32Little;
    return CGImageCreate(static_cast<size_t>(w), static_cast<size_t>(h), 8, 32, static_cast<size_t>(stride), cs, info, prov, nullptr, false,
                         kCGRenderingIntentDefault);
}

// Draws `img` into straight-alpha BGRA pixels of w x h.
bool pixelsOf(CGImageRef img, int w, int h, Picture& out, bool highQuality) {
    out.w = w;
    out.h = h;
    out.px.assign(static_cast<size_t>(w) * h * 4, 0);
    Ref<CGColorSpaceRef> cs(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    Ref<CGContextRef> ctx(CGBitmapContextCreate(out.px.data(), static_cast<size_t>(w), static_cast<size_t>(h), 8, static_cast<size_t>(w) * 4, cs,
                                                static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedFirst) | kCGBitmapByteOrder32Little));
    if (!ctx) return false;
    CGContextSetInterpolationQuality(ctx, highQuality ? kCGInterpolationHigh : kCGInterpolationDefault);
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), img);
    for (size_t i = 0; i < out.px.size(); i += 4) {   // premultiplied -> straight, as on Windows
        const unsigned a = out.px[i + 3];
        if (a && a < 255) for (int k = 0; k < 3; ++k) out.px[i + k] = static_cast<unsigned char>(std::min(255u, out.px[i + k] * 255u / a));
    }
    return true;
}

CGImageSourceRef sourceOf(const std::wstring& path) {
    const std::string p = platform::utf8(path);
    Ref<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(p.c_str()), static_cast<CFIndex>(p.size()), false));
    return url ? CGImageSourceCreateWithURL(url, nullptr) : nullptr;
}
} // namespace

bool encode(const unsigned char* bgra, int w, int h, int stride, Format f, int quality, bool alpha, std::vector<unsigned char>& out) {
    Ref<CGImageRef> img(imageOf(bgra, w, h, stride, alpha));
    if (!img) return false;
    Ref<CFMutableDataRef> data(CFDataCreateMutable(nullptr, 0));
    Ref<CGImageDestinationRef> dest(CGImageDestinationCreateWithData(data, f == Format::Png ? CFSTR("public.png") : CFSTR("public.jpeg"), 1, nullptr));
    if (!dest) return false;
    const float q = quality / 100.0f;
    Ref<CFNumberRef> qn(CFNumberCreate(nullptr, kCFNumberFloatType, &q));
    const void* keys[] = {kCGImageDestinationLossyCompressionQuality};
    const void* vals[] = {qn};
    Ref<CFDictionaryRef> props(CFDictionaryCreate(nullptr, keys, vals, f == Format::Jpeg ? 1 : 0, &kCFTypeDictionaryKeyCallBacks,
                                                  &kCFTypeDictionaryValueCallBacks));
    CGImageDestinationAddImage(dest, img, props);
    if (!CGImageDestinationFinalize(dest)) return false;
    out.assign(CFDataGetBytePtr(data), CFDataGetBytePtr(data) + CFDataGetLength(data));
    return true;
}

bool load(const std::wstring& path, Picture& out) {
    Ref<CGImageSourceRef> src(sourceOf(path));
    if (!src) return false;
    Ref<CGImageRef> img(CGImageSourceCreateImageAtIndex(src, 0, nullptr));
    if (!img) return false;
    return pixelsOf(img, static_cast<int>(CGImageGetWidth(img)), static_cast<int>(CGImageGetHeight(img)), out, false);
}

bool sizeOf(const std::wstring& path, int& w, int& h) {
    Ref<CGImageSourceRef> src(sourceOf(path));
    if (!src) return false;
    Ref<CFDictionaryRef> props(CGImageSourceCopyPropertiesAtIndex(src, 0, nullptr));
    if (!props) return false;
    auto num = [&](CFStringRef key, int& v) {
        auto n = static_cast<CFNumberRef>(CFDictionaryGetValue(props, key));
        return n && CFNumberGetValue(n, kCFNumberIntType, &v);
    };
    return num(kCGImagePropertyPixelWidth, w) && num(kCGImagePropertyPixelHeight, h) && w > 0 && h > 0;
}

bool scale(const unsigned char* bgra, int w, int h, int stride, int w2, int h2, Picture& out) {
    Ref<CGImageRef> img(imageOf(bgra, w, h, stride, true));
    return img && pixelsOf(img, w2, h2, out, true);
}

#else   // Linux, for the engine check: PNG through libpng, simple area scaling, no JPEG

bool encode(const unsigned char* bgra, int w, int h, int stride, Format f, int, bool alpha, std::vector<unsigned char>& out) {
    if (f != Format::Png) return false;
    png_image img{};
    img.version = PNG_IMAGE_VERSION;
    img.width = static_cast<png_uint_32>(w);
    img.height = static_cast<png_uint_32>(h);
    img.format = alpha ? PNG_FORMAT_BGRA : PNG_FORMAT_BGRA;
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        memcpy(px.data() + static_cast<size_t>(y) * w * 4, bgra + static_cast<size_t>(y) * stride, static_cast<size_t>(w) * 4);
        if (!alpha) for (int x = 0; x < w; ++x) px[(static_cast<size_t>(y) * w + x) * 4 + 3] = 255;
    }
    png_alloc_size_t size = 0;
    if (!png_image_write_to_memory(&img, nullptr, &size, 0, px.data(), 0, nullptr)) return false;
    out.resize(size);
    return png_image_write_to_memory(&img, out.data(), &size, 0, px.data(), 0, nullptr) != 0;
}

bool load(const std::wstring& path, Picture& out) {
    std::vector<unsigned char> file;
    if (!platform::readFile(path, file)) return false;
    png_image img{};
    img.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&img, file.data(), file.size())) return false;
    img.format = PNG_FORMAT_BGRA;
    out.w = static_cast<int>(img.width);
    out.h = static_cast<int>(img.height);
    out.px.assign(PNG_IMAGE_SIZE(img), 0);
    return png_image_finish_read(&img, nullptr, out.px.data(), 0, nullptr) != 0;
}

bool sizeOf(const std::wstring& path, int& w, int& h) {
    Picture p;
    if (!load(path, p)) return false;
    w = p.w; h = p.h;
    return true;
}

bool scale(const unsigned char* bgra, int w, int h, int stride, int w2, int h2, Picture& out) {
    out.w = w2; out.h = h2;
    out.px.assign(static_cast<size_t>(w2) * h2 * 4, 0);
    for (int y = 0; y < h2; ++y)
        for (int x = 0; x < w2; ++x) {
            const int sx = std::min(w - 1, x * w / w2), sy = std::min(h - 1, y * h / h2);
            memcpy(&out.px[(static_cast<size_t>(y) * w2 + x) * 4], bgra + static_cast<size_t>(sy) * stride + sx * 4, 4);
        }
    return true;
}

#endif

bool save(const unsigned char* bgra, int w, int h, int stride, Format f, int quality, bool alpha, const std::wstring& path) {
    std::vector<unsigned char> bytes;
    if (!encode(bgra, w, h, stride, f, quality, alpha, bytes)) return false;
    return platform::writeFile(path, bytes);
}

} // namespace image
