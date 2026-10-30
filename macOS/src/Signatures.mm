// Signatures on the Mac, as on Windows (Protect > Digital Signatures and Handwritten Signature):
// - signing with a certificate from the keychain or a .p12 / .pfx file, invisibly or with a visible stamp, with or
//   without a trusted time stamp; the signed copy is saved as a new file (shared/sign.cpp writes the PDF part);
// - making one's own certificate (kept in the login keychain);
// - checking the signatures of a file when it opens, with a warning when a signed file was changed;
// - a handwritten signature: typed in a handwriting font, drawn with the mouse or trackpad, or a picture of one.
// The system's part: Security (CMSEncoder, CMSDecoder, keychain), NSURLSession (time stamps).
#import <Cocoa/Cocoa.h>
#import <Security/Security.h>
#import <Security/CMSEncoder.h>
#import <Security/CMSDecoder.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "WindowController.h"
#import "PageView.h"
#import "Form.h"
#import <objc/runtime.h>
#include <memory>
#include <string>
#include <vector>
#include "document.hpp"
#include "platform.hpp"
#include "sign.hpp"

using form::Field;
using form::Fields;
using form::ns;
using form::ws;
using Bytes = std::vector<unsigned char>;

namespace {

std::wstring certName(SecCertificateRef c) {
    CFStringRef s = c ? SecCertificateCopySubjectSummary(c) : nullptr;
    std::wstring out = s ? ws((__bridge NSString*)s) : L"";
    if (s) CFRelease(s);
    return out;
}

std::wstring identityName(SecIdentityRef identity) {
    SecCertificateRef c = nullptr;
    if (SecIdentityCopyCertificate(identity, &c) != errSecSuccess) return L"";
    std::wstring n = certName(c);
    CFRelease(c);
    return n;
}

std::wstring osError(OSStatus st) {
    CFStringRef s = SecCopyErrorMessageString(st, nullptr);
    std::wstring out = s ? ws((__bridge NSString*)s) : L"error " + std::to_wstring(st);
    if (s) CFRelease(s);
    return out;
}

// A keychain identity (certificate and private key) as a signer: macOS makes the CMS signature.
sign::Signer signerFor(SecIdentityRef identity) {
    CFRetain(identity);
    std::shared_ptr<const void> keep(identity, [](const void* p) { CFRelease(p); });
    sign::Signer s;
    s.name = identityName(identity);
    s.cms = [keep](const Bytes& data, Bytes& cms, std::wstring& error) {
        SecIdentityRef id = (SecIdentityRef)keep.get();
        CMSEncoderRef enc = nullptr;
        OSStatus st = CMSEncoderCreate(&enc);
        if (st == errSecSuccess) st = CMSEncoderAddSigners(enc, id);
        if (st == errSecSuccess) st = CMSEncoderSetHasDetachedContent(enc, true);
        if (st == errSecSuccess) st = CMSEncoderSetSignerAlgorithm(enc, kCMSEncoderDigestAlgorithmSHA256);
        if (st == errSecSuccess) st = CMSEncoderAddSignedAttributes(enc, kCMSAttrSigningTime);
        if (st == errSecSuccess) st = CMSEncoderSetCertificateChainMode(enc, kCMSCertificateChainWithRoot);   // checkable elsewhere
        if (st == errSecSuccess) st = CMSEncoderUpdateContent(enc, data.data(), data.size());
        CFDataRef out = nullptr;
        if (st == errSecSuccess) st = CMSEncoderCopyEncodedContent(enc, &out);
        if (enc) CFRelease(enc);
        if (st != errSecSuccess || !out) {
            error = st == errSecAuthFailed || st == errSecInteractionNotAllowed
                        ? L"macOS did not allow the certificate's private key to be used. Allow Feather PDF when the keychain asks."
                        : L"macOS could not make the signature (" + osError(st) + L").";
            return false;
        }
        cms.assign(CFDataGetBytePtr(out), CFDataGetBytePtr(out) + CFDataGetLength(out));
        CFRelease(out);
        return true;
    };
    return s;
}

// 2 valid and trusted on this Mac, 1 valid but not trusted, 0 not matching.
int verifySignature(const Bytes& cms, const Bytes& data, std::wstring& signer) {
    CMSDecoderRef dec = nullptr;
    if (CMSDecoderCreate(&dec) != errSecSuccess) return -1;
    int verdict = 0;
    CFDataRef detached = CFDataCreate(nullptr, data.data(), static_cast<CFIndex>(data.size()));
    size_t signers = 0;
    if (CMSDecoderUpdateMessage(dec, cms.data(), cms.size()) == errSecSuccess && CMSDecoderSetDetachedContent(dec, detached) == errSecSuccess &&
        CMSDecoderFinalizeMessage(dec) == errSecSuccess && CMSDecoderGetNumSigners(dec, &signers) == errSecSuccess && signers > 0) {
        SecPolicyRef policy = SecPolicyCreateBasicX509();
        CMSSignerStatus status = kCMSSignerUnsigned;
        OSStatus certResult = 0;
        if (CMSDecoderCopySignerStatus(dec, 0, policy, true, &status, nullptr, &certResult) == errSecSuccess) {
            if (status == kCMSSignerValid) verdict = 2;
            else if (status == kCMSSignerInvalidCert) verdict = 1;   // the content matches; the certificate is not trusted here
        }
        CFRelease(policy);
        SecCertificateRef c = nullptr;
        if (CMSDecoderCopySignerCert(dec, 0, &c) == errSecSuccess && c) {
            signer = certName(c);
            CFRelease(c);
        }
    }
    CFRelease(detached);
    CFRelease(dec);
    return verdict;
}

bool postTimestampRequest(const std::string& url, const Bytes& body, Bytes& response, std::wstring& error) {
    NSURL* u = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
    if (!u) { error = L"The time stamp address is not valid."; return false; }
    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:u cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:20];
    req.HTTPMethod = @"POST";
    [req setValue:@"application/timestamp-query" forHTTPHeaderField:@"Content-Type"];
    [req setValue:@"application/timestamp-reply" forHTTPHeaderField:@"Accept"];
    req.HTTPBody = [NSData dataWithBytes:body.data() length:body.size()];
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block NSData* got = nil;
    __block NSInteger status = 0;
    __block BOOL failed = NO;
    [[NSURLSession.sharedSession dataTaskWithRequest:req completionHandler:^(NSData* d, NSURLResponse* r, NSError* e) {
        got = d;
        status = [r isKindOfClass:[NSHTTPURLResponse class]] ? ((NSHTTPURLResponse*)r).statusCode : 0;
        failed = e != nil;
        dispatch_semaphore_signal(done);
    }] resume];
    if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 30 * NSEC_PER_SEC)) != 0 || failed) {
        error = L"The time stamp server could not be reached. Check the internet connection, or sign without a time stamp.";
        return false;
    }
    if (status != 200 || !got.length) { error = L"The time stamp server answered with an error (" + std::to_wstring(status) + L")."; return false; }
    response.assign(static_cast<const unsigned char*>(got.bytes), static_cast<const unsigned char*>(got.bytes) + got.length);
    return true;
}

