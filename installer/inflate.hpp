#pragma once
// A small decoder for raw deflate data (RFC 1951), enough for the installer's packed files (made with Python's zlib,
// wbits -15). Written after the structure of zlib's "puff" reference decoder: stored, fixed and dynamic blocks.
#include <cstddef>
#include <cstdint>
#include <vector>

namespace inflate {

// Decodes `in` (one complete raw deflate stream) and appends the bytes to `out`. False on damaged data.
bool raw(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out);

// CRC-32 (the zlib / zip one), to check each unpacked file.
uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0);

} // namespace inflate
