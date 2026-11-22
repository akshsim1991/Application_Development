# The engine files that build off Windows (macOS app, and the engine check on Linux). Signatures and OCR take the
# system's services from the app (sign::setSystem, ocr::setEngine): see macOS/PORTING.md.
if(NOT FEATHER_ROOT)   # the repository's root (the Android app's native build sets it: its own CMake project is deeper)
  set(FEATHER_ROOT "${CMAKE_SOURCE_DIR}")
endif()
set(SHARED "${FEATHER_ROOT}/shared")
set(ENGINE_SOURCES
  ${SHARED}/pdfium_api.cpp
  ${SHARED}/platform.cpp
  ${SHARED}/document.cpp
  ${SHARED}/pageobjects.cpp
  ${SHARED}/equation.cpp
  ${SHARED}/contentgen.cpp
  ${SHARED}/tables.cpp
  ${SHARED}/headfoot.cpp
  ${SHARED}/annot.cpp
  ${SHARED}/outline.cpp
  ${SHARED}/redact.cpp
  ${SHARED}/sensitive.cpp
  ${SHARED}/search.cpp
  ${SHARED}/splitmerge.cpp
  ${SHARED}/textdiff.cpp
  ${SHARED}/attach.cpp
  ${SHARED}/webview.cpp
  ${SHARED}/access.cpp
  ${SHARED}/review.cpp
  ${SHARED}/readaloud.cpp
  ${SHARED}/formcalc.cpp
  ${SHARED}/sanitize.cpp
  ${SHARED}/formdata.cpp
  ${SHARED}/formcreate.cpp
  ${SHARED}/textpdf.cpp
  ${SHARED}/topdf.cpp
  ${SHARED}/fontlist.cpp
  ${SHARED}/buildinfo.cpp
  ${SHARED}/selftest.cpp
  ${SHARED}/cli.cpp
  ${SHARED}/crypto.cpp
  ${SHARED}/update.cpp
  ${SHARED}/protect.cpp
  ${SHARED}/image.cpp
  ${SHARED}/compress.cpp
  ${SHARED}/exporter.cpp
  ${SHARED}/picture.cpp
  ${SHARED}/pdfa.cpp
  ${SHARED}/visualdiff.cpp
  ${SHARED}/foldersearch.cpp
  ${SHARED}/ocr.cpp
  ${SHARED}/sign.cpp
  ${SHARED}/batch.cpp
  ${SHARED}/pending.cpp)

# PDFium for this system (scripts/fetch-pdfium.sh mac|linux puts it here; third_party/pdfium is the Windows one).
if(ANDROID)
  set(PDFIUM_DIR "${FEATHER_ROOT}/third_party/pdfium-android/${ANDROID_ABI}")
elseif(APPLE)
  set(PDFIUM_DIR "${FEATHER_ROOT}/third_party/pdfium-mac")
else()
  set(PDFIUM_DIR "${FEATHER_ROOT}/third_party/pdfium-linux")
endif()
