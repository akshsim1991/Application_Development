# Writes buildinfo.h with today's date; run on every build, the file changes only when the date does.
string(TIMESTAMP ISO "%Y-%m-%d")
string(TIMESTAMP DAY "%d")
string(TIMESTAMP MONTH "%m")
string(TIMESTAMP YEAR "%Y")
math(EXPR M "${MONTH} - 1")
math(EXPR D "${DAY} + 0")
set(NAMES January February March April May June July August September October November December)
list(GET NAMES ${M} MONTHNAME)
string(REPLACE "." ";" PARTS "${VERSION}")
list(GET PARTS 0 V0)
list(GET PARTS 1 V1)
list(GET PARTS 2 V2)
file(WRITE "${OUT}.tmp" "// Generated at build time by cmake/buildinfo.cmake.\n#pragma once\n"
  "#define FEATHER_BUILD_DATE \"${ISO}\"\n#define FEATHER_BUILD_DATE_LONG \"${D} ${MONTHNAME} ${YEAR}\"\n"
  "#define FEATHER_VERSION_STR \"${VERSION}\"\n#define FEATHER_VERSION_NUM ${V0},${V1},${V2},0\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OUT}.tmp" "${OUT}")
file(REMOVE "${OUT}.tmp")