bool signerFromFile(const std::wstring& path, const std::wstring& password, sign::Signer& signer, std::wstring& error) {
    NSData* data = [NSData dataWithContentsOfFile:ns(path)];
    if (!data) { error = L"The certificate file could not be read."; return false; }
    CFArrayRef items = nullptr;
    NSDictionary* opts = @{(__bridge id)kSecImportExportPassphrase : ns(password)};
    const OSStatus st = SecPKCS12Import((__bridge CFDataRef)data, (__bridge CFDictionaryRef)opts, &items);
    if (st == errSecAuthFailed || st == errSecPkcs12VerifyFailure) { error = L"The password of the certificate file is not right."; return false; }
    if (st != errSecSuccess || !items || CFArrayGetCount(items) == 0) {
        if (items) CFRelease(items);
        error = L"The certificate file could not be opened (" + osError(st) + L").";
        return false;
    }
    NSDictionary* first = (__bridge NSDictionary*)CFArrayGetValueAtIndex(items, 0);
    SecIdentityRef identity = (__bridge SecIdentityRef)first[(__bridge id)kSecImportItemIdentity];
    const bool ok = identity != nullptr;
    if (ok) signer = signerFor(identity);
    else error = L"The file has no certificate with a key that can sign.";
    CFRelease(items);
    return ok;
}

