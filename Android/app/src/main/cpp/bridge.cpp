// The bridge between the Android app (com.featherpdf.engine.Engine) and the shared engine. Every call takes one lock: the
// engine and PDFium are used by one thread at a time (pages are drawn on a background thread while the screen may edit).
// It uses no Android headers, so it is also built and tested on a desktop Java (tests/android/bridge_check.sh).
#include <jni.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "annot.hpp"
#include "crypto.hpp"
#include "document.hpp"
#include "image.hpp"
#include "pageobjects.hpp"
#include "pagecolours.hpp"
#include "pdfium_api.hpp"
#include "platform.hpp"
#include "search.hpp"

namespace {

std::recursive_mutex g_lock;
JavaVM* g_vm = nullptr;
jobject g_hooks = nullptr;   // global reference to the app's Engine.SystemHooks

struct Doc {
    Document d;
    pageobj::ParagraphHit hit;   // the paragraph found by the last paragraphAt()
};

Doc* docOf(jlong h) { return reinterpret_cast<Doc*>(h); }

// ---- text: Java strings are UTF-16; wchar_t is UTF-32 here
std::wstring wstr(JNIEnv* env, jstring s) {
    std::wstring out;
    if (!s) return out;
    const jsize n = env->GetStringLength(s);
    const jchar* c = env->GetStringChars(s, nullptr);
    for (jsize i = 0; i < n; ++i) {
        unsigned u = c[i];
        if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n && c[i + 1] >= 0xDC00 && c[i + 1] <= 0xDFFF) {
            u = 0x10000 + ((u - 0xD800) << 10) + (c[i + 1] - 0xDC00);
            ++i;
        }
        out.push_back(static_cast<wchar_t>(u));
    }
    env->ReleaseStringChars(s, c);
    return out;
}

jstring jstr(JNIEnv* env, const std::wstring& s) {
    std::vector<jchar> u;
    u.reserve(s.size());
    for (wchar_t w : s) {
        unsigned c = static_cast<unsigned>(w);
        if (c >= 0x10000 && c <= 0x10FFFF) {
            c -= 0x10000;
            u.push_back(static_cast<jchar>(0xD800 + (c >> 10)));
            u.push_back(static_cast<jchar>(0xDC00 + (c & 0x3FF)));
        } else u.push_back(static_cast<jchar>(c));
    }
    return env->NewString(u.data(), static_cast<jsize>(u.size()));
}

std::string utf8(JNIEnv* env, jstring s) { return platform::utf8(wstr(env, s)); }

// Java colours are 0xAARRGGBB; the engine's are COLORREF (0x00BBGGRR).
COLORREF colorref(jint argb) {
    const unsigned c = static_cast<unsigned>(argb);
    return RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}
jint argb(COLORREF c) { return static_cast<jint>(0xFF000000u | (GetRValue(c) << 16) | (GetGValue(c) << 8) | GetBValue(c)); }

jfloatArray floats(JNIEnv* env, const std::vector<float>& v) {
    jfloatArray a = env->NewFloatArray(static_cast<jsize>(v.size()));
    if (a && !v.empty()) env->SetFloatArrayRegion(a, 0, static_cast<jsize>(v.size()), v.data());
    return a;
}

std::vector<unsigned char> bytesOf(JNIEnv* env, jbyteArray a) {
    std::vector<unsigned char> v;
    if (!a) return v;
    v.resize(static_cast<size_t>(env->GetArrayLength(a)));
    if (!v.empty()) env->GetByteArrayRegion(a, 0, static_cast<jsize>(v.size()), reinterpret_cast<jbyte*>(v.data()));
    return v;
}
jbyteArray jbytes(JNIEnv* env, const std::vector<unsigned char>& v) {
    jbyteArray a = env->NewByteArray(static_cast<jsize>(v.size()));
    if (a && !v.empty()) env->SetByteArrayRegion(a, 0, static_cast<jsize>(v.size()), reinterpret_cast<const jbyte*>(v.data()));
    return a;
}

// ---- the app's system services (cryptography, pictures), called on whatever thread the engine runs
struct Env {
    JNIEnv* env = nullptr;
    bool attached = false;
    Env() {
        if (!g_vm) return;
        if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
#ifdef __ANDROID__
            attached = g_vm->AttachCurrentThread(&env, nullptr) == JNI_OK;
#else
            attached = g_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) == JNI_OK;
#endif
            if (!attached) env = nullptr;
        }
    }
    ~Env() { if (attached) g_vm->DetachCurrentThread(); }
};

