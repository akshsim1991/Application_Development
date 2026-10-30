# The engine files that build off Windows (macOS app, and the engine check on Linux). The rest of shared/ still uses
# Windows services (pictures through GDI+, cryptography, OCR, web time stamps): see macOS/PORTING.md for their stages.
set(SHARED "${CMAKE_SOURCE_DIR}/shared")
set(ENGINE_SOURCES
  ${SHARED}/pdfium_api.cpp
  ${SHARED}/platform.cpp
  ${SHARED}/document.cpp
  ${SHARED}/pageobjects.cpp
  ${SHARED}/contentgen.cpp
  ${SHARED}/tables.cpp
  ${SHARED}/headfoot.cpp
  ${SHARED}/annot.cpp
  ${SHARED}/outline.cpp
  ${SHARED}/redact.cpp
  ${SHARED}/sanitize.cpp
  ${SHARED}/formdata.cpp
  ${SHARED}/formcreate.cpp
  ${SHARED}/textpdf.cpp
  ${SHARED}/fontlist.cpp
  ${SHARED}/buildinfo.cpp
  ${SHARED}/selftest.cpp
  ${SHARED}/pending.cpp)

# PDFium for this system (scripts/fetch-pdfium.sh mac|linux puts it here; third_party/pdfium is the Windows one).
if(APPLE)
  set(PDFIUM_DIR "${CMAKE_SOURCE_DIR}/third_party/pdfium-mac")
else()
  set(PDFIUM_DIR "${CMAKE_SOURCE_DIR}/third_party/pdfium-linux")
endif()
