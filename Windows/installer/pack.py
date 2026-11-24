#!/usr/bin/env python3
"""Makes the Feather PDF installer: the setup program (FeatherPDFSetup.exe) with the program's files appended.

Usage: pack.py <build dir> <setup stub exe> <output exe>

Layout of what is appended to the stub (all numbers little-endian):
  "FPDFSET1", then strings as u32 length + UTF-8: version, build date (long form), build date (ISO)
  u32 file count; per file: u32 length + UTF-8 relative path (backslashes), u64 size, u32 CRC-32, u32 block count,
  per block: u64 offset (from the start of the data), u32 packed size, u32 size
  then the data: each file in blocks of 1 MiB, each block one raw deflate stream (so progress can be shown as it unpacks)
and at the very end: u64 offset of "FPDFSET1" in the file, "FPDFEND1".
The build date is read from FeatherPDF.exe itself (its version information says "Built YYYY-MM-DD"), so the installer
shows the date of the program it installs.
"""
import os
import re
import struct
import sys
import zlib

BLOCK = 1 << 20
MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]


def files_to_install(build):
    root = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(root))   # Windows/installer -> the top of the repository
    items = [("FeatherPDF.exe", os.path.join(build, "FeatherPDF.exe")),
             ("pdfium.dll", os.path.join(build, "pdfium.dll")),
             # Windows 11's first right-click menu (Windows/shellext): the command, its package and certificate
             ("FeatherPDFShell.dll", os.path.join(build, "FeatherPDFShell.dll")),
             ("FeatherPDF.msix", os.path.join(build, "FeatherPDF.msix")),
             ("FeatherPDF-package.cer", os.path.join(build, "FeatherPDF-package.cer")),
             ("Feather PDF User Guide.pdf", os.path.join(repo, "docs", "Feather PDF User Guide.pdf")),
             ("THIRD_PARTY_NOTICES.md", os.path.join(repo, "THIRD_PARTY_NOTICES.md"))]
    for sub in ("licenses", "tessdata"):
        top = os.path.join(build, sub)
        for dirpath, _, names in sorted(os.walk(top)):
            for n in sorted(names):
                full = os.path.join(dirpath, n)
                items.append((os.path.relpath(full, build).replace("/", "\\"), full))
    for rel, full in items:
        if not os.path.isfile(full):
            sys.exit("missing: " + full)
    return items


def version_string(exe, key):
    """A value of the version information (UTF-16 text after its key and padding)."""
    data = open(exe, "rb").read()
    at = data.find(key.encode("utf-16-le") + b"\0\0")
    if at < 0:
        sys.exit("FeatherPDF.exe has no %s in its version information" % key)
    at += len(key) * 2 + 2
    while data[at:at + 2] == b"\0\0":
        at += 2
    end = at
    while data[end:end + 2] != b"\0\0":
        end += 2
    return data[at:end].decode("utf-16-le")


def build_date(exe):
    m = re.fullmatch(r"Built (\d{4})-(\d{2})-(\d{2})", version_string(exe, "Comments"))
    if not m:
        sys.exit("FeatherPDF.exe has no build date in its version information")
    y, mo, d = (int(m.group(i)) for i in (1, 2, 3))
    return "%d %s %d" % (d, MONTHS[mo - 1], y), "%04d-%02d-%02d" % (y, mo, d)


def version(exe):
    return version_string(exe, "ProductVersion")


def s(text):
    b = text.encode("utf-8")
    return struct.pack("<I", len(b)) + b


def main():
    build, stub, out = sys.argv[1:4]
    exe = os.path.join(build, "FeatherPDF.exe")
    ver = version(exe)
    long_date, iso_date = build_date(exe)
    items = files_to_install(build)
    data = bytearray()
    entries = []
    for rel, full in items:
        content = open(full, "rb").read()
        blocks = []
        for i in range(0, max(len(content), 1), BLOCK):
            part = content[i:i + BLOCK]
            co = zlib.compressobj(9, zlib.DEFLATED, -15, 9)
            packed = co.compress(part) + co.flush()
            blocks.append((len(data), len(packed), len(part)))
            data += packed
        entries.append((rel, len(content), zlib.crc32(content) & 0xFFFFFFFF, blocks))
    head = bytearray(b"FPDFSET1")
    head += s(ver) + s(long_date) + s(iso_date)
    head += struct.pack("<I", len(entries))
    for rel, size, crc, blocks in entries:
        head += s(rel) + struct.pack("<QII", size, crc, len(blocks))
        for off, clen, ulen in blocks:
            head += struct.pack("<QII", off, clen, ulen)
    stub_bytes = open(stub, "rb").read()
    with open(out + ".tmp", "wb") as f:
        f.write(stub_bytes)
        f.write(head)
        f.write(data)
        f.write(struct.pack("<Q", len(stub_bytes)) + b"FPDFEND1")
    os.replace(out + ".tmp", out)
    total = sum(e[1] for e in entries)
    print("%s: version %s, built %s, %d files, %.1f MB unpacked, %.1f MB" % (out, ver, long_date, len(entries), total / 1e6, os.path.getsize(out) / 1e6))


if __name__ == "__main__":
    main()
