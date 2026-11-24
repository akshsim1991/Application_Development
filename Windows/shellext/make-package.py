#!/usr/bin/env python3
"""Makes FeatherPDF.msix: the sparse package that puts "Combine into one PDF" in File Explorer's first right-click
menu on Windows 11 (the command is FeatherPDFShell.dll, Windows/shellext/shellext.cpp), and FeatherPDF-package.cer,
the certificate it is signed with.

Usage: make-package.py <version a.b.c> <output folder>

A sparse package holds no program files: only its manifest and logos. Its files (FeatherPDF.exe, FeatherPDFShell.dll)
stay in the program folder, given as the package's external location when it is registered.

The certificate is made new at each build and its private key is deleted as soon as the package is signed, so nothing
else can ever be signed with it. The installer adds it to the PC's "Trusted People" (what Windows asks of a package
signed by its maker rather than bought from a certificate authority) and takes the older ones of the same name out.

The package: a ZIP of stored (uncompressed) files, with AppxBlockMap.xml (each file's SHA-256 per 64 KiB block) and
[Content_Types].xml; osslsigncode signs it (AppxSignature.p7x).
"""
import base64
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import zipfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CLSID = "89A01A5F-1EB5-4A99-9F4E-51CCED32C827"   # as in shellext.cpp
PUBLISHER = "CN=Feather PDF"
TYPES = [".pdf", ".png", ".jpg", ".jpeg", ".bmp", ".gif", ".tif", ".tiff", ".txt", ".md", ".markdown",
         ".ppt", ".pptx", ".pps", ".ppsx", ".odp", ".doc", ".docx", ".rtf", ".odt", ".xls", ".xlsx", ".ods"]


def manifest(version):
    items = "\n".join(
        '            <desktop5:ItemType Type="%s"><desktop5:Verb Id="FeatherPDFCombine" Clsid="%s"/></desktop5:ItemType>' % (t, CLSID)
        for t in TYPES)
    return """<?xml version="1.0" encoding="utf-8"?>
<Package xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
         xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
         xmlns:uap10="http://schemas.microsoft.com/appx/manifest/uap/windows10/10"
         xmlns:desktop4="http://schemas.microsoft.com/appx/manifest/desktop/windows10/4"
         xmlns:desktop5="http://schemas.microsoft.com/appx/manifest/desktop/windows10/5"
         xmlns:com="http://schemas.microsoft.com/appx/manifest/com/windows10"
         xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
         IgnorableNamespaces="uap uap10 desktop4 desktop5 com rescap">
  <Identity Name="FeatherPDF.ExplorerMenu" Publisher="%s" Version="%s.0" ProcessorArchitecture="x64"/>
  <Properties>
    <DisplayName>Feather PDF</DisplayName>
    <PublisherDisplayName>Feather PDF</PublisherDisplayName>
    <Logo>Assets\\StoreLogo.png</Logo>
    <uap10:AllowExternalContent>true</uap10:AllowExternalContent>
  </Properties>
  <Resources>
    <Resource Language="en-us"/>
  </Resources>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.19041.0" MaxVersionTested="10.0.26100.0"/>
  </Dependencies>
  <Capabilities>
    <rescap:Capability Name="runFullTrust"/>
  </Capabilities>
  <Applications>
    <Application Id="FeatherPDF" Executable="FeatherPDF.exe" uap10:TrustLevel="mediumIL" uap10:RuntimeBehavior="win32App">
      <uap:VisualElements AppListEntry="none" DisplayName="Feather PDF" Description="Feather PDF: File Explorer menu"
                          BackgroundColor="transparent" Square150x150Logo="Assets\\Square150x150Logo.png" Square44x44Logo="Assets\\Square44x44Logo.png"/>
      <Extensions>
        <desktop4:Extension Category="windows.fileExplorerContextMenus">
          <desktop4:FileExplorerContextMenus>
%s
          </desktop4:FileExplorerContextMenus>
        </desktop4:Extension>
        <com:Extension Category="windows.comServer">
          <com:ComServer>
            <com:SurrogateServer DisplayName="Feather PDF File Explorer menu">
              <com:Class Id="%s" Path="FeatherPDFShell.dll" ThreadingModel="STA"/>
            </com:SurrogateServer>
          </com:ComServer>
        </com:Extension>
      </Extensions>
    </Application>
  </Applications>
</Package>
""" % (PUBLISHER, version, items, CLSID)


