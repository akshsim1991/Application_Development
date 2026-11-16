#pragma once
// The cryptography the engine needs, from the system: Windows' CNG (bcrypt), Apple's CommonCrypto, or OpenSSL (the
// engine check on Linux). The PDF logic that uses it (protect.cpp) is shared.
#include <cstddef>
#include <functional>
#include <vector>

namespace crypto {

using Bytes = std::vector<unsigned char>;
enum class Hash { Sha256, Sha384, Sha512 };

bool hash(Hash h, const Bytes& data, Bytes& out);
// AES-CBC (key 16 / 24 / 32 bytes) without padding: `in` must be a multiple of 16 bytes.
bool aesCbc(const Bytes& key, const Bytes& iv, const Bytes& in, Bytes& out);
// AES-ECB of whole 16-byte blocks.
bool aesEcb(const Bytes& key, const Bytes& in, Bytes& out);
// Random bytes from the system's secure source.
bool random(size_t n, Bytes& out);

// On Android (or with FEATHER_SYSTEM_HOOKS) the app gives the cryptography (Java's MessageDigest, Cipher, SecureRandom).
struct System {
    std::function<bool(Hash, const Bytes&, Bytes&)> hash;
    std::function<bool(bool cbc, const Bytes& key, const Bytes* iv, const Bytes& in, Bytes& out)> aes;
    std::function<bool(size_t, Bytes&)> random;
};
void setSystem(System s);

} // namespace crypto