// The identities (certificate with private key) in the keychains, with their names.
std::vector<std::pair<std::wstring, SecIdentityRef>> keychainIdentities() {
    std::vector<std::pair<std::wstring, SecIdentityRef>> out;
    CFTypeRef result = nullptr;
    NSDictionary* q = @{(__bridge id)kSecClass : (__bridge id)kSecClassIdentity, (__bridge id)kSecMatchLimit : (__bridge id)kSecMatchLimitAll,
                        (__bridge id)kSecReturnRef : @YES};
    if (SecItemCopyMatching((__bridge CFDictionaryRef)q, &result) != errSecSuccess || !result) return out;
    for (id item in (__bridge NSArray*)result) {
        SecIdentityRef identity = (__bridge SecIdentityRef)item;
        CFRetain(identity);
        std::wstring n = identityName(identity);
        out.push_back({n.empty() ? L"(no name)" : n, identity});
    }
    CFRelease(result);
    return out;
}

// ---- making one's own certificate: an RSA 2048 key in the keychain, and a self-signed X.509 certificate for it ----

Bytes derLen(size_t n) {
    if (n < 0x80) return {static_cast<unsigned char>(n)};
    Bytes b;
    for (size_t v = n; v; v >>= 8) b.insert(b.begin(), static_cast<unsigned char>(v & 0xFF));
    b.insert(b.begin(), static_cast<unsigned char>(0x80 | b.size()));
    return b;
}
Bytes tlv(unsigned char tag, const Bytes& content) {
    Bytes out{tag};
    const Bytes l = derLen(content.size());
    out.insert(out.end(), l.begin(), l.end());
    out.insert(out.end(), content.begin(), content.end());
    return out;
}
Bytes cat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}
Bytes oid(std::initializer_list<unsigned char> v) { return tlv(0x06, Bytes(v)); }
Bytes utf8(const std::wstring& s) { const std::string u = platform::utf8(s); return Bytes(u.begin(), u.end()); }
Bytes timeOf(NSDate* d) {   // UTCTime YYMMDDHHMMSSZ (before 2050), else GeneralizedTime
    NSDateFormatter* f = [[NSDateFormatter alloc] init];
    f.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
    f.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    NSCalendar* cal = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian];
    const bool utc = [cal component:NSCalendarUnitYear fromDate:d] < 2050;
    f.dateFormat = utc ? @"yyMMddHHmmss'Z'" : @"yyyyMMddHHmmss'Z'";
    const std::string s = [f stringFromDate:d].UTF8String;
    return tlv(utc ? 0x17 : 0x18, Bytes(s.begin(), s.end()));
}

