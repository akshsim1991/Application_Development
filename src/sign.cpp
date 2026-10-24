#include "sign.hpp"
#include <algorithm>
#include <cstring>
#include <cwctype>
#include <map>
#include <string>

namespace sign {
namespace {

using Bytes = std::vector<unsigned char>;

std::string hex(const unsigned char* p, size_t n) {
    static const char* d = "0123456789ABCDEF";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) { s += d[p[i] >> 4]; s += d[p[i] & 15]; }
    return s;
}

// A PDF text string for `w`: plain ASCII in parentheses when possible, otherwise UTF-16BE in hex.
std::string pdfText(const std::wstring& w) {
    bool ascii = true;
    for (wchar_t c : w) if (c < 0x20 || c > 0x7E || c == L'(' || c == L')' || c == L'\\') ascii = false;
    if (ascii) return "(" + std::string(w.begin(), w.end()) + ")";
    std::string s = "<FEFF";
    char b[8];
    for (wchar_t c : w) { snprintf(b, sizeof b, "%04X", static_cast<unsigned>(c)); s += b; }
    return s + ">";
}

std::string pdfDate() {
    SYSTEMTIME t; GetSystemTime(&t);
    char b[40];
    snprintf(b, sizeof b, "D:%04d%02d%02d%02d%02d%02d+00'00'", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}

bool isWs(unsigned char c) { return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\f' || c == 0; }

struct Pdf {
    const Bytes& f;
    std::map<long, size_t> offsets;
    std::string trailer;
    size_t xrefPos = 0;
    long size = 0;

    explicit Pdf(const Bytes& file) : f(file) {}

    std::string word(size_t& p) const {
        while (p < f.size() && isWs(f[p])) ++p;
        std::string w;
        while (p < f.size() && !isWs(f[p])) w += static_cast<char>(f[p++]);
        return w;
    }

    bool parse() {
        const std::string tail(f.end() - std::min<size_t>(f.size(), 64), f.end());
        size_t sx = tail.rfind("startxref");
        if (sx == std::string::npos) return false;
        size_t p = f.size() - tail.size() + sx + 9;
        xrefPos = static_cast<size_t>(_atoi64(word(p).c_str()));
        if (xrefPos == 0 || xrefPos >= f.size()) return false;
        p = xrefPos;
        if (word(p) != "xref") return false;
        for (;;) {
            size_t save = p;
            std::string a = word(p);
            if (a == "trailer") { p = save; break; }
            long start = atol(a.c_str()), count = atol(word(p).c_str());
            for (long i = 0; i < count; ++i) {
                std::string off = word(p), gen = word(p), kind = word(p);
                if (kind == "n") offsets[start + i] = static_cast<size_t>(_atoi64(off.c_str()));
            }
        }
        word(p);
        while (p < f.size() && isWs(f[p])) ++p;
        size_t sxp = std::string(f.begin() + p, f.end()).find("startxref");
        trailer.assign(f.begin() + p, f.begin() + p + (sxp == std::string::npos ? f.size() - p : sxp));
        while (!trailer.empty() && isWs(static_cast<unsigned char>(trailer.back()))) trailer.pop_back();
        size_t sp = trailer.find("/Size");
        size = sp == std::string::npos ? 0 : atol(trailer.c_str() + sp + 5);
        return size > 0 && !offsets.empty();
    }

    // The text between "obj" and "endobj" of an object (it must not contain a stream).
    bool body(long num, std::string& out) const {
        auto it = offsets.find(num);
        if (it == offsets.end()) return false;
        size_t p = it->second;
        word(p); word(p);
        if (word(p) != "obj") return false;
        size_t s = p;
        std::string rest(f.begin() + s, f.begin() + std::min(f.size(), s + 200000));
        size_t e = rest.find("endobj");
        if (e == std::string::npos) return false;
        out = rest.substr(0, e);
        while (!out.empty() && isWs(static_cast<unsigned char>(out.back()))) out.pop_back();
        while (!out.empty() && isWs(static_cast<unsigned char>(out.front()))) out.erase(out.begin());
        return true;
    }

    long refAfter(const std::string& text, const char* key) const {   // "/Key N G R" -> N
        size_t k = text.find(key);
        if (k == std::string::npos) return -1;
        const char* c = text.c_str() + k + strlen(key);
        while (*c == ' ') ++c;
        char* end = nullptr;
        long n = strtol(c, &end, 10);
        if (end == c) return -1;
        while (*end == ' ') ++end;
        strtol(end, &end, 10);
        while (*end == ' ') ++end;
        return *end == 'R' ? n : -1;
    }
};

size_t matchClose(const std::string& s, size_t open, char o, char c) {
    int depth = 0;
    for (size_t i = open; i < s.size(); ++i) {
        if (s[i] == o) ++depth;
        else if (s[i] == c && --depth == 0) return i;
    }
    return std::string::npos;
}

// Adds "N 0 R" to the array that follows `key` in `dict` (creating the key when missing). Returns false if the array
// is stored in another object (then `indirectObj` receives that object's number).
bool addToArray(std::string& dict, const char* key, long ref, long& indirectObj) {
    indirectObj = -1;
    const std::string r = " " + std::to_string(ref) + " 0 R";
    size_t k = dict.find(key);
    if (k == std::string::npos) {
        size_t open = dict.find("<<");
        if (open == std::string::npos) return false;
        dict.insert(open + 2, std::string(key) + "[" + std::to_string(ref) + " 0 R]");
        return true;
    }
    size_t v = k + strlen(key);
    while (v < dict.size() && dict[v] == ' ') ++v;
    if (v < dict.size() && dict[v] == '[') {
        size_t close = matchClose(dict, v, '[', ']');
        if (close == std::string::npos) return false;
        dict.insert(close, r);
        return true;
    }
    char* end = nullptr;
    long n = strtol(dict.c_str() + v, &end, 10);
    if (end != dict.c_str() + v) { indirectObj = n; return false; }
    return false;
}

std::string hexOfCert(PCCERT_CONTEXT c) { return hex(c->pbCertEncoded, c->cbCertEncoded); }

} // namespace

std::wstring subjectName(PCCERT_CONTEXT cert) {
    wchar_t name[256]{};
    CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, 256);
    return name;
}

bool signPdf(const Bytes& pdf, PCCERT_CONTEXT cert, const Options& opt, Bytes& out, std::wstring& error) {
    error = L"The document could not be prepared for signing.";
    Pdf in(pdf);
    if (!in.parse()) { error = L"The document could not be prepared for signing (its structure is not supported)."; return false; }

    // --- locate the catalog and the first page ---
    const long rootNum = in.refAfter(in.trailer, "/Root");
    std::string catalog;
    if (rootNum < 0 || !in.body(rootNum, catalog)) return false;
    long pageNum = in.refAfter(catalog, "/Pages");
    std::string pageText;
    for (int depth = 0; depth < 24 && pageNum >= 0; ++depth) {
        if (!in.body(pageNum, pageText)) return false;
        size_t kids = pageText.find("/Kids");
        if (kids == std::string::npos) break;
        size_t open = pageText.find('[', kids);
        if (open == std::string::npos) return false;
        const char* c = pageText.c_str() + open + 1;
        char* end = nullptr;
        long n = strtol(c, &end, 10);
        if (end == c) return false;
        pageNum = n;
    }
    if (pageNum < 0) return false;

    const long sigNum = in.size, fieldNum = in.size + 1, newSize = in.size + 2;
    std::map<long, std::string> newObjects;   // object number -> new text of the whole object body

    // --- page: add the signature widget to /Annots ---
    long ind = -1;
    if (!addToArray(pageText, "/Annots", fieldNum, ind)) {
        if (ind < 0) return false;
        std::string arr;
        if (!in.body(ind, arr)) return false;
        size_t close = arr.rfind(']');
        if (arr.empty() || arr[0] != '[' || close == std::string::npos) return false;
        arr.insert(close, " " + std::to_string(fieldNum) + " 0 R");
        newObjects[ind] = arr;
    } else newObjects[pageNum] = pageText;
    // --- catalog: AcroForm with the field and SigFlags 3 (signatures exist, document is append-only) ---
    size_t af = catalog.find("/AcroForm");
    if (af == std::string::npos) {
        size_t open = catalog.find("<<");
        if (open == std::string::npos) return false;
        catalog.insert(open + 2, "/AcroForm<</Fields[" + std::to_string(fieldNum) + " 0 R]/SigFlags 3>>");
        newObjects[rootNum] = catalog;
    } else {
        size_t v = af + 9;
        while (v < catalog.size() && catalog[v] == ' ') ++v;
        std::string* target = &catalog;
        std::string formObj;
        long formNum = -1;
        size_t dictStart = std::string::npos;
        if (catalog.compare(v, 2, "<<") == 0) dictStart = v;
        else {
            formNum = in.refAfter(catalog, "/AcroForm");
            if (formNum < 0 || !in.body(formNum, formObj)) return false;
            target = &formObj;
            dictStart = formObj.find("<<");
        }
        if (dictStart == std::string::npos) return false;
        size_t dictEnd = matchClose(*target, dictStart, '<', '>');   // matches the first '<' of "<<"
        // matchClose counts single characters, so find the closing ">>" by depth of "<<" pairs
        int depth = 0; dictEnd = std::string::npos;
        for (size_t i = dictStart; i + 1 < target->size(); ++i) {
            if ((*target)[i] == '<' && (*target)[i + 1] == '<') { ++depth; ++i; }
            else if ((*target)[i] == '>' && (*target)[i + 1] == '>') { if (--depth == 0) { dictEnd = i; break; } ++i; }
        }
        if (dictEnd == std::string::npos) return false;
        std::string dict = target->substr(dictStart, dictEnd + 2 - dictStart);
        long ind2 = -1;
        if (!addToArray(dict, "/Fields", fieldNum, ind2)) return false;   // fields stored elsewhere: not handled
        size_t sf = dict.find("/SigFlags");
        if (sf == std::string::npos) dict.insert(2, "/SigFlags 3");
        else {
            size_t e = sf + 9;
            while (e < dict.size() && dict[e] == ' ') ++e;
            while (e < dict.size() && isdigit(static_cast<unsigned char>(dict[e]))) ++e;
            dict.replace(sf, e - sf, "/SigFlags 3");
        }
        target->replace(dictStart, dictEnd + 2 - dictStart, dict);
        if (formNum >= 0) newObjects[formNum] = formObj; else newObjects[rootNum] = catalog;
    }

    // --- the signature dictionary and its widget ---
    const size_t kSigBytes = 14000;   // room for the PKCS#7 blob (certificate chain included)
    const std::string zeros(kSigBytes * 2, '0');
    std::string sigDict = "<</Type/Sig/Filter/Adobe.PPKLite/SubFilter/adbe.pkcs7.detached/ByteRange[0 0000000000 0000000000 0000000000]/Contents<" + zeros + ">"
                          "/M(" + pdfDate() + ")/Name" + pdfText(subjectName(cert));
    if (!opt.reason.empty()) sigDict += "/Reason" + pdfText(opt.reason);
    if (!opt.location.empty()) sigDict += "/Location" + pdfText(opt.location);
    sigDict += ">>";
    newObjects[sigNum] = sigDict;
    newObjects[fieldNum] = "<</Type/Annot/Subtype/Widget/FT/Sig/T(Signature1)/V " + std::to_string(sigNum) + " 0 R/F 132/Rect[0 0 0 0]/P " +
                           std::to_string(pageNum) + " 0 R>>";

    // --- assemble the incremental update ---
    out = pdf;
    std::string tail;
    if (out.empty() || (out.back() != '\n' && out.back() != '\r')) out.push_back('\n');
    std::map<long, size_t> at;
    auto append = [&](const std::string& s) { out.insert(out.end(), s.begin(), s.end()); };
    for (auto& kv : newObjects) {
        at[kv.first] = out.size();
        append(std::to_string(kv.first) + " 0 obj\r\n" + kv.second + "\r\nendobj\r\n");
    }
    const size_t xref = out.size();
    std::string x = "xref\r\n";
    for (auto it = at.begin(); it != at.end();) {
        auto run = it; long first = it->first, count = 0;
        while (run != at.end() && run->first == first + count) { ++run; ++count; }
        x += std::to_string(first) + " " + std::to_string(count) + "\r\n";
        for (auto j = it; j != run; ++j) { char b[32]; snprintf(b, sizeof b, "%010llu 00000 n\r\n", static_cast<unsigned long long>(j->second)); x += b; }
        it = run;
    }
    // the new trailer repeats Root/Info/ID and points back to the previous cross-reference table
    std::string t = "<</Size " + std::to_string(newSize) + "/Root " + std::to_string(rootNum) + " 0 R";
    long infoNum = in.refAfter(in.trailer, "/Info");
    if (infoNum >= 0) t += "/Info " + std::to_string(infoNum) + " 0 R";
    size_t idPos = in.trailer.find("/ID");
    if (idPos != std::string::npos) {
        size_t s = in.trailer.find('[', idPos);
        size_t e = in.trailer.find(']', s);
        if (s != std::string::npos && e != std::string::npos) t += "/ID" + in.trailer.substr(s, e - s + 1);
    }
    t += "/Prev " + std::to_string(in.xrefPos) + ">>";
    x += "trailer\r\n" + t + "\r\nstartxref\r\n" + std::to_string(xref) + "\r\n%%EOF\r\n";
    append(x);

    // --- byte range and signature ---
    const std::string marker = "/Contents<";
    auto found = std::search(out.begin() + static_cast<long>(at[sigNum]), out.end(), marker.begin(), marker.end());
    if (found == out.end()) return false;
    const size_t cStart = static_cast<size_t>(found - out.begin()) + marker.size() - 1;   // the '<'
    const size_t cEnd = cStart + 1 + zeros.size() + 1;                                     // after the '>'
    const std::string brMarker = "/ByteRange[0 0000000000 0000000000 0000000000]";
    auto br = std::search(out.begin() + static_cast<long>(at[sigNum]), out.end(), brMarker.begin(), brMarker.end());
    if (br == out.end()) return false;
    char brText[80];
    snprintf(brText, sizeof brText, "/ByteRange[0 %010llu %010llu %010llu]", static_cast<unsigned long long>(cStart),
             static_cast<unsigned long long>(cEnd), static_cast<unsigned long long>(out.size() - cEnd));
    std::memcpy(&*br, brText, brMarker.size());

    Bytes signedData(out.begin(), out.begin() + static_cast<long>(cStart));
    signedData.insert(signedData.end(), out.begin() + static_cast<long>(cEnd), out.end());

    CRYPT_SIGN_MESSAGE_PARA para{};
    para.cbSize = sizeof(para);
    para.dwMsgEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
    para.pSigningCert = cert;
    para.HashAlgorithm.pszObjId = const_cast<LPSTR>(szOID_NIST_sha256);
    // include the whole certificate chain so the signature can be checked elsewhere
    std::vector<PCCERT_CONTEXT> chainCerts{cert};
    CERT_CHAIN_PARA cp{sizeof(cp)};
    PCCERT_CHAIN_CONTEXT chain = nullptr;
    if (CertGetCertificateChain(nullptr, cert, nullptr, nullptr, &cp, 0, nullptr, &chain) && chain && chain->cChain > 0) {
        PCERT_SIMPLE_CHAIN sc = chain->rgpChain[0];
        for (DWORD i = 1; i < sc->cElement; ++i) chainCerts.push_back(sc->rgpElement[i]->pCertContext);
    }
    para.cMsgCert = static_cast<DWORD>(chainCerts.size());
    para.rgpMsgCert = chainCerts.data();
    const BYTE* dataPtr = signedData.data();
    DWORD dataLen = static_cast<DWORD>(signedData.size());
    DWORD sigLen = 0;
    bool ok = CryptSignMessage(&para, TRUE, 1, &dataPtr, &dataLen, nullptr, &sigLen) != 0;
    if (!ok && GetLastError() == static_cast<DWORD>(NTE_BAD_ALGID)) {
        // Only systems without SHA-256 support in their crypto provider (some emulation layers) end up here.
        para.HashAlgorithm.pszObjId = const_cast<LPSTR>(szOID_OIWSEC_sha1);
        ok = CryptSignMessage(&para, TRUE, 1, &dataPtr, &dataLen, nullptr, &sigLen) != 0;
    }
    Bytes sig(ok ? sigLen : 0);
    if (ok) ok = CryptSignMessage(&para, TRUE, 1, &dataPtr, &dataLen, sig.data(), &sigLen) != 0;
    const DWORD err = GetLastError();
    if (chain) CertFreeCertificateChain(chain);
    if (!ok) {
        error = (err == static_cast<DWORD>(NTE_BAD_KEYSET) || err == static_cast<DWORD>(NTE_NO_KEY) || err == static_cast<DWORD>(CRYPT_E_NO_KEY_PROPERTY))
                    ? L"The certificate's private key could not be used. Choose a certificate that has a private key."
                    : (L"Windows could not make the signature (error " + std::to_wstring(err) + L").");
        return false;
    }
    sig.resize(sigLen);
    if (sig.size() > kSigBytes) { error = L"The certificate chain is too large to fit in the signature."; return false; }
    const std::string h = hex(sig.data(), sig.size());
    std::memcpy(&out[cStart + 1], h.data(), h.size());
    error.clear();
    return true;
}

// ---------- checking ----------

std::vector<Status> check(const Bytes& file) {
    std::vector<Status> results;
    // Find every signature dictionary by its /ByteRange and /Contents.
    const std::string text(file.begin(), file.end());
    size_t pos = 0;
    while ((pos = text.find("/ByteRange", pos)) != std::string::npos) {
        Status st;
        size_t br = text.find('[', pos);
        size_t brEnd = br == std::string::npos ? br : text.find(']', br);
        pos += 10;
        if (br == std::string::npos || brEnd == std::string::npos) continue;
        long long r[4] = {0, 0, 0, 0};
        if (sscanf(text.c_str() + br + 1, "%lld %lld %lld %lld", &r[0], &r[1], &r[2], &r[3]) != 4) continue;
        // the dictionary around this ByteRange
        size_t dictStart = text.rfind("<<", pos);
        size_t cPos = text.find("/Contents", dictStart);
        size_t reasonPos = text.find("/Reason", dictStart);
        if (cPos == std::string::npos) continue;
        size_t hs = text.find('<', cPos + 9), he = hs == std::string::npos ? hs : text.find('>', hs);
        if (hs == std::string::npos || he == std::string::npos) continue;
        Bytes blob;
        int hi = -1;
        for (size_t i = hs + 1; i < he; ++i) {
            unsigned char ch = static_cast<unsigned char>(text[i]);
            int v = isdigit(ch) ? ch - '0' : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : (ch >= 'A' && ch <= 'F') ? ch - 'A' + 10 : -1;
            if (v < 0) continue;
            if (hi < 0) hi = v; else { blob.push_back(static_cast<unsigned char>(hi * 16 + v)); hi = -1; }
        }
        // PKCS#7 blobs are zero-padded to the reserved size: cut at the end of the DER structure.
        if (blob.size() > 4 && blob[0] == 0x30) {
            size_t len = 0, hdr = 2;
            if (blob[1] & 0x80) { size_t n = blob[1] & 0x7F; hdr = 2 + n; for (size_t i = 0; i < n && 2 + i < blob.size(); ++i) len = len * 256 + blob[2 + i]; }
            else len = blob[1];
            if (hdr + len <= blob.size()) blob.resize(hdr + len);
        }
        auto dateOf = [&](const char* key) {
            size_t k = text.find(key, dictStart);
            if (k == std::string::npos || k > he + 400) return std::wstring();
            size_t s = text.find('(', k);
            size_t e = s == std::string::npos ? s : text.find(')', s);
            if (s == std::string::npos || e == std::string::npos || e - s > 40) return std::wstring();
            std::string d = text.substr(s + 1, e - s - 1);
            if (d.size() >= 14 && d.compare(0, 2, "D:") == 0) return std::wstring(d.begin() + 2, d.begin() + 6) + L"-" + std::wstring(d.begin() + 6, d.begin() + 8) + L"-" +
                                                                       std::wstring(d.begin() + 8, d.begin() + 10) + L" " + std::wstring(d.begin() + 10, d.begin() + 12) + L":" + std::wstring(d.begin() + 12, d.begin() + 14);
            return std::wstring(d.begin(), d.end());
        };
        st.time = dateOf("/M");
        if (reasonPos != std::string::npos && reasonPos < he + 400) {
            size_t s = text.find('(', reasonPos), e = s == std::string::npos ? s : text.find(')', s);
            if (s != std::string::npos && e != std::string::npos && e - s < 200) { std::string rs = text.substr(s + 1, e - s - 1); st.reason.assign(rs.begin(), rs.end()); }
        }
        // sanity of the byte range: it must leave out exactly the hex string
        const bool rangeOk = r[0] == 0 && r[1] >= 0 && r[2] > r[1] && static_cast<unsigned long long>(r[2] + r[3]) <= file.size() &&
                             static_cast<size_t>(r[1]) == hs && static_cast<size_t>(r[2]) == he + 1;
        if (!rangeOk || blob.empty()) { st.kind = Status::Unreadable; st.note = L"The signature is incomplete or damaged."; results.push_back(st); continue; }
        Bytes data(file.begin(), file.begin() + r[1]);
        data.insert(data.end(), file.begin() + r[2], file.begin() + r[2] + r[3]);
        const bool coversAll = static_cast<size_t>(r[2] + r[3]) == file.size();

        CRYPT_VERIFY_MESSAGE_PARA vp{};
        vp.cbSize = sizeof(vp);
        vp.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
        const BYTE* dp = data.data();
        DWORD dl = static_cast<DWORD>(data.size());
        PCCERT_CONTEXT signer = nullptr;
        const bool valid = CryptVerifyDetachedMessageSignature(&vp, 0, blob.data(), static_cast<DWORD>(blob.size()), 1, &dp, &dl, &signer) != 0;
        if (signer) st.signer = subjectName(signer);
        if (!valid) {
            st.kind = Status::Changed;
            st.note = L"The signature does not match: the document was changed after it was signed. Do not trust its content.";
        } else {
            // is the certificate trusted on this PC?
            bool trusted = false;
            if (signer) {
                CERT_CHAIN_PARA cp{sizeof(cp)};
                PCCERT_CHAIN_CONTEXT chain = nullptr;
                if (CertGetCertificateChain(nullptr, signer, nullptr, nullptr, &cp, 0, nullptr, &chain) && chain) {
                    CERT_CHAIN_POLICY_PARA pp{sizeof(pp)};
                    CERT_CHAIN_POLICY_STATUS ps{sizeof(ps)};
                    trusted = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, chain, &pp, &ps) && ps.dwError == 0;
                    CertFreeCertificateChain(chain);
                }
            }
            st.kind = trusted ? Status::Valid : Status::ValidUntrusted;
            st.note = trusted ? L"The signature is valid and the document has not been changed since."
                              : L"The document has not been changed since it was signed, but the certificate was made by the signer or by an authority Windows does not know. Ask the sender if you need to be sure who signed.";
            if (!coversAll) st.note += L" Pages or comments were added after the signature.";
        }
        if (signer) CertFreeCertificateContext(signer);
        results.push_back(st);
    }
    return results;
}

