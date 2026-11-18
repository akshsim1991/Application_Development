#!/usr/bin/env python3
"""Extract the program files from a PneuSim Inno Setup installer (tested on 3.2.0, Inno 6.7).

Usage:  python extract_installer.py PneuSim-Setup-3.2.0.exe OUTDIR

Writes OUTDIR/PneuSim.exe (with Inno's x86 call filter reversed so the .NET metadata is valid),
OUTDIR/PneuSim.exe.config, OUTDIR/README.md and OUTDIR/circuit_NN.pneu (in installer order:
ex1, ex10, ex11, ex12, ex2 … ex9, then the six generated examples).
Only the Python standard library is used.
"""
import lzma
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from inno_unfilter import decode  # noqa: E402


def pe_size(buf: bytes) -> int:
    pe = struct.unpack_from("<I", buf, 0x3C)[0]
    nsec = struct.unpack_from("<H", buf, pe + 6)[0]
    opt = struct.unpack_from("<H", buf, pe + 20)[0]
    sec = pe + 24 + opt
    end = 0
    for i in range(nsec):
        raw_size, raw_ptr = struct.unpack_from("<II", buf, sec + 40 * i + 16)
        end = max(end, raw_ptr + raw_size)
    return end


def main(setup: str, out: str) -> None:
    data = open(setup, "rb").read()
    pos = data.find(b"zlb\x1a")
    if pos < 0:
        sys.exit("no compressed file chunk found")
    props = data[pos + 4]
    dict_size = (2 | (props & 1)) << (props // 2 + 11)
    dec = lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=[{"id": lzma.FILTER_LZMA2, "dict_size": dict_size}])
    chunk = dec.decompress(data[pos + 5:])
    os.makedirs(out, exist_ok=True)

    exe_end = pe_size(chunk)
    open(os.path.join(out, "PneuSim.exe"), "wb").write(decode(chunk[:exe_end]))
    rest = chunk[exe_end:]
    cfg_end = rest.find(b"</configuration>") + len(b"</configuration>")
    open(os.path.join(out, "PneuSim.exe.config"), "wb").write(rest[:cfg_end])
    starts = [m.start() for m in re.finditer(re.escape(b"\xef\xbb\xbf<?xml"), rest)]
    open(os.path.join(out, "README.md"), "wb").write(rest[cfg_end:starts[0]])
    starts.append(len(rest))
    for i in range(len(starts) - 1):
        open(os.path.join(out, f"circuit_{i + 1:02d}.pneu"), "wb").write(rest[starts[i]:starts[i + 1]])
    print(f"PneuSim.exe {exe_end} bytes, {len(starts) - 1} circuits -> {out}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
