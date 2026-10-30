# The engine check: the shared engine built for this system with the same test commands as the Windows app
# (compare.sh runs both and compares them).
include(${CMAKE_SOURCE_DIR}/shared/engine.cmake)
add_executable(engine_check main.cpp ${ENGINE_SOURCES})
target_include_directories(engine_check PRIVATE "${SHARED}" "${PDFIUM_DIR}/include" "${CMAKE_BINARY_DIR}")
target_compile_definitions(engine_check PRIVATE FEATHER_VERSION="${PROJECT_VERSION}")
add_dependencies(engine_check buildinfo)
target_compile_options(engine_check PRIVATE -ffp-contract=off)   # computes as the Windows app does
find_package(OpenSSL REQUIRED COMPONENTS Crypto)   # the cryptography (crypto.cpp) off Windows and the Mac
find_package(PNG REQUIRED)   # pictures (image.cpp) on Linux: PNG only
target_link_libraries(engine_check PRIVATE ${CMAKE_DL_LIBS} OpenSSL::Crypto PNG::PNG)
set_target_properties(engine_check PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_custom_command(TARGET engine_check POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PDFIUM_DIR}/lib/libpdfium.so" "${CMAKE_BINARY_DIR}")