bool makeOwnCertificate(const std::wstring& name, const std::wstring& email, std::wstring& error) {
    NSString* label = [NSString stringWithFormat:@"Feather PDF signing key (%@)", ns(name)];
    NSDictionary* keyAttrs = @{(__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeRSA, (__bridge id)kSecAttrKeySizeInBits : @2048,
                               (__bridge id)kSecPrivateKeyAttrs : @{(__bridge id)kSecAttrIsPermanent : @YES, (__bridge id)kSecAttrLabel : label}};
    CFErrorRef cfErr = nullptr;
    SecKeyRef priv = SecKeyCreateRandomKey((__bridge CFDictionaryRef)keyAttrs, &cfErr);
    if (!priv) {
        error = L"The key could not be made in the keychain" + (cfErr ? L" (" + ws(((__bridge NSError*)cfErr).localizedDescription) + L")." : std::wstring(L"."));
        if (cfErr) CFRelease(cfErr);
        return false;
    }
    SecKeyRef pub = SecKeyCopyPublicKey(priv);
    CFDataRef pubData = pub ? SecKeyCopyExternalRepresentation(pub, nullptr) : nullptr;   // PKCS#1 RSAPublicKey
    if (pub) CFRelease(pub);
    if (!pubData) { CFRelease(priv); error = L"The public key could not be read."; return false; }
    Bytes pkcs1(CFDataGetBytePtr(pubData), CFDataGetBytePtr(pubData) + CFDataGetLength(pubData));
    CFRelease(pubData);

    const Bytes rsaAlg = tlv(0x30, cat({oid({0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01}), Bytes{0x05, 0x00}}));
    const Bytes sha256Rsa = tlv(0x30, cat({oid({0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x0B}), Bytes{0x05, 0x00}}));
    Bytes bits{0x00};
    bits.insert(bits.end(), pkcs1.begin(), pkcs1.end());
    const Bytes spki = tlv(0x30, cat({rsaAlg, tlv(0x03, bits)}));
    Bytes rdn = tlv(0x31, tlv(0x30, cat({oid({0x55, 0x04, 0x03}), tlv(0x0C, utf8(name))})));   // CN
    if (!email.empty()) {
        const std::string e = platform::utf8(email);
        rdn = cat({rdn, tlv(0x31, tlv(0x30, cat({oid({0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x09, 0x01}), tlv(0x16, Bytes(e.begin(), e.end()))})))});
    }
    const Bytes dn = tlv(0x30, rdn);
    Bytes serial(16);
    if (SecRandomCopyBytes(kSecRandomDefault, serial.size(), serial.data()) != errSecSuccess) serial.assign(16, 0x42);
    serial[0] = (serial[0] & 0x7F) | 0x01;
    NSDate* now = [NSDate date];
    const Bytes validity = tlv(0x30, cat({timeOf([now dateByAddingTimeInterval:-86400]), timeOf([now dateByAddingTimeInterval:5 * 365.25 * 86400])}));
    // extensions: key usage digitalSignature + nonRepudiation (critical), extended key usage e-mail protection and document signing
    const Bytes keyUsage = tlv(0x30, cat({oid({0x55, 0x1D, 0x0F}), Bytes{0x01, 0x01, 0xFF}, tlv(0x04, Bytes{0x03, 0x02, 0x06, 0xC0})}));
    const Bytes eku = tlv(0x30, cat({oid({0x55, 0x1D, 0x25}),
                                     tlv(0x04, tlv(0x30, cat({oid({0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x04}),
                                                              oid({0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x0A, 0x03, 0x0C})})))}));
    const Bytes basic = tlv(0x30, cat({oid({0x55, 0x1D, 0x13}), tlv(0x04, tlv(0x30, Bytes{}))}));
    const Bytes extensions = tlv(0xA3, tlv(0x30, cat({keyUsage, eku, basic})));
    const Bytes tbs = tlv(0x30, cat({tlv(0xA0, Bytes{0x02, 0x01, 0x02}), tlv(0x02, serial), sha256Rsa, dn, validity, dn, spki, extensions}));

    CFDataRef tbsData = CFDataCreate(nullptr, tbs.data(), static_cast<CFIndex>(tbs.size()));
    CFDataRef sig = SecKeyCreateSignature(priv, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, tbsData, &cfErr);
    CFRelease(tbsData);
    CFRelease(priv);
    if (!sig) {
        error = L"The certificate could not be signed with its key.";
        if (cfErr) CFRelease(cfErr);
        return false;
    }
    Bytes sigBits{0x00};
    sigBits.insert(sigBits.end(), CFDataGetBytePtr(sig), CFDataGetBytePtr(sig) + CFDataGetLength(sig));
    CFRelease(sig);
    const Bytes der = tlv(0x30, cat({tbs, sha256Rsa, tlv(0x03, sigBits)}));
    CFDataRef certData = CFDataCreate(nullptr, der.data(), static_cast<CFIndex>(der.size()));
    SecCertificateRef cert = SecCertificateCreateWithData(nullptr, certData);
    CFRelease(certData);
    if (!cert) { error = L"The certificate could not be made."; return false; }
    const OSStatus st = SecItemAdd((__bridge CFDictionaryRef)@{(__bridge id)kSecClass : (__bridge id)kSecClassCertificate,
                                                               (__bridge id)kSecValueRef : (__bridge id)cert}, nullptr);
    CFRelease(cert);
    if (st != errSecSuccess && st != errSecDuplicateItem) { error = L"The certificate could not be kept in the keychain (" + osError(st) + L")."; return false; }
    return true;
}

std::wstring sigSummary(const std::vector<sign::Status>& v) {
    std::wstring t;
    int n = 0;
    for (const auto& st : v) {
        ++n;
        const wchar_t* head = st.kind == sign::Status::Valid ? L"Valid" : st.kind == sign::Status::ValidUntrusted ? L"Signed, but not confirmed"
                            : st.kind == sign::Status::Changed ? L"NOT VALID: changed after signing" : L"Could not be checked";
        t += L"Signature " + std::to_wstring(n) + L": " + head + L"\n";
        t += L"  Signed by: " + (st.signer.empty() ? L"(unknown)" : st.signer) + L"\n";
        if (!st.time.empty()) t += L"  Time: " + st.time + L"\n";
        if (!st.reason.empty()) t += L"  Reason: " + st.reason + L"\n";
        t += L"  " + st.note + L"\n\n";
    }
    return t;
}

std::wstring signaturePng() { return platform::appDataDir() + L"/signature.png"; }

bool savePng(NSImage* img, const std::wstring& path) {
    CGImageRef cg = [img CGImageForProposedRect:nullptr context:nil hints:nil];
    if (!cg) return false;
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:cg];
    return [[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] writeToFile:ns(path) atomically:YES];
}

