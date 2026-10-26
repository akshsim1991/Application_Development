#include "buildinfo.hpp"
#include "buildinfo.h"   // written at build time (cmake/buildinfo.cmake)

const wchar_t* buildDate() { return L"" FEATHER_BUILD_DATE_LONG; }
