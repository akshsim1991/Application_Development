# Feather PDF for iPhone and iPad: the UIKit screens in src/ (Objective-C++), the PDF engine in ../shared.
# Configure with -DCMAKE_SYSTEM_NAME=iOS (-DFEATHER_IOS_SDK=simulator and CMAKE_OSX_SYSROOT=iphonesimulator for the
# simulator); `--target ipa` writes dist/FeatherPDF-<version>-ios.ipa. See iOS/README.md.
include(${CMAKE_SOURCE_DIR}/shared/engine.cmake)
set(IOS_VERSION ${PROJECT_VERSION})
if(FEATHER_IOS_SDK STREQUAL "simulator")
  set(IOS_PLATFORM iPhoneSimulator)
else()
  set(IOS_PLATFORM iPhoneOS)
endif()
string(TOLOWER ${IOS_PLATFORM} IOS_PLATFORM_LOWER)

file(GLOB ICONS "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.png")
add_executable(FeatherPDFiOS MACOSX_BUNDLE
  src/FPApp.mm
  src/FPEngine.mm
  src/FPFiles.mm
  src/FPPagesView.mm
  src/FPSettings.mm
  src/FPSettingsController.mm
  src/FPSheets.mm
  src/FPUpdater.mm
  src/FPViewer.mm
  ${ENGINE_SOURCES}
  ${ICONS})
set_source_files_properties(${ICONS} PROPERTIES MACOSX_PACKAGE_LOCATION "")   # (iOS apps are flat: the bundle's top)
set_target_properties(FeatherPDFiOS PROPERTIES
  OUTPUT_NAME FeatherPDF
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
target_include_directories(FeatherPDFiOS PRIVATE src "${SHARED}" "${PDFIUM_DIR}/include" "${CMAKE_BINARY_DIR}")
target_compile_definitions(FeatherPDFiOS PRIVATE FEATHER_VERSION="${IOS_VERSION}" FEATHER_CODENAME="${FEATHER_CODENAME}")
target_compile_options(FeatherPDFiOS PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc> -ffp-contract=off)
add_dependencies(FeatherPDFiOS buildinfo)
target_link_libraries(FeatherPDFiOS PRIVATE "-framework UIKit" "-framework Foundation" "-framework CoreGraphics" "-framework QuartzCore"
  "-framework ImageIO" "-framework CoreFoundation" "-framework CoreText" "-framework Security" "-framework UniformTypeIdentifiers")

# PDFium goes inside the app as a framework (iOS loads libraries only from the app's Frameworks folder, inside a
# framework): Frameworks/pdfium.framework/pdfium, its install name changed to match, with its own Info.plist. The
# licences go in the app (About > Open-source licences). Then everything is signed "ad hoc": the simulator needs it,
# and on a phone Sideloadly, AltStore or Xcode signs it again with the user's Apple ID.
set(FW "$<TARGET_BUNDLE_DIR:FeatherPDFiOS>/Frameworks/pdfium.framework")
add_custom_command(TARGET FeatherPDFiOS POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${FW}"
  COMMAND ${CMAKE_COMMAND} -E copy "${PDFIUM_DIR}/lib/libpdfium.dylib" "${FW}/pdfium"
  COMMAND install_name_tool -id @rpath/pdfium.framework/pdfium "${FW}/pdfium"
  COMMAND ${CMAKE_COMMAND} -DOUT=${FW}/Info.plist -DPLATFORM=${IOS_PLATFORM} -P "${CMAKE_CURRENT_SOURCE_DIR}/framework-plist.cmake"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PDFIUM_DIR}/licenses" "$<TARGET_BUNDLE_DIR:FeatherPDFiOS>/licenses"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/shared/fonts/GUST-FONT-LICENSE.txt" "$<TARGET_BUNDLE_DIR:FeatherPDFiOS>/licenses/Latin_Modern_Math_GUST.txt"
  COMMAND codesign --force --sign - "${FW}"
  COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:FeatherPDFiOS>")

# `cmake --build . --target ipa`: Payload/FeatherPDF.app in a zip, the form Sideloadly, AltStore and others install.
add_custom_target(ipa
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist"
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/make-ipa.sh" "$<TARGET_BUNDLE_DIR:FeatherPDFiOS>" "${CMAKE_SOURCE_DIR}/dist/FeatherPDF-${IOS_VERSION}-ios.ipa"
  DEPENDS FeatherPDFiOS
  COMMENT "Making the .ipa")