// The name in a handwriting font, dark blue ink, on a transparent picture.
bool typedSignature(const std::wstring& name, const std::wstring& path) {
    NSFont* font = nil;
    for (NSString* f in @[ @"SnellRoundhand", @"Zapfino", @"BradleyHandITCTT-Bold", @"Noteworthy-Light", @"Chalkboard" ])
        if ((font = [NSFont fontWithName:f size:72])) break;
    if (!font) font = [NSFont systemFontOfSize:72];
    NSDictionary* attrs = @{NSFontAttributeName : font, NSForegroundColorAttributeName : [NSColor colorWithSRGBRed:0.08 green:0.12 blue:0.45 alpha:1]};
    NSString* text = ns(name);
    const NSSize size = [text sizeWithAttributes:attrs];
    NSImage* img = [NSImage imageWithSize:NSMakeSize(std::ceil(size.width) + 32, std::ceil(size.height) + 16) flipped:NO drawingHandler:^BOOL(NSRect) {
        [text drawAtPoint:NSMakePoint(16, 8) withAttributes:attrs];
        return YES;
    }];
    return savePng(img, path);
}

} // namespace

void FPInstallSignSystem() {
    sign::System s;
    s.httpPost = postTimestampRequest;
    s.verify = verifySignature;
    s.signerFromFile = signerFromFile;
    sign::setSystem(s);
}

// The drawing pad: draw with the mouse or trackpad.
@interface FPSignaturePad : NSView
@property(nonatomic, strong) NSMutableArray<NSBezierPath*>* strokes;
@end
@implementation FPSignaturePad
- (instancetype)initWithFrame:(NSRect)r {
    if ((self = [super initWithFrame:r])) _strokes = [NSMutableArray array];
    return self;
}
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty {
    [NSColor.whiteColor setFill];
    NSRectFill(self.bounds);
    [[NSColor colorWithWhite:0.8 alpha:1] setStroke];
    NSBezierPath* line = [NSBezierPath bezierPath];
    [line moveToPoint:NSMakePoint(20, self.bounds.size.height - 40)];
    [line lineToPoint:NSMakePoint(self.bounds.size.width - 20, self.bounds.size.height - 40)];
    [line stroke];
    [self drawInk];
}
- (void)drawInk {
    [[NSColor colorWithSRGBRed:0.08 green:0.12 blue:0.45 alpha:1] setStroke];
    for (NSBezierPath* p in _strokes) [p stroke];
}
- (void)mouseDown:(NSEvent*)e {
    NSBezierPath* p = [NSBezierPath bezierPath];
    p.lineWidth = 3;
    p.lineCapStyle = NSLineCapStyleRound;
    p.lineJoinStyle = NSLineJoinStyleRound;
    [p moveToPoint:[self convertPoint:e.locationInWindow fromView:nil]];
    [_strokes addObject:p];
}
- (void)mouseDragged:(NSEvent*)e {
    [_strokes.lastObject lineToPoint:[self convertPoint:e.locationInWindow fromView:nil]];
    [self setNeedsDisplay:YES];
}
- (void)clear { [_strokes removeAllObjects]; [self setNeedsDisplay:YES]; }
// The ink only, cut to its bounds, on a transparent picture.
- (NSImage*)ink {
    if (!_strokes.count) return nil;
    NSRect box = NSZeroRect;
    for (NSBezierPath* p in _strokes) box = NSIsEmptyRect(box) ? p.bounds : NSUnionRect(box, p.bounds);
    box = NSInsetRect(box, -6, -6);
    const CGFloat scale = 2;
    return [NSImage imageWithSize:NSMakeSize(box.size.width * scale, box.size.height * scale) flipped:YES drawingHandler:^BOOL(NSRect) {
        NSAffineTransform* t = [NSAffineTransform transform];
        [t scaleBy:scale];
        [t translateXBy:-box.origin.x yBy:-box.origin.y];
        [t concat];
        [self drawInk];
        return YES;
    }];
}
@end

@interface NSControl (FeatherRun)
- (void)featherRun:(id)sender;   // TableSheet.mm
@end

@interface FPWindowController (Editing)
- (void)placePictureFile:(const std::wstring&)path width:(double)widthPt note:(const std::wstring&)note;
@end

@interface FPWindowController (Signatures)
@end

@implementation FPWindowController (Signatures)

- (void)sigWarn:(const std::wstring&)t { form::message(self.window, t, true); }
- (void)sigNote:(const std::wstring&)t { form::message(self.window, t); }

