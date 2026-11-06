#!/bin/zsh
set -euo pipefail
cd "${0:A:h}"
STAGING="$(mktemp -d "${TMPDIR:-/tmp}/FileGlade-file-tests.XXXXXX")"
trap 'rm -rf "$STAGING"' EXIT
xcrun swiftc -swift-version 5 -module-cache-path "$STAGING/cache" \
  FileGlade/FileOperations.swift Tests/main.swift -o "$STAGING/check"
"$STAGING/check"
