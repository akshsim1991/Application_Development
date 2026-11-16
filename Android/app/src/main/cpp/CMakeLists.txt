# libfeatherpdf.so: the shared engine (the same files as the Windows and Mac apps) and the bridge to the Android app.
# Built by the Android Gradle plugin for each ABI; also on a desktop Linux with a JDK (FEATHER_HOST_TEST), where
# tests/android/bridge_check.sh drives it from Java with the system services given as on Android.
cmake_minimum_required(VERSION 3.22)
project(featherpdf CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(FEATHER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
include(${FEATHER_ROOT}/shared/engine.cmake)

# the version, from the repository's CMakeLists.txt (one number for every app)
file(READ "${FEATHER_ROOT}/CMakeLists.txt" ROOT_CMAKE)
string(REGEX MATCH "project\\(FeatherPDF VERSION ([0-9.]+)" _ "${ROOT_CMAKE}")
set(FEATHER_APP_VERSION "${CMAKE_MATCH_1}")
add_custom_target(buildinfo ALL
  COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_BINARY_DIR}/buildinfo.h -DVERSION=${FEATHER_APP_VERSION} -P ${FEATHER_ROOT}/cmake/buildinfo.cmake
  BYPRODUCTS ${CMAKE_BINARY_DIR}/buildinfo.h)

add_library(featherpdf SHARED bridge.cpp ${ENGINE_SOURCES})
add_dependencies(featherpdf buildinfo)
target_include_directories(featherpdf PRIVATE "${SHARED}" "${PDFIUM_DIR}/include" "${CMAKE_BINARY_DIR}")
target_compile_definitions(featherpdf PRIVATE FEATHER_VERSION="${FEATHER_APP_VERSION}" FEATHER_SYSTEM_HOOKS=1)
target_compile_options(featherpdf PRIVATE -ffp-contract=off -fvisibility=hidden)   # computes as the Windows app does
if(ANDROID)
  target_link_libraries(featherpdf PRIVATE log dl)
  target_link_options(featherpdf PRIVATE "-Wl,-z,max-page-size=16384")   # 16 KB pages (Android 15 devices)
else()
  find_package(JNI REQUIRED)
  target_include_directories(featherpdf PRIVATE ${JNI_INCLUDE_DIRS})
  target_link_libraries(featherpdf PRIVATE ${CMAKE_DL_LIBS})
endif()