- (void)warnIfSignatureBroken {
    Document* d = self.pdf;
    if (!d) return;
    for (const sign::Status& s : d->signatures())
        if (s.kind == sign::Status::Changed || s.kind == sign::Status::Unreadable) {
            [self sigWarn:L"A digital signature in this document does not match: the document was changed after it was signed (or the signature is damaged). "
                          L"Do not trust its content. Protect > Digital Signatures > Show Signatures gives the details."];
            return;
        }
}

- (IBAction)showSignatures:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    Fields f(1);
    f[0] = {Field::Note, L"", {}, d->signatures().empty() ? L"This document has no digital signatures." : sigSummary(d->signatures())};
    form::show(self.window, L"Digital signatures", f, L"!Close", nullptr, nullptr);
}

- (IBAction)makeOwnCertificate:(id)sender {
    Fields f(3);
    f[0] = {Field::Note, L"", {}, L"Makes a certificate for signing, kept in your login keychain. Other people see your signature as valid, but they must choose to trust it, because no authority vouches for it."};
    f[1] = {Field::Text, L"Your name:", {}, ws(NSFullUserName())};
    f[2] = {Field::Text, L"E-mail (optional):", {}, L""};
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Make my own certificate", f, L"Make", [](const Fields& v) -> std::wstring {
        return v[1].text.empty() ? L"Type your name as it should appear in your signatures." : L"";
    }, [weak](const Fields& v) {
        std::wstring err;
        if (makeOwnCertificate(v[1].text, v[2].text, err)) [weak sigNote:L"The certificate was made and kept in your login keychain. You can choose it when you sign."];
        else [weak sigWarn:err];
    });
}

- (IBAction)signDocument:(id)sender {
    Document* d = self.pdf;
    if (!d) return;
    if (d->isProtected()) {
        [self sigNote:L"Remove the password protection first (Protect > Remove Password Protection), then sign. A signed file cannot be changed afterwards."];
        return;
    }
    Fields f(7);
    f[0] = {Field::Note, L"", {}, L"Signs the document with a certificate. The signed copy is saved as a new file, and any change made to it afterwards shows up as an invalid signature."};
    f[1] = {Field::Choice, L"Certificate:", {L"One of my certificates (keychain)", L"A certificate file (.p12 / .pfx)"}, L"", 0};
    f[2] = {Field::Text, L"Reason (optional):", {}, L""};
    f[3] = {Field::Text, L"Place (optional):", {}, L""};
    f[4] = {Field::Choice, L"Signature on the page:", {L"Invisible", L"Visible stamp, bottom right", L"Visible stamp, bottom left", L"Visible stamp, top right", L"Visible stamp, top left"}, L"", 0};
    f[5] = {Field::Choice, L"Time stamp (proves when you signed):", {L"None", L"DigiCert", L"Sectigo", L"GlobalSign", L"Another server (type the address below)"}, L"", 0};
    f[6] = {Field::Text, L"Time stamp server address:", {}, L"http://"};
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Sign with a certificate", f, L"Choose Certificate…", [](const Fields& v) -> std::wstring {
        if (v[5].choice == 4 && v[6].text.rfind(L"http", 0) != 0) return L"Type the address of the time stamp server, starting with http:// or https://.";
        return L"";
    }, [weak, d](const Fields& v) {
        FPWindowController* me = weak;
        if (!me) return;
        sign::Options opt;
        opt.reason = v[2].text;
        opt.location = v[3].text;
        if (v[4].choice > 0) {
            const int page = [me.pageView currentPage];
            double cl, cb, cr, ct;
            if (d->cropBoxOf(page, cl, cb, cr, ct)) {
                const double w = 200, h = 60, m = 36;
                const bool right = v[4].choice == 1 || v[4].choice == 3, bottom = v[4].choice == 1 || v[4].choice == 2;
                opt.visible = true;
                opt.page = page;
                opt.left = right ? cr - m - w : cl + m;
                opt.right = opt.left + w;
                opt.bottom = bottom ? cb + m : ct - m - h;
                opt.top = opt.bottom + h;
            }
        }
        static const wchar_t* servers[] = {L"", L"http://timestamp.digicert.com", L"http://timestamp.sectigo.com", L"http://timestamp.globalsign.com/tsa/r6advanced1"};
        const std::wstring url = v[5].choice == 4 ? v[6].text : servers[std::clamp(v[5].choice, 0, 3)];
        opt.timestampUrl.assign(url.begin(), url.end());
        const bool fromKeychain = v[1].choice == 0;   // a copy: the form's answers are gone when the block runs
        dispatch_async(dispatch_get_main_queue(), ^{
            if (fromKeychain) [weak chooseKeychainSignerThen:opt];
            else [weak chooseFileSignerThen:opt];
        });
    });
}