// Calls a hook method returning byte[]; false when there is no hook, it threw, or it returned null.
template <typename... A>
bool callBytes(const char* name, const char* sig, std::vector<unsigned char>& out, A... args) {
    Env e;
    if (!e.env || !g_hooks) return false;
    JNIEnv* env = e.env;
    jclass cls = env->GetObjectClass(g_hooks);
    jmethodID m = env->GetMethodID(cls, name, sig);
    env->DeleteLocalRef(cls);
    if (!m) { env->ExceptionClear(); return false; }
    auto r = static_cast<jbyteArray>(env->CallObjectMethod(g_hooks, m, args...));
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    if (!r) return false;
    out = bytesOf(env, r);
    env->DeleteLocalRef(r);
    return true;
}

void installSystem() {
    crypto::System c;
    c.hash = [](crypto::Hash h, const crypto::Bytes& data, crypto::Bytes& out) {
        Env e;
        if (!e.env) return false;
        jbyteArray a = jbytes(e.env, data);
        const int alg = h == crypto::Hash::Sha256 ? 256 : h == crypto::Hash::Sha384 ? 384 : 512;
        const bool ok = callBytes("hash", "(I[B)[B", out, static_cast<jint>(alg), a);
        e.env->DeleteLocalRef(a);
        return ok;
    };
    c.aes = [](bool cbc, const crypto::Bytes& key, const crypto::Bytes* iv, const crypto::Bytes& in, crypto::Bytes& out) {
        Env e;
        if (!e.env) return false;
        jbyteArray k = jbytes(e.env, key), v = iv ? jbytes(e.env, *iv) : nullptr, d = jbytes(e.env, in);
        const bool ok = callBytes("aes", "(Z[B[B[B)[B", out, static_cast<jboolean>(cbc), k, v, d);
        e.env->DeleteLocalRef(k); if (v) e.env->DeleteLocalRef(v); e.env->DeleteLocalRef(d);
        return ok;
    };
    c.random = [](size_t n, crypto::Bytes& out) { return callBytes("random", "(I)[B", out, static_cast<jint>(n)); };
    crypto::setSystem(c);

    image::System im;
    im.encode = [](const std::vector<unsigned char>& bgra, int w, int h, image::Format f, int quality, bool alpha, std::vector<unsigned char>& out) {
        Env e;
        if (!e.env) return false;
        jbyteArray px = jbytes(e.env, bgra);
        const bool ok = callBytes("encodeImage", "([BIIZIZ)[B", out, px, static_cast<jint>(w), static_cast<jint>(h),
                                  static_cast<jboolean>(f == image::Format::Jpeg), static_cast<jint>(quality), static_cast<jboolean>(alpha));
        e.env->DeleteLocalRef(px);
        return ok;
    };
    im.load = [](const std::wstring& path, image::Picture& out) {
        Env e;
        if (!e.env) return false;
        jstring p = jstr(e.env, path);
        std::vector<unsigned char> b;   // width and height (4 bytes each, big-endian), then BGRA
        const bool ok = callBytes("decodeImage", "(Ljava/lang/String;)[B", b, p);
        e.env->DeleteLocalRef(p);
        if (!ok || b.size() < 8) return false;
        out.w = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
        out.h = (b[4] << 24) | (b[5] << 16) | (b[6] << 8) | b[7];
        if (out.w <= 0 || out.h <= 0 || b.size() != 8 + static_cast<size_t>(out.w) * out.h * 4) return false;
        out.px.assign(b.begin() + 8, b.end());
        return true;
    };
    image::setSystem(im);
}

pagecolours::Mode modeOf(int m) {
    switch (m) {
        case 1: return pagecolours::Mode::Dark;
        case 2: return pagecolours::Mode::Dimmed;
        case 3: return pagecolours::Mode::BlackboardInverted;
        case 4: return pagecolours::Mode::BlackboardOriginal;
        default: return pagecolours::Mode::Normal;
    }
}

} // namespace

#define FN(name) JNICALL Java_com_featherpdf_engine_Engine_##name
#define LOCK std::lock_guard<std::recursive_mutex> guard(g_lock)

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

