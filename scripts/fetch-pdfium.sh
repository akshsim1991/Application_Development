#!/usr/bin/env bash
# Downloads the prebuilt PDFium (same release the 2.12 installer shipped: chromium/8066, no V8/XFA)
# into third_party/pdfium (headers, licenses, pdfium.dll).
set -euo pipefail
cd "$(dirname "$0")/.."
dest=third_party/pdfium
[ -f "$dest/bin/pdfium.dll" ] && { echo "pdfium already present"; exit 0; }
mkdir -p "$dest"
curl -fsSL -o /tmp/pdfium-win-x64.tgz \
  "https://github.com/bblanchon/pdfium-binaries/releases/download/chromium%2F8066/pdfium-win-x64.tgz"
tar xzf /tmp/pdfium-win-x64.tgz -C "$dest"
echo "pdfium fetched into $dest"
