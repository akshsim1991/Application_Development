#!/bin/zsh
set -euo pipefail
cd "${0:A:h}"
STAGING="$(mktemp -d "${TMPDIR:-/tmp}/FileGlade-dashboard-tests.XXXXXX")"
trap 'rm -rf "$STAGING"' EXIT
xcrun swiftc -swift-version 5 -parse-as-library -module-cache-path "${TMPDIR:-/tmp}/FileGlade-swift-cache" \
  Source/PowerCore.swift Source/PowerTools.swift Source/Dashboard.swift Source/ZIPFoundation/*.swift Tests/DashboardTests.swift \
  -o "$STAGING/check" -framework SwiftUI -framework PDFKit -framework ImageIO -framework CryptoKit -lcompression -lsqlite3
"$STAGING/check"
