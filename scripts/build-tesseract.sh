#!/usr/bin/env bash
# Builds the OCR engine (Tesseract + Leptonica, static, no other dependencies) with mingw-w64 into
# third_party/tesseract, and fetches the English language data. Optional: without it Feather PDF still builds,
# only "Recognise text (OCR)" is unavailable.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/third_party/tesseract"
work="${TMPDIR:-/tmp}/feather-tess"
tc="$root/cmake/mingw-w64.cmake"
mkdir -p "$work" "$out/include/tesseract" "$out/include/leptonica" "$out/lib" "$out/tessdata" "$out/licenses"
cd "$work"
[ -d leptonica ] || git clone --depth 1 https://github.com/DanBloomberg/leptonica
[ -d tesseract ] || git clone --depth 1 https://github.com/tesseract-ocr/tesseract
inst="$work/inst"
cmake -S leptonica -B lb -G Ninja -DCMAKE_TOOLCHAIN_FILE="$tc" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
  -DSW_BUILD=OFF -DBUILD_PROG=OFF -DCMAKE_INSTALL_PREFIX="$inst" \
  -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON -DCMAKE_DISABLE_FIND_PACKAGE_PNG=ON -DCMAKE_DISABLE_FIND_PACKAGE_JPEG=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_TIFF=ON -DCMAKE_DISABLE_FIND_PACKAGE_GIF=ON -DCMAKE_DISABLE_FIND_PACKAGE_WebP=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_OpenJPEG=ON
cmake --build lb && cmake --install lb
# Configure twice: the first pass can fail to see the Leptonica package (known quirk of its generated config).
for i in 1 2; do
  cmake -S tesseract -B tb -G Ninja -DCMAKE_TOOLCHAIN_FILE="$tc" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DSW_BUILD=OFF -DBUILD_TRAINING_TOOLS=OFF -DBUILD_TESTS=OFF -DDISABLE_ARCHIVE=ON -DDISABLE_CURL=ON -DOPENMP_BUILD=OFF \
    -DGRAPHICS_DISABLED=ON -DDISABLED_LEGACY_ENGINE=ON -DLeptonica_DIR="$inst/lib/cmake/leptonica" -DCMAKE_PREFIX_PATH="$inst" && break
done
# Only the library is needed; the command line tool fails to link on case-sensitive hosts, so build just the library.
cmake --build tb --target libtesseract
cp tb/libtesseract*.a "$out/lib/libtesseract.a"
cp "$inst"/lib/libleptonica*.dll "$out/lib/libleptonica.a"     # a static archive despite the mingw file name
cp tesseract/include/tesseract/*.h tb/include/tesseract/version.h "$out/include/tesseract/"
cp "$inst"/include/leptonica/*.h "$out/include/leptonica/"
cp tesseract/LICENSE "$out/licenses/tesseract-APACHE-2.0.txt"
cp leptonica/leptonica-license.txt "$out/licenses/leptonica.txt"
curl -fsSL -o "$out/tessdata/eng.traineddata" https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/main/eng.traineddata
echo "OCR engine ready in $out"
