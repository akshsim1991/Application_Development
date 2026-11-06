#!/bin/zsh
set -euo pipefail
cd "${0:A:h}"
STAGING="$(mktemp -d "${TMPDIR:-/tmp}/FileGlade-power-tests.XXXXXX")"
trap 'rm -rf "$STAGING"' EXIT
xcrun swiftc -swift-version 5 -parse-as-library -module-cache-path "${TMPDIR:-/tmp}/FileGlade-swift-cache" \
  Source/PowerCore.swift Source/ZIPFoundation/*.swift Tests/PowerTests.swift \
  -o "$STAGING/check" -framework PDFKit -framework ImageIO -framework CryptoKit -lcompression
"$STAGING/check"
