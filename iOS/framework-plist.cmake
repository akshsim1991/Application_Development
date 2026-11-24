# The Info.plist of Frameworks/pdfium.framework (iOS will not load a framework without one). -DOUT=... -DPLATFORM=...
file(WRITE "${OUT}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
  <key>CFBundleExecutable</key><string>pdfium</string>
  <key>CFBundleIdentifier</key><string>com.featherpdf.ios.pdfium</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>CFBundleName</key><string>pdfium</string>
  <key>CFBundlePackageType</key><string>FMWK</string>
  <key>CFBundleShortVersionString</key><string>8066</string>
  <key>CFBundleVersion</key><string>8066</string>
  <key>CFBundleSupportedPlatforms</key><array><string>${PLATFORM}</string></array>
  <key>MinimumOSVersion</key><string>15.0</string>
</dict>
</plist>
")