def logos(folder):
    icon = Image.open(os.path.join(REPO, "Windows", "resources", "FeatherPDF.ico"))
    best = max(icon.ico.sizes(), key=lambda s: s[0])
    icon.size = best
    img = icon.convert("RGBA")
    out = {}
    for name, size in (("Square44x44Logo.png", 44), ("Square150x150Logo.png", 150), ("StoreLogo.png", 50)):
        path = os.path.join(folder, name)
        img.resize((size, size), Image.LANCZOS).save(path)
        out["Assets\\" + name] = path
    return out


def block_map(entries):
    """entries: (name in the package with backslashes, bytes); the files are stored, so blocks have no Size."""
    lines = ['<?xml version="1.0" encoding="UTF-8" standalone="no"?>',
             '<BlockMap xmlns="http://schemas.microsoft.com/appx/2010/blockmap" HashMethod="http://www.w3.org/2001/04/xmlenc#sha256">']
    for name, data in entries:
        lfh = 30 + len(name.encode("utf-8"))   # the local file header: fixed part and the name (no extra field)
        lines.append('<File Name="%s" Size="%d" LfhSize="%d">' % (name, len(data), lfh))
        for at in range(0, max(len(data), 1), 65536):
            if at >= len(data) and len(data) > 0:
                break
            h = base64.b64encode(hashlib.sha256(data[at:at + 65536]).digest()).decode()
            lines.append('<Block Hash="%s"/>' % h)
        lines.append("</File>")
    lines.append("</BlockMap>")
    return "\r\n".join(lines).encode("utf-8")


def content_types():
    return ('<?xml version="1.0" encoding="UTF-8"?>'
            '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
            '<Default Extension="xml" ContentType="application/vnd.ms-appx.manifest+xml"/>'
            '<Default Extension="png" ContentType="image/png"/>'
            '<Override PartName="/AppxBlockMap.xml" ContentType="application/vnd.ms-appx.blockmap+xml"/>'
            '</Types>').encode("utf-8")


def stored(zf, name, data, compress=False):
    info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_DEFLATED if compress else zipfile.ZIP_STORED
    info.create_system = 0
    zf.writestr(info, data)


def main():
    version, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp()
    try:
        files = [("AppxManifest.xml", manifest(version).encode("utf-8"))]
        for name, path in sorted(logos(work).items()):
            files.append((name, open(path, "rb").read()))
        unsigned = os.path.join(work, "unsigned.msix")
        with zipfile.ZipFile(unsigned, "w") as zf:
            for name, data in files:
                stored(zf, name.replace("\\", "/"), data)
            stored(zf, "AppxBlockMap.xml", block_map(files))
            # (compressed, as makeappx writes it: osslsigncode rewrites it compressed when signing, and kept the
            # "stored" mark of a stored one, which made the package unreadable)
            stored(zf, "[Content_Types].xml", content_types(), compress=True)
        # a certificate for this build only; its key is deleted below
        key, cert = os.path.join(work, "key.pem"), os.path.join(work, "cert.pem")
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-sha256", "-days", "3650",
                        "-subj", "/CN=Feather PDF", "-keyout", key, "-out", cert,
                        "-addext", "basicConstraints=critical,CA:FALSE",
                        "-addext", "keyUsage=critical,digitalSignature",
                        "-addext", "extendedKeyUsage=codeSigning"], check=True, capture_output=True)
        signed = os.path.join(out, "FeatherPDF.msix")
        if os.path.exists(signed):
            os.remove(signed)
        subprocess.run(["osslsigncode", "sign", "-certs", cert, "-key", key, "-h", "sha256", "-in", unsigned, "-out", signed],
                       check=True, capture_output=True)
        subprocess.run(["openssl", "x509", "-in", cert, "-outform", "DER", "-out", os.path.join(out, "FeatherPDF-package.cer")],
                       check=True, capture_output=True)
        print("made", signed)
    finally:
        shutil.rmtree(work, ignore_errors=True)   # the private key with it


if __name__ == "__main__":
    main()
