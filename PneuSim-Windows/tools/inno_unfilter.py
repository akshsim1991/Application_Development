#!/usr/bin/env python3
"""Reverse Inno Setup's x86 call-instruction filter (Inno 5.3.9+ variant, 64 KiB blocks).

Inno Setup applies this filter to .exe/.dll files before compressing them. It rewrites the
32-bit operand after every E8/E9 byte, which corrupts .NET metadata. Decoding restores the
original file byte for byte.

Usage:  python inno_unfilter.py FILTERED.exe ORIGINAL.exe
"""
import sys


def decode(buf: bytes) -> bytes:
    p = bytearray(buf)
    block = 0x10000
    for base in range(0, len(p), block):
        size = min(block, len(p) - base)
        if size < 5:
            continue
        size -= 4
        i = 0
        while i < size:
            if p[base + i] in (0xE8, 0xE9):
                i += 1
                j = base + i
                if p[j + 3] in (0x00, 0xFF):
                    addr = (base + i + 4) & 0xFFFFFF
                    rel = p[j] | (p[j + 1] << 8) | (p[j + 2] << 16)
                    rel = (rel - addr) & 0xFFFFFFFF
                    if rel & 0x800000:
                        p[j + 3] ^= 0xFF
                    p[j] = rel & 0xFF
                    p[j + 1] = (rel >> 8) & 0xFF
                    p[j + 2] = (rel >> 16) & 0xFF
                i += 4
            else:
                i += 1
    return bytes(p)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    open(sys.argv[2], "wb").write(decode(open(sys.argv[1], "rb").read()))
