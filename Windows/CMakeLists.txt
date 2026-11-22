# Feather PDF for Windows: the Win32 screens in src/, the PDF engine in ../shared, the setup program in installer/.
set(PDFIUM_DIR "${CMAKE_SOURCE_DIR}/third_party/pdfium")
set(SHARED "${CMAKE_SOURCE_DIR}/shared")

add_executable(FeatherPDF WIN32
  src/main.cpp
  ${SHARED}/pdfium_api.cpp
  ${SHARED}/document.cpp
  src/viewer.cpp
  src/sidebar.cpp
  ${SHARED}/annot.cpp
  src/formdialog.cpp
  ${SHARED}/pageobjects.cpp
  ${SHARED}/equation.cpp
  ${SHARED}/exporter.cpp
  src/speech.cpp
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
  ${SHARED}/compress.cpp
  ${SHARED}/protect.cpp
  ${SHARED}/ocr.cpp
  ${SHARED}/sign.cpp
  src/sigpad.cpp
  ${SHARED}/outline.cpp
  ${SHARED}/foldersearch.cpp
  ${SHARED}/batch.cpp
  ${SHARED}/formcreate.cpp
  ${SHARED}/formdata.cpp
  ${SHARED}/visualdiff.cpp
  ${SHARED}/textpdf.cpp
  ${SHARED}/topdf.cpp
  ${SHARED}/sanitize.cpp
  ${SHARED}/picture.cpp
  src/printing.cpp
  ${SHARED}/pdfa.cpp
  ${SHARED}/fontlist.cpp
  src/inlineedit.cpp
  ${SHARED}/contentgen.cpp
  ${SHARED}/buildinfo.cpp
  src/icons.cpp
  src/startpage.cpp
  ${SHARED}/tables.cpp
  ${SHARED}/headfoot.cpp
  ${SHARED}/platform.cpp
  ${SHARED}/selftest.cpp
  ${SHARED}/cli.cpp
  ${SHARED}/crypto.cpp
  ${SHARED}/image.cpp
  src/hfdialog.cpp
  src/eqeditor.cpp
  src/themeui.cpp
  src/settingsdlg.cpp
  src/tableeditor.cpp
  src/updater.cpp
  src/officepdf.cpp
  ${SHARED}/update.cpp
  resources/FeatherPDF.rc)

target_include_directories(FeatherPDF PRIVATE src "${SHARED}" "${PDFIUM_DIR}/include" "${CMAKE_BINARY_DIR}")
target_compile_definitions(FeatherPDF PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  FEATHER_VERSION="${PROJECT_VERSION}" FEATHER_CODENAME="${FEATHER_CODENAME}" FEATHER_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  FEATHER_VERSION_MINOR=${PROJECT_VERSION_MINOR} FEATHER_VERSION_PATCH=${PROJECT_VERSION_PATCH})
add_dependencies(FeatherPDF buildinfo)
# The programs stay in the top of the build folder, where the tests and the installer look for them.
set_target_properties(FeatherPDF PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
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

target_link_libraries(FeatherPDF PRIVATE user32 gdi32 gdiplus msimg32 uuid bcrypt crypt32 ncrypt cryptui comdlg32 shell32 comctl32 ole32 oleaut32 uxtheme dwmapi winspool winhttp shlwapi)

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

# ---- the installer: a setup program with the files packed into it (installer/README in installer/pack.py) ----
add_executable(FeatherPDFSetup WIN32 installer/setup.cpp installer/inflate.cpp installer/setup.rc)
target_include_directories(FeatherPDFSetup PRIVATE installer src "${CMAKE_BINARY_DIR}")
target_compile_definitions(FeatherPDFSetup PRIVATE UNICODE _UNICODE NOMINMAX)
add_dependencies(FeatherPDFSetup buildinfo)
set_target_properties(FeatherPDFSetup PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_source_files_properties(installer/setup.rc PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/buildinfo.h")
target_link_libraries(FeatherPDFSetup PRIVATE user32 gdi32 gdiplus shell32 ole32 oleaut32 comctl32 uuid dwmapi advapi32)
if(MINGW)
  target_link_options(FeatherPDFSetup PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()
# `ninja installer` writes dist/FeatherPDF-Setup-<version>.exe
find_package(Python3 COMPONENTS Interpreter)
set(SETUP_OUT "${CMAKE_SOURCE_DIR}/dist/FeatherPDF-Setup-${PROJECT_VERSION}.exe")
add_custom_target(installer
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/installer/pack.py" "$<TARGET_FILE_DIR:FeatherPDF>" "$<TARGET_FILE:FeatherPDFSetup>" "${SETUP_OUT}"
  DEPENDS FeatherPDF FeatherPDFSetup
  COMMENT "Packing the installer")
