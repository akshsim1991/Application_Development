cmake_minimum_required(VERSION 3.16)
project(FeatherPDF VERSION 2.23.3 LANGUAGES CXX)
# Each minor version (2.15, 2.16, ...) has a name of its own, shown with the number in About and the status bar.
set(FEATHER_CODENAME "Explorer")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The layout: shared/ is the PDF engine both apps use (tables, header and footer, page objects, forms, ...),
# Windows/ the Windows app and its installer, macOS/ the Mac app.

# The build date (About, file properties): buildinfo.h is written on every build and changes when the date does.
add_custom_target(buildinfo ALL
  COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_BINARY_DIR}/buildinfo.h -DVERSION=${PROJECT_VERSION} -P ${CMAKE_SOURCE_DIR}/cmake/buildinfo.cmake
  BYPRODUCTS ${CMAKE_BINARY_DIR}/buildinfo.h)

if(WIN32)
  enable_language(RC)
  add_subdirectory(Windows)
elseif(APPLE)
  enable_language(OBJCXX)
  add_subdirectory(macOS)
else()
  # Linux: only the engine check (tests/engine), which shows the shared code builds and works off Windows.
  add_subdirectory(tests/engine)
endif()