// Loads PDFium and takes the app's system services. Returns null, or why it could not start.
JNIEXPORT jstring FN(init)(JNIEnv* env, jclass, jobject hooks) {
    LOCK;
    if (g_hooks) { env->DeleteGlobalRef(g_hooks); g_hooks = nullptr; }
    if (hooks) g_hooks = env->NewGlobalRef(hooks);
    installSystem();
    std::wstring err;
    if (!pdfium::load(err)) return jstr(env, err.empty() ? L"The PDF library could not be loaded." : err);
    return nullptr;
}

// Opens a PDF file. result[0] gets the outcome (0 opened, 4 a password is needed, 5 the password is wrong, others: see
// describe()). Returns the document's handle, or 0.
JNIEXPORT jlong FN(open)(JNIEnv* env, jclass, jstring path, jstring password, jintArray result) {
    LOCK;
    auto doc = std::make_unique<Doc>();
    const OpenResult r = doc->d.open(wstr(env, path), utf8(env, password));
    const jint code = static_cast<jint>(r);
    if (result && env->GetArrayLength(result) > 0) env->SetIntArrayRegion(result, 0, 1, &code);
    if (r != OpenResult::Ok) return 0;
    return reinterpret_cast<jlong>(doc.release());
}

JNIEXPORT jstring FN(describe)(JNIEnv* env, jclass, jint result, jstring name) {
    return jstr(env, Document::describe(static_cast<OpenResult>(result), wstr(env, name)));
}

JNIEXPORT void FN(close)(JNIEnv*, jclass, jlong h) {
    LOCK;
    delete docOf(h);
}

JNIEXPORT jint FN(pageCount)(JNIEnv*, jclass, jlong h) {
    LOCK;
    return h ? docOf(h)->d.pageCount() : 0;
}

// Width and height of each page in points, as shown (rotation included).
JNIEXPORT jfloatArray FN(pageSizes)(JNIEnv* env, jclass, jlong h) {
    LOCK;
    std::vector<float> v;
    if (h) for (int i = 0; i < docOf(h)->d.pageCount(); ++i) {
        const PageSize& s = docOf(h)->d.pageSize(i);
        v.push_back(static_cast<float>(s.w));
        v.push_back(static_cast<float>(s.h));
    }
    return floats(env, v);
}

// Draws part of a page into `pixels` (a direct buffer of width x height pixels, `stride` bytes a row, RGBA as Android's
// ARGB_8888 bitmaps hold them). The whole page is sizeX x sizeY pixels with its top left corner at (startX, startY) of
// the buffer (negative: the buffer shows a part further in). colourMode: 0 normal, 1 night, 2 dimmed, 3 blackboard
// (inverted), 4 blackboard (pictures in their own colours).
JNIEXPORT jboolean FN(render)(JNIEnv* env, jclass, jlong h, jint page, jobject pixels, jint width, jint height, jint stride,
                              jint startX, jint startY, jint sizeX, jint sizeY, jint colourMode) {
    LOCK;
    if (!h || page < 0 || page >= docOf(h)->d.pageCount() || width <= 0 || height <= 0) return JNI_FALSE;
    auto* buf = static_cast<unsigned char*>(env->GetDirectBufferAddress(pixels));
    if (!buf || env->GetDirectBufferCapacity(pixels) < static_cast<jlong>(stride) * height) return JNI_FALSE;
    const auto& api = pdfium::api();
    FPDF_BITMAP bmp = api.FPDFBitmap_CreateEx(width, height, FPDFBitmap_BGRA, buf, stride);
    if (!bmp) return JNI_FALSE;
    api.FPDFBitmap_FillRect(bmp, 0, 0, width, height, 0xFFFFFFFF);
    Document& d = docOf(h)->d;
    d.renderPage(page, bmp, startX, startY, sizeX, sizeY, true);
    const pagecolours::Mode m = modeOf(colourMode);
    if (m != pagecolours::Mode::Normal) {
        std::vector<pagecolours::Box> keep;
        if (m == pagecolours::Mode::BlackboardOriginal) keep = pagecolours::pictureBoxes(d, page, startX, startY, sizeX, sizeY);
        pagecolours::apply(m, buf, width, height, stride, keep);
    }
    api.FPDFBitmap_Destroy(bmp);
    for (int y = 0; y < height; ++y) {   // BGRA -> RGBA
        unsigned char* p = buf + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x, p += 4) std::swap(p[0], p[2]);
    }
    return JNI_TRUE;
}

