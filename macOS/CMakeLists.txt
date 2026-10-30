# Feather PDF for macOS: the AppKit screens in src/ (Objective-C++), the PDF engine in ../shared.
# Builds "Feather PDF.app" for Apple silicon and Intel (CMAKE_OSX_ARCHITECTURES="arm64;x86_64"); see macOS/README.md.
include(${CMAKE_SOURCE_DIR}/shared/engine.cmake)
set(MAC_VERSION ${PROJECT_VERSION})   # the same version as the Windows app (top CMakeLists.txt)

set(GUIDE "${CMAKE_SOURCE_DIR}/docs/Feather PDF User Guide.pdf")
set(ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppIcon.icns")
add_executable(FeatherPDFMac MACOSX_BUNDLE
  src/main.mm
  src/AppDelegate.mm
  src/FPDocument.mm
  src/WindowController.mm
  src/PageView.mm
  src/Render.mm
  src/Trace.mm
  src/StartWindow.mm
  src/Form.mm
  src/Tools.mm
  src/Sidebar.mm
  src/ThumbMenu.mm
  src/SideBySide.mm
  src/OcrVision.mm
  src/Measure.mm
  src/Signatures.mm
  src/Batch.mm
  src/Updater.mm
  src/Annotate.mm
  src/Forms.mm
  src/PagesMore.mm
  src/Reading.mm
  src/Palette.mm
  src/Editing.mm
  src/TableSheet.mm
  src/FolderSearch.mm
  src/Settings.mm
  ${ENGINE_SOURCES}
  "${ICON}"
  "${GUIDE}")
set_source_files_properties("${ICON}" "${GUIDE}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(FeatherPDFMac PROPERTIES
  OUTPUT_NAME "Feather PDF"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(FeatherPDFMac PRIVATE src "${SHARED}" "${PDFIUM_DIR}/include" "${CMAKE_BINARY_DIR}")
target_compile_definitions(FeatherPDFMac PRIVATE FEATHER_VERSION="${MAC_VERSION}")
# -ffp-contract=off: Apple silicon would otherwise fuse multiply-adds, and positions would differ from the Windows
# app's in the last digit (a table edge at 645.4 instead of 645.3).
target_compile_options(FeatherPDFMac PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc> -ffp-contract=off)
add_dependencies(FeatherPDFMac buildinfo)
target_link_libraries(FeatherPDFMac PRIVATE "-framework Cocoa" "-framework CoreGraphics" "-framework ImageIO" "-framework CoreFoundation"
  "-framework UniformTypeIdentifiers" "-framework Security" "-framework AVFoundation" "-framework Vision" "-framework CoreText")

# PDFium goes inside the app (Contents/Frameworks), where the app looks for it; then the whole app is signed for this
# Mac ("ad hoc"): Apple silicon runs nothing unsigned. A Developer ID signature replaces this for a public download.
add_custom_command(TARGET FeatherPDFMac POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:FeatherPDFMac>/Frameworks"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PDFIUM_DIR}/lib/libpdfium.dylib" "$<TARGET_BUNDLE_CONTENT_DIR:FeatherPDFMac>/Frameworks/"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PDFIUM_DIR}/licenses" "$<TARGET_BUNDLE_CONTENT_DIR:FeatherPDFMac>/Resources/licenses"
  COMMAND codesign --force --sign - "$<TARGET_BUNDLE_CONTENT_DIR:FeatherPDFMac>/Frameworks/libpdfium.dylib"
  COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:FeatherPDFMac>")

# `cmake --build . --target dmg` writes dist/FeatherPDF-<version>-mac.dmg: the app and a link to Applications.
add_custom_target(dmg
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist"
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/make-dmg.sh" "$<TARGET_BUNDLE_DIR:FeatherPDFMac>" "${CMAKE_SOURCE_DIR}/dist/FeatherPDF-${MAC_VERSION}-mac.dmg"
  DEPENDS FeatherPDFMac
  COMMENT "Making the disk image")