- (void)chooseKeychainSignerThen:(sign::Options)opt {
    auto ids = std::make_shared<std::vector<std::pair<std::wstring, SecIdentityRef>>>(keychainIdentities());
    if (ids->empty()) {
        [self sigNote:L"You have no certificates with a private key yet. Use \"Make My Own Certificate\" first, or sign with a certificate file."];
        return;
    }
    std::vector<std::wstring> names;
    for (const auto& i : *ids) names.push_back(i.first);
    Fields f(1);
    f[0] = {Field::Choice, L"Sign as:", names, L"", 0};
    __weak FPWindowController* weak = self;
    auto release = [ids] { for (auto& i : *ids) CFRelease(i.second); ids->clear(); };
    form::show(self.window, L"Choose the certificate", f, L"Sign…", nullptr, [weak, ids, opt, release](const Fields& v) {
        const size_t i = static_cast<size_t>(std::clamp(v[0].choice, 0, static_cast<int>(ids->size()) - 1));
        sign::Signer s = signerFor((*ids)[i].second);
        release();
        dispatch_async(dispatch_get_main_queue(), ^{ [weak saveSignedWith:s options:opt]; });
    });
}

- (void)chooseFileSignerThen:(sign::Options)opt {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedFileTypes = @[ @"p12", @"pfx" ];
    p.title = @"Certificate with key";
    __weak FPWindowController* weak = self;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        const std::wstring path = platform::wide(p.URL.fileSystemRepresentation);
        dispatch_async(dispatch_get_main_queue(), ^{
            Fields f(1);
            f[0] = {Field::Password, L"Password of the certificate file:", {}, L""};
            FPWindowController* me = weak;
            if (!me) return;
            form::show(me.window, L"Certificate password", f, L"OK", nullptr, [weak, path, opt](const Fields& v) {
                sign::Signer s;
                std::wstring err;
                if (!signerFromFile(path, v[0].text, s, err)) { [weak sigWarn:err]; return; }
                dispatch_async(dispatch_get_main_queue(), ^{ [weak saveSignedWith:s options:opt]; });
            });
        });
    }];
}

- (void)saveSignedWith:(sign::Signer)signer options:(sign::Options)opt {
    Document* d = self.pdf;
    if (!d) return;
    NSSavePanel* p = [NSSavePanel savePanel];
    p.allowedContentTypes = @[ UTTypePDF ];
    NSString* base = [(NSDocument*)self.document displayName].stringByDeletingPathExtension;
    p.nameFieldStringValue = [NSString stringWithFormat:@"%@ (signed).pdf", base];
    p.title = @"Save the signed document as";
    __weak FPWindowController* weak = self;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        FPWindowController* me = weak;
        if (!me) return;
        NSURL* url = p.URL;
        for (NSDocument* doc in NSDocumentController.sharedDocumentController.documents)
            if ([doc.fileURL isEqual:url]) { [me sigWarn:L"That file is open in a window. Close it first, or choose another name."]; return; }
        Bytes plain, signedBytes;
        std::wstring err;
        [NSCursor.operationNotAllowedCursor set];
        const bool ok = d->serialize(plain, FPDF_REMOVE_SECURITY) && sign::signPdf(plain, signer, opt, signedBytes, err);
        [NSCursor.arrowCursor set];
        if (!ok) { [me sigWarn:err.empty() ? L"The document could not be signed." : err]; return; }
        const SaveResult sr = Document::writeFile(platform::wide(url.fileSystemRepresentation), signedBytes);
        if (sr != SaveResult::Ok) {
            [me sigWarn:sr == SaveResult::DiskFull ? L"The signed document could not be saved because the disk is full."
                                                    : L"The signed document could not be saved. The folder may be read-only."];
            return;
        }
        NSAlert* a = [[NSAlert alloc] init];
        a.messageText = @"The document was signed and saved.";
        a.informativeText = @"Open the signed copy?";
        [a addButtonWithTitle:@"Open"];
        [a addButtonWithTitle:@"Not Now"];
        [a beginSheetModalForWindow:me.window completionHandler:^(NSModalResponse resp) {
            if (resp == NSAlertFirstButtonReturn)
                [NSDocumentController.sharedDocumentController openDocumentWithContentsOfURL:url display:YES completionHandler:^(NSDocument*, BOOL, NSError*) {}];
        }];
    }];
}

