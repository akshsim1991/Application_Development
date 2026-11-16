#include "crypto.hpp"

#if defined(__ANDROID__) && !defined(FEATHER_SYSTEM_HOOKS)
#define FEATHER_SYSTEM_HOOKS 1
#endif
#if defined(FEATHER_SYSTEM_HOOKS)
// given by the app (crypto::setSystem)
#elif defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecRandom.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace crypto {

#if defined(FEATHER_SYSTEM_HOOKS)

namespace {
System& sys() { static System s; return s; }
} // namespace
void setSystem(System s) { sys() = std::move(s); }
bool hash(Hash h, const Bytes& data, Bytes& out) { return sys().hash && sys().hash(h, data, out); }
bool aesCbc(const Bytes& key, const Bytes& iv, const Bytes& in, Bytes& out) { return sys().aes && in.size() % 16 == 0 && sys().aes(true, key, &iv, in, out); }
bool aesEcb(const Bytes& key, const Bytes& in, Bytes& out) { return sys().aes && in.size() % 16 == 0 && sys().aes(false, key, nullptr, in, out); }
bool random(size_t n, Bytes& out) { return sys().random && sys().random(n, out) && out.size() == n; }

#elif defined(_WIN32)

namespace {
struct Alg {
    BCRYPT_ALG_HANDLE h = nullptr;
    explicit Alg(LPCWSTR id, LPCWSTR mode = nullptr) {
        if (BCryptOpenAlgorithmProvider(&h, id, nullptr, 0) != 0) { h = nullptr; return; }
        if (mode) BCryptSetProperty(h, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(mode)),
                                    static_cast<ULONG>((wcslen(mode) + 1) * sizeof(wchar_t)), 0);
    }
    ~Alg() { if (h) BCryptCloseAlgorithmProvider(h, 0); }
};

bool aes(LPCWSTR mode, const Bytes& key, const Bytes* iv, const Bytes& in, Bytes& out) {
    Alg a(BCRYPT_AES_ALGORITHM, mode);
    if (!a.h) return false;
    BCRYPT_KEY_HANDLE k = nullptr;
    if (BCryptGenerateSymmetricKey(a.h, &k, nullptr, 0, const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0) != 0) return false;
    Bytes ivc = iv ? *iv : Bytes();
    out.assign(in.size(), 0);
    ULONG got = 0;
    const bool ok = BCryptEncrypt(k, const_cast<PUCHAR>(in.data()), static_cast<ULONG>(in.size()), nullptr, iv ? ivc.data() : nullptr,
                                  static_cast<ULONG>(ivc.size()), out.data(), static_cast<ULONG>(out.size()), &got, 0) == 0;
    BCryptDestroyKey(k);
    return ok;
}
} // namespace