// Page point -> pixel of a page drawn sizeX x sizeY: {x, y} for each pair of `pts`.
JNIEXPORT jfloatArray FN(pageToDevice)(JNIEnv* env, jclass, jlong h, jint page, jint sizeX, jint sizeY, jfloatArray pts) {
    LOCK;
    std::vector<float> out;
    FPDF_PAGE pg = h ? docOf(h)->d.page(page) : nullptr;
    const jsize n = pts ? env->GetArrayLength(pts) : 0;
    std::vector<float> in(static_cast<size_t>(n));
    if (n) env->GetFloatArrayRegion(pts, 0, n, in.data());
    constexpr int S = 16;   // FPDF_PageToDevice gives whole pixels: work at 16 times the size
    for (jsize i = 0; pg && i + 1 < n; i += 2) {
        int dx = 0, dy = 0;
        pdfium::api().FPDF_PageToDevice(pg, 0, 0, sizeX * S, sizeY * S, 0, in[i], in[i + 1], &dx, &dy);
        out.push_back(static_cast<float>(dx) / S);
        out.push_back(static_cast<float>(dy) / S);
    }
    return floats(env, out);
}

JNIEXPORT jfloatArray FN(deviceToPage)(JNIEnv* env, jclass, jlong h, jint page, jint sizeX, jint sizeY, jfloat x, jfloat y) {
    LOCK;
    std::vector<float> out;
    if (FPDF_PAGE pg = h ? docOf(h)->d.page(page) : nullptr) {
        constexpr int S = 16;
        double px = 0, py = 0;
        pdfium::api().FPDF_DeviceToPage(pg, 0, 0, sizeX * S, sizeY * S, 0, static_cast<int>(std::lround(x * S)), static_cast<int>(std::lround(y * S)), &px, &py);
        out = {static_cast<float>(px), static_cast<float>(py)};
    }
    return floats(env, out);
}

// ---- text selection (page points, origin bottom left): the letters between two points, in reading order
namespace {
// The letter at a point, else the nearest one (a finger seldom lands on a letter): distance to its box, a line above or
// below counting three times as far as one beside it. -1 when the page has no text.
int nearestChar(FPDF_TEXTPAGE tp, double x, double y) {
    const auto& api = pdfium::api();
    const int at = api.FPDFText_GetCharIndexAtPos(tp, x, y, 4, 4);
    if (at >= 0) return at;
    int best = -1;
    double bestD = 1e18;
    const int n = api.FPDFText_CountChars(tp);
    for (int i = 0; i < n; ++i) {
        double l, r, b, t;
        if (!api.FPDFText_GetCharBox(tp, i, &l, &r, &b, &t) || r <= l) continue;
        const double dx = x < l ? l - x : x > r ? x - r : 0, dy = y < b ? b - y : y > t ? y - t : 0;
        const double dist = dx * dx + 9 * dy * dy;
        if (dist < bestD) { bestD = dist; best = i; }
    }
    return best;
}

bool charRange(Document& d, int page, float x1, float y1, float x2, float y2, int& a, int& b) {
    FPDF_TEXTPAGE tp = d.text(page);
    if (!tp) return false;
    a = nearestChar(tp, x1, y1);
    b = nearestChar(tp, x2, y2);
    if (a < 0 || b < 0) return false;
    if (a > b) std::swap(a, b);
    return true;
}
} // namespace

JNIEXPORT jfloatArray FN(selectionRects)(JNIEnv* env, jclass, jlong h, jint page, jfloat x1, jfloat y1, jfloat x2, jfloat y2) {
    LOCK;
    std::vector<float> v;
    int a = 0, b = 0;
    if (h && charRange(docOf(h)->d, page, x1, y1, x2, y2, a, b)) {
        FPDF_TEXTPAGE tp = docOf(h)->d.text(page);
        const auto& api = pdfium::api();
        const int n = api.FPDFText_CountRects(tp, a, b - a + 1);
        for (int i = 0; i < n; ++i) {
            double l, t, r, bo;
            if (!api.FPDFText_GetRect(tp, i, &l, &t, &r, &bo)) continue;
            v.insert(v.end(), {static_cast<float>(l), static_cast<float>(bo), static_cast<float>(r), static_cast<float>(t)});
        }
    }
    return floats(env, v);
}

