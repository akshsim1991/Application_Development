cmake_minimum_required(VERSION 3.16)
project(FeatherPDF VERSION 2.13.0 LANGUAGES CXX RC)

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
  resources/FeatherPDF.rc)

target_include_directories(FeatherPDF PRIVATE src "${PDFIUM_DIR}/include")
target_compile_definitions(FeatherPDF PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  FEATHER_VERSION="${PROJECT_VERSION}")
target_link_libraries(FeatherPDF PRIVATE user32 gdi32 gdiplus msimg32 comdlg32 shell32 comctl32 ole32)

if(MINGW)
  target_link_options(FeatherPDF PRIVATE -municode -static -static-libgcc -static-libstdc++)
endif()

# Ship pdfium.dll and the licenses next to the exe (the exe finds them there at run time).
add_custom_command(TARGET FeatherPDF POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PDFIUM_DIR}/bin/pdfium.dll" "$<TARGET_FILE_DIR:FeatherPDF>"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PDFIUM_DIR}/licenses" "$<TARGET_FILE_DIR:FeatherPDF>/licenses")