// ---------- certificates ----------

PCCERT_CONTEXT certFromPfx(const std::wstring& path, const std::wstring& password, HCERTSTORE& store, std::wstring& error) {
    store = nullptr;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { error = L"The certificate file could not be read."; return nullptr; }
    LARGE_INTEGER sz{};
    GetFileSizeEx(f, &sz);
    Bytes data(static_cast<size_t>(std::min<long long>(sz.QuadPart, 16 << 20)));
    DWORD got = 0;
    ReadFile(f, data.data(), static_cast<DWORD>(data.size()), &got, nullptr);
    CloseHandle(f);
    CRYPT_DATA_BLOB blob{static_cast<DWORD>(got), data.data()};
    store = PFXImportCertStore(&blob, password.c_str(), PKCS12_NO_PERSIST_KEY);
    if (!store) {
        error = GetLastError() == static_cast<DWORD>(ERROR_INVALID_PASSWORD) ? L"That password did not open the certificate file." : L"That is not a certificate file with a key (.pfx or .p12).";
        return nullptr;
    }
    PCCERT_CONTEXT c = nullptr, best = nullptr;
    while ((c = CertEnumCertificatesInStore(store, c)) != nullptr) {
        DWORD n = 0;
        if (CertGetCertificateContextProperty(c, CERT_KEY_PROV_INFO_PROP_ID, nullptr, &n) || CertGetCertificateContextProperty(c, CERT_KEY_CONTEXT_PROP_ID, nullptr, &n)) {
            best = CertDuplicateCertificateContext(c);
            CertFreeCertificateContext(c);
            break;
        }
    }
    if (!best) { error = L"The file has no certificate with a key that can sign."; CertCloseStore(store, 0); store = nullptr; }
    return best;
}

