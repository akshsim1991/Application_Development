#import "Render.h"
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <cstring>
#include <vector>

CGImageRef FPRenderPage(Document& doc, int page, int pw, int ph, int rx, int ry, int rw, int rh, pagecolours::Mode colours) {
    if (rw <= 0 || rh <= 0 || page < 0 || page >= doc.pageCount()) return nullptr;
    const auto& api = pdfium::api();
    const int stride = rw * 4;
    std::vector<unsigned char> buf(static_cast<size_t>(stride) * rh);
    FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(rw, rh, FPDFBitmap_BGRx, buf.data(), stride);
    if (!bmp) return nullptr;
    api.FPDFBitmap_FillRect(bmp, 0, 0, rw, rh, 0xFFFFFFFF);
    doc.renderPage(page, bmp, -rx, -ry, pw, ph);
    api.FPDFBitmap_Destroy(bmp);
    pagecolours::apply(colours, buf.data(), rw, rh, stride);
    // PDFium's BGRx is, read as a little-endian 32-bit number, xRGB: "skip first" in Core Graphics' words.
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef ctx = CGBitmapContextCreate(buf.data(), rw, rh, 8, stride, cs,
                                             static_cast<CGBitmapInfo>(kCGImageAlphaNoneSkipFirst) | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);
    if (!ctx) return nullptr;
    CGImageRef img = CGBitmapContextCreateImage(ctx);   // a copy: the buffer can go
    CGContextRelease(ctx);
    return img;
}

bool FPWritePNG(CGImageRef image, const char* path) {
    if (!image) return false;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8*>(path), strlen(path), false);
    if (!url) return false;
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
    CFRelease(url);
    if (!dest) return false;
    CGImageDestinationAddImage(dest, image, nullptr);
    const bool ok = CGImageDestinationFinalize(dest);
    CFRelease(dest);
    return ok;
}