bool hash(Hash h, const Bytes& data, Bytes& out) {
    Alg a(h == Hash::Sha256 ? BCRYPT_SHA256_ALGORITHM : h == Hash::Sha384 ? BCRYPT_SHA384_ALGORITHM : BCRYPT_SHA512_ALGORITHM);
    if (!a.h) return false;
    DWORD len = 0, got = 0;
    BCryptGetProperty(a.h, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&len), sizeof(len), &got, 0);
    out.assign(len, 0);
    // create, feed, finish: on every Windows from 7 (BCryptHash, all in one call, exists only from Windows 10)
    BCRYPT_HASH_HANDLE hh = nullptr;
    if (BCryptCreateHash(a.h, &hh, nullptr, 0, nullptr, 0, 0) != 0) return false;
    const bool ok = BCryptHashData(hh, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), 0) == 0 &&
                    BCryptFinishHash(hh, out.data(), len, 0) == 0;
    BCryptDestroyHash(hh);
    return ok;
}
bool aesCbc(const Bytes& key, const Bytes& iv, const Bytes& in, Bytes& out) { return aes(BCRYPT_CHAIN_MODE_CBC, key, &iv, in, out); }
bool aesEcb(const Bytes& key, const Bytes& in, Bytes& out) { return aes(BCRYPT_CHAIN_MODE_ECB, key, nullptr, in, out); }
bool random(size_t n, Bytes& out) {
    out.assign(n, 0);
    return BCryptGenRandom(nullptr, out.data(), static_cast<ULONG>(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}

#elif defined(__APPLE__)

namespace {
bool aes(CCOptions options, const Bytes& key, const Bytes* iv, const Bytes& in, Bytes& out) {
    out.assign(in.size(), 0);
    size_t got = 0;
    return CCCrypt(kCCEncrypt, kCCAlgorithmAES, options, key.data(), key.size(), iv ? iv->data() : nullptr, in.data(), in.size(),
                   out.data(), out.size(), &got) == kCCSuccess && got == in.size();
}
} // namespace

bool hash(Hash h, const Bytes& data, Bytes& out) {
    const auto n = static_cast<CC_LONG>(data.size());
    switch (h) {
        case Hash::Sha256: out.assign(CC_SHA256_DIGEST_LENGTH, 0); CC_SHA256(data.data(), n, out.data()); break;
        case Hash::Sha384: out.assign(CC_SHA384_DIGEST_LENGTH, 0); CC_SHA384(data.data(), n, out.data()); break;
        case Hash::Sha512: out.assign(CC_SHA512_DIGEST_LENGTH, 0); CC_SHA512(data.data(), n, out.data()); break;
    }
    return true;
}
bool aesCbc(const Bytes& key, const Bytes& iv, const Bytes& in, Bytes& out) { return aes(0, key, &iv, in, out); }
bool aesEcb(const Bytes& key, const Bytes& in, Bytes& out) { return aes(kCCOptionECBMode, key, nullptr, in, out); }
bool random(size_t n, Bytes& out) {
    out.assign(n, 0);
    return SecRandomCopyBytes(kSecRandomDefault, n, out.data()) == errSecSuccess;
}

#else   // OpenSSL

namespace {
bool aes(bool cbc, const Bytes& key, const Bytes* iv, const Bytes& in, Bytes& out) {
    const EVP_CIPHER* c = key.size() == 16 ? (cbc ? EVP_aes_128_cbc() : EVP_aes_128_ecb())
                        : key.size() == 24 ? (cbc ? EVP_aes_192_cbc() : EVP_aes_192_ecb())
                                           : (cbc ? EVP_aes_256_cbc() : EVP_aes_256_ecb());
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    out.assign(in.size() + 16, 0);
    int n1 = 0, n2 = 0;
    bool ok = EVP_EncryptInit_ex(ctx, c, nullptr, key.data(), iv ? iv->data() : nullptr) == 1 &&
              EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 &&
              EVP_EncryptUpdate(ctx, out.data(), &n1, in.data(), static_cast<int>(in.size())) == 1 &&
              EVP_EncryptFinal_ex(ctx, out.data() + n1, &n2) == 1;
    EVP_CIPHER_CTX_free(ctx);
    out.resize(static_cast<size_t>(n1 + n2));
    return ok && out.size() == in.size();
}
} // namespace

bool hash(Hash h, const Bytes& data, Bytes& out) {
    const EVP_MD* md = h == Hash::Sha256 ? EVP_sha256() : h == Hash::Sha384 ? EVP_sha384() : EVP_sha512();
    out.assign(static_cast<size_t>(EVP_MD_size(md)), 0);
    unsigned int len = 0;
    return EVP_Digest(data.data(), data.size(), out.data(), &len, md, nullptr) == 1;
}
bool aesCbc(const Bytes& key, const Bytes& iv, const Bytes& in, Bytes& out) { return aes(true, key, &iv, in, out); }
bool aesEcb(const Bytes& key, const Bytes& in, Bytes& out) { return aes(false, key, nullptr, in, out); }
bool random(size_t n, Bytes& out) {
    out.assign(n, 0);
    return RAND_bytes(out.data(), static_cast<int>(n)) == 1;
}

#endif

#if !defined(FEATHER_SYSTEM_HOOKS)
void setSystem(System) {}   // the system's own cryptography is used
#endif

} // namespace crypto
