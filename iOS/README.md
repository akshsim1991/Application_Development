# Feather PDF for iPhone and iPad

The UIKit app (Objective-C++, `src/`) on the same PDF engine as the Windows, Mac and Android apps (`../shared`).

| File | What it is |
| --- | --- |
| `src/FPApp.mm` | the app, the start screen (open a PDF, recent documents) and the build check (`--selftest`) |
| `src/FPViewer.mm` | the documents in tabs: sidebar (thumbnails, bookmarks), full screen, tools, find, menus, saving |
| `src/FPPagesView.mm` | the pages, drawn in tiles at any zoom, with the overlay (matches, selection, edit outlines, pen) |
| `src/FPSheets.mm` | the paragraph editor (x², x₂) and the equation editor (live picture) |
| `src/FPEngine.mm` | the engine's calls in Objective-C (as the Android bridge) |
| `src/FPSettings*.mm`, `src/FPUpdater.mm` | settings, About, new versions |

## Building (on a Mac with Xcode)

```sh
bash scripts/fetch-pdfium.sh ios
cmake -S . -B build-ios -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DFEATHER_IOS_SDK=device -DCMAKE_BUILD_TYPE=Release
cmake --build build-ios --target ipa        # dist/FeatherPDF-<version>-ios.ipa
```

For the simulator: `-DCMAKE_OSX_SYSROOT=iphonesimulator -DFEATHER_IOS_SDK=simulator`. `.github/workflows/ios.yml`
does both and runs the build check on a simulator.

## Putting it on an iPhone (sideloading from a Mac)

The `.ipa` is not signed for the App Store; a Mac signs it with your own Apple ID:

* **Sideloadly** (sideloadly.io): connect the iPhone by cable, drop the `.ipa` on Sideloadly, type your Apple ID, Start.
* **AltStore** (altstore.io): install AltServer on the Mac and AltStore on the iPhone, then open the `.ipa` in AltStore.

Then on the iPhone: Settings > General > VPN & Device Management > your Apple ID > Trust; and on iOS 16 or later
Settings > Privacy & Security > Developer Mode > On (the iPhone restarts). With a free Apple ID the app must be signed
again every 7 days (AltStore does it by itself over Wi-Fi; Sideloadly can too). Installing a newer `.ipa` the same way
keeps your settings and recent documents.