// ---- handwritten signature: a picture put on the page (for proof of who signed, use a digital signature) ----

- (void)placeHandwritten:(const std::wstring&)png {
    [self placePictureFile:png width:150
                      note:L"Click where the top-left corner of the signature should go (a picture of your handwriting; for proof of who signed, use Digital Signatures)."];
}

- (IBAction)signatureTyped:(id)sender {
    Fields f(1);
    f[0] = {Field::Text, L"Your name as you sign it:", {}, ws(NSFullUserName())};
    __weak FPWindowController* weak = self;
    form::show(self.window, L"Signature", f, L"Make", [](const Fields& v) { return v[0].text.empty() ? std::wstring(L"Type your name.") : std::wstring(); },
               [weak](const Fields& v) {
        std::wstring name = v[0].text.substr(0, 60);
        const std::wstring png = signaturePng();
        if (typedSignature(name, png)) [weak placeHandwritten:png];
        else [weak sigWarn:L"The signature could not be made."];
    });
}

- (IBAction)signatureDrawn:(id)sender {
    NSPanel* panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 560, 290) styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
    NSView* root = panel.contentView;
    NSTextField* title = [NSTextField labelWithString:@"Draw your signature with the mouse or trackpad"];
    title.frame = NSMakeRect(20, 256, 520, 20);
    FPSignaturePad* pad = [[FPSignaturePad alloc] initWithFrame:NSMakeRect(20, 60, 520, 186)];
    pad.wantsLayer = YES;
    pad.layer.borderWidth = 1;
    pad.layer.borderColor = NSColor.separatorColor.CGColor;
    NSButton* clear = [NSButton buttonWithTitle:@"Clear" target:pad action:@selector(clear)];
    clear.frame = NSMakeRect(20, 14, 90, 32);
    NSButton* cancel = [NSButton buttonWithTitle:@"Cancel" target:nil action:nil];
    cancel.frame = NSMakeRect(300, 14, 100, 32);
    cancel.keyEquivalent = @"\033";
    NSButton* use = [NSButton buttonWithTitle:@"Use This Signature" target:nil action:nil];
    use.frame = NSMakeRect(404, 14, 136, 32);
    use.keyEquivalent = @"\r";
    for (NSView* v in @[ title, pad, clear, cancel, use ]) [root addSubview:v];
    __weak FPWindowController* weak = self;
    __weak NSPanel* weakPanel = panel;
    cancel.target = cancel; cancel.action = @selector(featherRun:);
    use.target = use; use.action = @selector(featherRun:);
    objc_setAssociatedObject(cancel, "act", ^{ [weak.window endSheet:weakPanel returnCode:NSModalResponseCancel]; }, OBJC_ASSOCIATION_COPY);
    objc_setAssociatedObject(use, "act", ^{ [weak.window endSheet:weakPanel returnCode:NSModalResponseOK]; }, OBJC_ASSOCIATION_COPY);
    [self.window beginSheet:panel completionHandler:^(NSModalResponse r) {
        if (r != NSModalResponseOK) return;
        NSImage* ink = [pad ink];
        const std::wstring png = signaturePng();
        if (ink && savePng(ink, png)) [weak placeHandwritten:png];
        else if (!ink) [weak sigNote:L"Nothing was drawn."];
        else [weak sigWarn:L"The signature could not be saved."];
    }];
}

- (IBAction)signaturePicture:(id)sender {
    NSOpenPanel* p = [NSOpenPanel openPanel];
    p.allowedContentTypes = @[ UTTypeImage ];
    p.title = @"Choose a picture of your signature";
    __weak FPWindowController* weak = self;
    [p beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse r) {
        if (r == NSModalResponseOK) [weak placeHandwritten:platform::wide(p.URL.fileSystemRepresentation)];
    }];
}

- (IBAction)signatureSaved:(id)sender { [self placeHandwritten:signaturePng()]; }

- (BOOL)validateSignatures:(NSMenuItem*)item {
    if (item.action == @selector(signatureSaved:)) return [NSFileManager.defaultManager fileExistsAtPath:ns(signaturePng())];
    if (item.action == @selector(showSignatures:)) return self.pdf && !self.pdf->signatures().empty();
    return self.pdf != nullptr;
}

@end
