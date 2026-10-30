#include "inflate.hpp"

namespace inflate {
namespace {

constexpr int kMaxBits = 15;        // longest Huffman code
constexpr int kMaxLCodes = 286;     // literal / length codes
constexpr int kMaxDCodes = 30;      // distance codes
constexpr int kFixLCodes = 288;     // literal / length codes in the fixed table

struct State {
    const uint8_t* in;
    size_t inLen, pos = 0;
    uint32_t bitBuf = 0;
    int bitCnt = 0;
    std::vector<uint8_t>* out;
    bool bad = false;
};

// `need` bits from the input, first bit in the lowest place
int bits(State& s, int need) {
    uint32_t val = s.bitBuf;
    while (s.bitCnt < need) {
        if (s.pos >= s.inLen) { s.bad = true; return 0; }
        val |= static_cast<uint32_t>(s.in[s.pos++]) << s.bitCnt;
        s.bitCnt += 8;
    }
    s.bitBuf = val >> need;
    s.bitCnt -= need;
    return static_cast<int>(val & ((1u << need) - 1));
}

// a canonical Huffman code: how many codes of each length, and the symbols in code order
struct Huffman {
    short count[kMaxBits + 1];
    short symbol[kFixLCodes];
};

int decode(State& s, const Huffman& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= kMaxBits; ++len) {
        code |= bits(s, 1);
        if (s.bad) return -1;
        const int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s.bad = true;   // ran out of codes
    return -1;
}

// builds `h` from code lengths; returns 0 for a complete code, > 0 incomplete, < 0 over-subscribed
int construct(Huffman& h, const short* length, int n) {
    for (int len = 0; len <= kMaxBits; ++len) h.count[len] = 0;
    for (int sym = 0; sym < n; ++sym) h.count[length[sym]]++;
    if (h.count[0] == n) return 0;   // no codes: complete, but decoding will fail
    int left = 1;
    for (int len = 1; len <= kMaxBits; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return left;
    }
    short offs[kMaxBits + 1];
    offs[1] = 0;
    for (int len = 1; len < kMaxBits; ++len) offs[len + 1] = static_cast<short>(offs[len] + h.count[len]);
    for (int sym = 0; sym < n; ++sym)
        if (length[sym] != 0) h.symbol[offs[length[sym]]++] = static_cast<short>(sym);
    return left;
}

bool stored(State& s) {
    s.bitBuf = 0; s.bitCnt = 0;   // to the byte boundary
    if (s.pos + 4 > s.inLen) return false;
    const unsigned len = s.in[s.pos] | (s.in[s.pos + 1] << 8);
    const unsigned nlen = s.in[s.pos + 2] | (s.in[s.pos + 3] << 8);
    s.pos += 4;
    if (len != (~nlen & 0xffffu)) return false;
    if (s.pos + len > s.inLen) return false;
    s.out->insert(s.out->end(), s.in + s.pos, s.in + s.pos + len);
    s.pos += len;
    return true;
}

const short kLBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const short kLExt[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const short kDBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const short kDExt[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool codes(State& s, const Huffman& lencode, const Huffman& distcode) {
    std::vector<uint8_t>& out = *s.out;
    for (;;) {
        int sym = decode(s, lencode);
        if (sym < 0) return false;
        if (sym < 256) { out.push_back(static_cast<uint8_t>(sym)); continue; }
        if (sym == 256) return true;   // end of the block
        sym -= 257;
        if (sym >= 29) return false;
        const int len = kLBase[sym] + bits(s, kLExt[sym]);
        const int dsym = decode(s, distcode);
        if (dsym < 0 || dsym >= 30) return false;
        const size_t dist = static_cast<size_t>(kDBase[dsym] + bits(s, kDExt[dsym]));
        if (s.bad || dist > out.size()) return false;
        const size_t from = out.size() - dist;
        for (int i = 0; i < len; ++i) out.push_back(out[from + static_cast<size_t>(i)]);   // may overlap: byte by byte
    }
}

bool fixedBlock(State& s) {
    static Huffman lencode, distcode;
    static bool made = false;
    if (!made) {
        short lengths[kFixLCodes];
        int sym = 0;
        for (; sym < 144; ++sym) lengths[sym] = 8;
        for (; sym < 256; ++sym) lengths[sym] = 9;
        for (; sym < 280; ++sym) lengths[sym] = 7;
        for (; sym < kFixLCodes; ++sym) lengths[sym] = 8;
        construct(lencode, lengths, kFixLCodes);
        for (sym = 0; sym < kMaxDCodes; ++sym) lengths[sym] = 5;
        construct(distcode, lengths, kMaxDCodes);
        made = true;
    }
    return codes(s, lencode, distcode);
}

bool dynamicBlock(State& s) {
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;
    if (s.bad || nlen > kMaxLCodes || ndist > kMaxDCodes) return false;
    short lengths[kMaxLCodes + kMaxDCodes];
    int index = 0;
    for (; index < ncode; ++index) lengths[order[index]] = static_cast<short>(bits(s, 3));
    for (; index < 19; ++index) lengths[order[index]] = 0;
    if (s.bad) return false;
    Huffman lencode, distcode;
    if (construct(lencode, lengths, 19) != 0) return false;   // the code-length code must be complete
    index = 0;
    while (index < nlen + ndist) {
        int symbol = decode(s, lencode);
        if (symbol < 0) return false;
        if (symbol < 16) { lengths[index++] = static_cast<short>(symbol); continue; }
        short len = 0;
        int repeat;
        if (symbol == 16) {
            if (index == 0) return false;
            len = lengths[index - 1];
            repeat = 3 + bits(s, 2);
        } else if (symbol == 17) repeat = 3 + bits(s, 3);
        else repeat = 11 + bits(s, 7);
        if (s.bad || index + repeat > nlen + ndist) return false;
        while (repeat--) lengths[index++] = len;
    }
    if (lengths[256] == 0) return false;   // no end-of-block code
    const int err = construct(lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1)) return false;   // only one code may be incomplete
    const int derr = construct(distcode, lengths + nlen, ndist);
    if (derr < 0 || (derr > 0 && ndist - distcode.count[0] != 1)) return false;
    return codes(s, lencode, distcode);
}

} // namespace

bool raw(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out) {
    State s;
    s.in = in; s.inLen = inLen; s.out = &out;
    int last;
    do {
        last = bits(s, 1);
        const int type = bits(s, 2);
        if (s.bad) return false;
        bool ok;
        if (type == 0) ok = stored(s);
        else if (type == 1) ok = fixedBlock(s);
        else if (type == 2) ok = dynamicBlock(s);
        else ok = false;
        if (!ok || s.bad) return false;
    } while (!last);
    return true;
}

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc) {
    static uint32_t table[256];
    static bool made = false;
    if (!made) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        made = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < len; ++i) crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return ~crc;
}

} // namespace inflate