JNIEXPORT jstring FN(selectionText)(JNIEnv* env, jclass, jlong h, jint page, jfloat x1, jfloat y1, jfloat x2, jfloat y2) {
    LOCK;
    int a = 0, b = 0;
    if (!h || !charRange(docOf(h)->d, page, x1, y1, x2, y2, a, b)) return jstr(env, L"");
    return jstr(env, docOf(h)->d.textRange(page, a, b - a + 1));
}

// ---- editing text
// What edit mode outlines: kind (0 text, 1 picture), left, bottom, right, top for each block.
JNIEXPORT jfloatArray FN(editableBlocks)(JNIEnv* env, jclass, jlong h, jint page) {
    LOCK;
    std::vector<float> v;
    if (h) for (const auto& b : pageobj::editableBlocks(docOf(h)->d, page))
        v.insert(v.end(), {b.kind == pageobj::EditBlock::Picture ? 1.f : 0.f, static_cast<float>(b.left), static_cast<float>(b.bottom),
                           static_cast<float>(b.right), static_cast<float>(b.top)});
    return floats(env, v);
}

// The paragraph at a page point; kept for replaceParagraph(). Returns its text (null when there is none); info gets:
// left, top, width, height (points), size, bold, italic, family (0 sans, 1 serif, 2 mono), colour (ARGB), lines, align.
JNIEXPORT jstring FN(paragraphAt)(JNIEnv* env, jclass, jlong h, jint page, jfloat x, jfloat y, jdoubleArray info) {
    LOCK;
    if (!h) return nullptr;
    Doc& doc = *docOf(h);
    doc.hit = pageobj::paragraphAt(doc.d, page, x, y);
    if (!doc.hit.found) return nullptr;
    const auto& p = doc.hit;
    const double v[11] = {p.left, p.top, p.width, p.lineHeight * std::max(1, p.lines), p.style.sizePt, p.style.bold ? 1.0 : 0.0,
                          p.style.italic ? 1.0 : 0.0, static_cast<double>(static_cast<int>(p.style.family)),
                          static_cast<double>(argb(p.style.color)), static_cast<double>(p.lines), static_cast<double>(static_cast<int>(p.align))};
    if (info && env->GetArrayLength(info) >= 11) env->SetDoubleArrayRegion(info, 0, 11, v);
    return jstr(env, p.text);
}

// Sets the paragraph found by the last paragraphAt() again with `text` in this style. Returns null when done, else why not.
JNIEXPORT jstring FN(replaceParagraph)(JNIEnv* env, jclass, jlong h, jstring text, jint family, jboolean bold, jboolean italic,
                                       jdouble size, jint color) {
    LOCK;
    if (!h || !docOf(h)->hit.found) return jstr(env, L"Tap the paragraph again.");
    Doc& doc = *docOf(h);
    pageobj::TextStyle st = doc.hit.style;
    st.family = static_cast<pageobj::Family>(std::clamp(static_cast<int>(family), 0, 2));
    st.bold = bold; st.italic = italic;
    st.sizePt = size > 1 ? size : st.sizePt;
    st.color = colorref(color);
    std::wstring why;
    const bool ok = pageobj::replaceParagraph(doc.d, doc.hit, wstr(env, text), st, doc.hit.align, &why);
    doc.hit = pageobj::ParagraphHit{};   // the page changed: the next edit finds its paragraph again
    if (ok) return nullptr;
    return jstr(env, why.empty() ? L"The paragraph could not be changed." : why);
}

JNIEXPORT jboolean FN(addTextBox)(JNIEnv* env, jclass, jlong h, jint page, jdouble x, jdouble y, jstring text, jdouble size, jint color) {
    LOCK;
    if (!h) return JNI_FALSE;
    pageobj::TextStyle st;
    st.sizePt = size > 1 ? size : 12;
    st.color = colorref(color);
    if (!pageobj::addTextBox(docOf(h)->d, page, x, y, wstr(env, text), st)) return JNI_FALSE;
    docOf(h)->d.resetText(page);   // as the desktop apps do: the page's text is read again (finding, selecting)
    return JNI_TRUE;
}

