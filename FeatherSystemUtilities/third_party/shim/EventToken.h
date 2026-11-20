// MinGW-w64 lacks EventToken.h (needed by WebView2.h). Same layout as the Windows SDK definition.
#pragma once
#ifndef __EVENTTOKEN_SHIM__
#define __EVENTTOKEN_SHIM__
typedef struct EventRegistrationToken { __int64 value; } EventRegistrationToken;
#endif
