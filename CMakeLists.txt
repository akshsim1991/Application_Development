cmake_minimum_required(VERSION 3.16)
project(FeatherPDF VERSION 2.14.2 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PDFIUM_DIR "${CMAKE_SOURCE_DIR}/third_party/pdfium")

add_executable(FeatherPDF WIN32
  src/main.cpp
  src/pdfium_api.cpp
  src/document.cpp
  src/viewer.cpp
  src/sidebar.cpp
  src/annot.cpp
  src/formdialog.cpp
  src/pageobjects.cpp
  src/exporter.cpp
  src/speech.cpp
  src/redact.cpp
  src/compress.cpp
  src/protect.cpp
  src/ocr.cpp
  src/sign.cpp
  src/sigpad.cpp
  src/outline.cpp
  src/foldersearch.cpp
  src/batch.cpp
  src/formcreate.cpp
  src/formdata.cpp
  src/visualdiff.cpp
  src/textpdf.cpp
  src/sanitize.cpp
  src/picture.cpp
  src/printing.cpp
  src/pdfa.cpp
  src/fontlist.cpp
  src/inlineedit.cpp
  src/contentgen.cpp
  src/buildinfo.cpp
  src/icons.cpp
  src/startpage.cpp
  resources/FeatherPDF.rc)

target_include_directories(FeatherPDF PRIVATE src "${PDFIUM_DIR}/include" "${CMAKE_BINARY_DIR}")
target_compile_definitions(FeatherPDF PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  FEATHER_VERSION="${PROJECT_VERSION}" FEATHER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  FEATHER_VERSION_MINOR=${PROJECT_VERSION_MINOR} FEATHER_VERSION_PATCH=${PROJECT_VERSION_PATCH})
# The build date (About, file properties): buildinfo.h is written on every build and changes when the date does.
add_custom_target(buildinfo ALL
  COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_BINARY_DIR}/buildinfo.h -DVERSION=${PROJECT_VERSION} -P ${CMAKE_SOURCE_DIR}/cmake/buildinfo.cmake
  BYPRODUCTS ${CMAKE_BINARY_DIR}/buildinfo.h)
add_dependencies(FeatherPDF buildinfo)
set_source_files_properties(resources/FeatherPDF.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/buildinfo.h")
# Optional OCR engine (scripts/build-tesseract.sh). Without it the app builds and OCR reports that it is unavailable.
set(TESS_DIR "${CMAKE_SOURCE_DIR}/third_party/tesseract")
if(EXISTS "${TESS_DIR}/lib/libtesseract.a")
  target_compile_definitions(FeatherPDF PRIVATE FEATHER_OCR=1)
  target_include_directories(FeatherPDF PRIVATE "${TESS_DIR}/include")
  target_link_libraries(FeatherPDF PRIVATE "${TESS_DIR}/lib/libtesseract.a" "${TESS_DIR}/lib/libleptonica.a" ws2_32)
  add_custom_command(TARGET FeatherPDF POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TESS_DIR}/tessdata" "$<TARGET_FILE_DIR:FeatherPDF>/tessdata"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TESS_DIR}/licenses" "$<TARGET_FILE_DIR:FeatherPDF>/licenses")
endif()

target_link_libraries(FeatherPDF PRIVATE user32 gdi32 gdiplus msimg32 uuid bcrypt crypt32 ncrypt cryptui comdlg32 shell32 comctl32 ole32 uxtheme dwmapi winspool winhttp)

if(MINGW)
  target_link_options(FeatherPDF PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

# The user guide sits next to the exe (Help: User guide, F1).
add_custom_command(TARGET FeatherPDF POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/docs/Feather PDF User Guide.pdf" "$<TARGET_FILE_DIR:FeatherPDF>/Feather PDF User Guide.pdf")
# Ship pdfium.dll and the licenses next to the exe (the exe finds them there at run time).
add_custom_command(TARGET FeatherPDF POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PDFIUM_DIR}/bin/pdfium.dll" "$<TARGET_FILE_DIR:FeatherPDF>"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PDFIUM_DIR}/licenses" "$<TARGET_FILE_DIR:FeatherPDF>/licenses")