PCCERT_CONTEXT makeOwnCertificate(const std::wstring& name, const std::wstring& email, std::wstring& error) {
    // An RSA 2048 signing key in the "Microsoft Enhanced RSA and AES Cryptographic Provider", then a self-signed
    // certificate valid for ten years, saved in the user's personal store so it can be chosen again later.
    wchar_t container[64];
    swprintf(container, 64, L"FeatherPDF-%08lX%08lX", GetTickCount(), GetCurrentProcessId());
    HCRYPTPROV prov = 0;
    if (!CryptAcquireContextW(&prov, container, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_NEWKEYSET)) {
        error = L"Windows could not make a place for the new key.";
        return nullptr;
    }
    HCRYPTKEY key = 0;
    if (!CryptGenKey(prov, AT_SIGNATURE, (2048u << 16), &key)) {
        CryptReleaseContext(prov, 0);
        error = L"Windows could not make the certificate.";
        return nullptr;
    }
    CryptDestroyKey(key);

    std::wstring dn = L"CN=" + name;
    if (!email.empty()) dn += L", E=" + email;
    DWORD nameLen = 0;
    CertStrToNameW(X509_ASN_ENCODING, dn.c_str(), CERT_X500_NAME_STR, nullptr, nullptr, &nameLen, nullptr);
    Bytes nameBlob(nameLen);
    CERT_NAME_BLOB subject{nameLen, nameBlob.data()};
    if (!nameLen || !CertStrToNameW(X509_ASN_ENCODING, dn.c_str(), CERT_X500_NAME_STR, nullptr, nameBlob.data(), &nameLen, nullptr)) {
        CryptReleaseContext(prov, 0);
        error = L"The name could not be used for a certificate.";
        return nullptr;
    }
    CRYPT_KEY_PROV_INFO kpi{};
    kpi.pwszContainerName = container;
    kpi.pwszProvName = const_cast<LPWSTR>(MS_ENH_RSA_AES_PROV_W);
    kpi.dwProvType = PROV_RSA_AES;
    kpi.dwKeySpec = AT_SIGNATURE;
    CRYPT_ALGORITHM_IDENTIFIER alg{const_cast<LPSTR>(szOID_RSA_SHA256RSA), {0, nullptr}};
    SYSTEMTIME start, end;
    GetSystemTime(&start);
    end = start; end.wYear += 10;
    PCCERT_CONTEXT cert = CertCreateSelfSignCertificate(prov, &subject, 0, &kpi, &alg, &start, &end, nullptr);
    if (!cert) {   // some providers cannot do SHA-256 certificates: fall back to the default algorithm
        cert = CertCreateSelfSignCertificate(prov, &subject, 0, &kpi, nullptr, &start, &end, nullptr);
    }
    CryptReleaseContext(prov, 0);   // the certificate keeps its own link to the key container
    if (!cert) { error = L"Windows could not make the certificate."; return nullptr; }
    HCERTSTORE my = CertOpenSystemStoreW(0, L"MY");
    if (my) {
        if (!CertAddCertificateContextToStore(my, cert, CERT_STORE_ADD_REPLACE_EXISTING, nullptr)) error = L"The certificate could not be kept in your personal certificates.";
        CertCloseStore(my, 0);
    }
    return cert;
}

} // namespace sign
