#!/bin/zsh
set -euo pipefail
cd "${0:A:h}"
DESTINATION="$PWD/FileGlade.app"
STAGING="$(mktemp -d "${TMPDIR:-/tmp}/FileGlade-build.XXXXXX")"
trap 'rm -rf "$STAGING"' EXIT
APP="$STAGING/FileGlade.app"
CACHE="${TMPDIR:-/tmp}/FileGlade-swift-cache"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$CACHE"
xcrun swiftc -swift-version 5 -O -module-cache-path "$CACHE" -target "$(uname -m)-apple-macosx13.0" Source/main.swift Source/PowerCore.swift Source/PowerTools.swift Source/Dashboard.swift Source/FileOps.swift Source/Preview.swift Source/QuickSwitcher.swift Source/Updater.swift Source/Panes.swift Source/Compare.swift Source/Archives.swift Source/DocSearch.swift Source/Shortcuts.swift Source/ZIPFoundation/*.swift -o "$APP/Contents/MacOS/FileGlade" -framework AppKit -framework SwiftUI -framework UniformTypeIdentifiers -framework CryptoKit -framework PDFKit -framework ImageIO -framework AVKit -framework AVFoundation -framework QuickLookThumbnailing -lcompression -lsqlite3
cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>FileGlade</string>
<key>CFBundleDisplayName</key><string>FileGlade</string>
<key>CFBundleIdentifier</key><string>local.fileglade.explorer</string>
<key>CFBundleVersion</key><string>6</string>
<key>CFBundleShortVersionString</key><string>3.0</string>
<key>CFBundleExecutable</key><string>FileGlade</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleIconFile</key><string>AppIcon</string>
<key>LSMinimumSystemVersion</key><string>13.0</string>
<key>NSHighResolutionCapable</key><true/>
<key>NSDocumentsFolderUsageDescription</key><string>Browse and manage the files you choose in Documents.</string>
<key>NSDesktopFolderUsageDescription</key><string>Browse and manage the files you choose on your Desktop.</string>
<key>NSDownloadsFolderUsageDescription</key><string>Browse and manage the files you choose in Downloads.</string>
<key>NSRemovableVolumesUsageDescription</key><string>Browse connected drives that you choose.</string>
</dict></plist>
PLIST
if [[ -f Source/AppIcon.icns ]]; then cp Source/AppIcon.icns "$APP/Contents/Resources/AppIcon.icns"; fi
cp Source/ZIPFoundation/LICENSE "$APP/Contents/Resources/ZIPFoundation-LICENSE.txt"
xattr -cr "$APP"
# Sign with a stable identity so macOS remembers folder permissions across rebuilds.
# An ad-hoc signature changes every build, which makes macOS ask for access again each time.
IDENTITY="${FILEGLADE_SIGN_IDENTITY:-$(security find-identity -v -p codesigning | awk '/Apple Development/ {print $2; exit}')}"
IDENTITY="${IDENTITY:--}"
echo "Signing with: ${IDENTITY}"
codesign --force --deep --sign "$IDENTITY" --identifier local.fileglade.explorer "$APP"
ditto --norsrc --noextattr "$APP" "$DESTINATION"
xattr -cr "$DESTINATION"
codesign --verify --deep --strict "$DESTINATION"
echo "Built: $DESTINATION"