// ---- annotations
// kind: 0 highlight, 1 underline, 2 strike-out. rects: left, bottom, right, top for each.
JNIEXPORT jboolean FN(addMarkup)(JNIEnv* env, jclass, jlong h, jint page, jint kind, jfloatArray rects, jint color) {
    LOCK;
    if (!h || !rects) return JNI_FALSE;
    const jsize n = env->GetArrayLength(rects);
    std::vector<float> f(static_cast<size_t>(n));
    if (n) env->GetFloatArrayRegion(rects, 0, n, f.data());
    std::vector<annot::RectD> r;
    for (jsize i = 0; i + 3 < n; i += 4) r.push_back({f[i], f[i + 1], f[i + 2], f[i + 3]});
    if (r.empty()) return JNI_FALSE;
    const annot::Markup m = kind == 1 ? annot::Markup::Underline : kind == 2 ? annot::Markup::Strikethrough : annot::Markup::Highlight;
    return annot::addMarkup(docOf(h)->d, page, m, r, colorref(color)) ? JNI_TRUE : JNI_FALSE;
}

// strokes: each a float[] of x, y page points.
JNIEXPORT jboolean FN(addInk)(JNIEnv* env, jclass, jlong h, jint page, jobjectArray strokes, jint color, jfloat width) {
    LOCK;
    if (!h || !strokes) return JNI_FALSE;
    std::vector<std::vector<annot::PointD>> s;
    for (jsize i = 0; i < env->GetArrayLength(strokes); ++i) {
        auto a = static_cast<jfloatArray>(env->GetObjectArrayElement(strokes, i));
        if (!a) continue;
        const jsize n = env->GetArrayLength(a);
        std::vector<float> f(static_cast<size_t>(n));
        if (n) env->GetFloatArrayRegion(a, 0, n, f.data());
        env->DeleteLocalRef(a);
        std::vector<annot::PointD> pts;
        for (jsize k = 0; k + 1 < n; k += 2) pts.push_back({f[k], f[k + 1]});
        if (pts.size() == 1) pts.push_back({pts[0].x + 0.5, pts[0].y});   // a dot
        if (!pts.empty()) s.push_back(std::move(pts));
    }
    if (s.empty()) return JNI_FALSE;
    return annot::addInk(docOf(h)->d, page, s, colorref(color), width) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean FN(addNote)(JNIEnv* env, jclass, jlong h, jint page, jfloat x, jfloat y, jstring text, jint color) {
    LOCK;
    if (!h) return JNI_FALSE;
    return annot::addNote(docOf(h)->d, page, {x, y}, wstr(env, text), colorref(color)) ? JNI_TRUE : JNI_FALSE;
}

// The annotation at a page point (-1: none); kind[0] gets its type name, kind[1] its comment.
JNIEXPORT jint FN(annotAt)(JNIEnv* env, jclass, jlong h, jint page, jfloat x, jfloat y, jobjectArray kind) {
    LOCK;
    if (!h) return -1;
    const int i = annot::hitTest(docOf(h)->d, page, {x, y});
    if (i >= 0 && kind && env->GetArrayLength(kind) >= 2) {
        for (const auto& a : annot::list(docOf(h)->d, page)) {
            if (a.index != i) continue;
            jstring t = jstr(env, annot::typeName(a.subtype)), c = jstr(env, a.contents);
            env->SetObjectArrayElement(kind, 0, t);
            env->SetObjectArrayElement(kind, 1, c);
            break;
        }
    }
    return i;
}

JNIEXPORT jboolean FN(removeAnnot)(JNIEnv*, jclass, jlong h, jint page, jint index) {
    LOCK;
    return h && annot::remove(docOf(h)->d, page, index) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean FN(setAnnotText)(JNIEnv* env, jclass, jlong h, jint page, jint index, jstring text) {
    LOCK;
    return h && annot::setContents(docOf(h)->d, page, index, wstr(env, text)) ? JNI_TRUE : JNI_FALSE;
}

// ---- finding, bookmarks
// Each match: page, left, bottom, right, top.
JNIEXPORT jfloatArray FN(find)(JNIEnv* env, jclass, jlong h, jstring query, jboolean matchCase, jboolean wholeWord) {
    LOCK;
    std::vector<float> v;
    if (h) {
        search::Options o;
        o.matchCase = matchCase; o.wholeWord = wholeWord;
        const auto& api = pdfium::api();
        Document& d = docOf(h)->d;
        for (const auto& t : search::find(d, wstr(env, query), o)) {
            if (t.annot >= 0 || t.count <= 0) continue;
            // a match in the text is its letters: the box around the rectangles PDFium gives for them
            FPDF_TEXTPAGE tp = d.text(t.page);
            if (!tp) continue;
            double l = 1e9, b = 1e9, r = -1e9, top = -1e9;
            const int n = api.FPDFText_CountRects(tp, t.start, t.count);
            for (int i = 0; i < n; ++i) {
                double rl, rt, rr, rb;
                if (!api.FPDFText_GetRect(tp, i, &rl, &rt, &rr, &rb)) continue;
                l = std::min(l, rl); r = std::max(r, rr); b = std::min(b, std::min(rb, rt)); top = std::max(top, std::max(rb, rt));
            }
            if (r < l) continue;
            v.insert(v.end(), {static_cast<float>(t.page), static_cast<float>(l), static_cast<float>(b), static_cast<float>(r), static_cast<float>(top)});
        }
    }
    return floats(env, v);
}

// The bookmarks, depth first: "depth<TAB>page<TAB>title" for each.
JNIEXPORT jobjectArray FN(bookmarks)(JNIEnv* env, jclass, jlong h) {
    LOCK;
    std::vector<std::wstring> rows;
    if (h) {
        std::function<void(const std::vector<Bookmark>&, int)> walk = [&](const std::vector<Bookmark>& v, int depth) {
            for (const auto& b : v) {
                rows.push_back(std::to_wstring(depth) + L"\t" + std::to_wstring(b.page) + L"\t" + b.title);
                if (rows.size() < 5000) walk(b.children, depth + 1);
            }
        };
        walk(docOf(h)->d.bookmarks(), 0);
    }
    jclass str = env->FindClass("java/lang/String");
    jobjectArray a = env->NewObjectArray(static_cast<jsize>(rows.size()), str, nullptr);
    for (size_t i = 0; i < rows.size(); ++i) {
        jstring s = jstr(env, rows[i]);
        env->SetObjectArrayElement(a, static_cast<jsize>(i), s);
        env->DeleteLocalRef(s);
    }
    return a;
}

// ---- pages, history, saving
JNIEXPORT jboolean FN(rotatePages)(JNIEnv* env, jclass, jlong h, jintArray pages, jint quarterTurns) {
    LOCK;
    if (!h || !pages) return JNI_FALSE;
    std::vector<int> p(static_cast<size_t>(env->GetArrayLength(pages)));
    if (!p.empty()) env->GetIntArrayRegion(pages, 0, static_cast<jsize>(p.size()), reinterpret_cast<jint*>(p.data()));
    return docOf(h)->d.rotatePages(p, quarterTurns) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean FN(deletePages)(JNIEnv* env, jclass, jlong h, jintArray pages) {
    LOCK;
    if (!h || !pages) return JNI_FALSE;
    std::vector<int> p(static_cast<size_t>(env->GetArrayLength(pages)));
    if (!p.empty()) env->GetIntArrayRegion(pages, 0, static_cast<jsize>(p.size()), reinterpret_cast<jint*>(p.data()));
    return docOf(h)->d.deletePages(p) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean FN(undo)(JNIEnv*, jclass, jlong h) { LOCK; return h && docOf(h)->d.undo() ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean FN(redo)(JNIEnv*, jclass, jlong h) { LOCK; return h && docOf(h)->d.redo() ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean FN(canUndo)(JNIEnv*, jclass, jlong h) { LOCK; return h && docOf(h)->d.canUndo() ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean FN(canRedo)(JNIEnv*, jclass, jlong h) { LOCK; return h && docOf(h)->d.canRedo() ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean FN(isModified)(JNIEnv*, jclass, jlong h) { LOCK; return h && docOf(h)->d.dirty() ? JNI_TRUE : JNI_FALSE; }

// The document as it is to be saved (null when it could not be written).
JNIEXPORT jbyteArray FN(bytesToSave)(JNIEnv* env, jclass, jlong h) {
    LOCK;
    std::vector<unsigned char> out;
    if (!h || !docOf(h)->d.bytesToSave(out) || out.empty()) return nullptr;
    return jbytes(env, out);
}

// The bytes from bytesToSave() were written to the document's place: it is no longer modified.
JNIEXPORT void FN(markSaved)(JNIEnv*, jclass, jlong h) {
    LOCK;
    if (h) docOf(h)->d.savedAs(docOf(h)->d.path());
}

} // extern "C"
