#pragma once
// Read-aloud through the Windows speech voices (SAPI).
#include <functional>
#include <string>
#include <windows.h>

namespace speech {

// Starts reading `text`. `rate` runs from -10 (slow) to 10 (fast). On failure returns false and fills
// `error` with a message that says what to do about it.
bool start(const std::wstring& text, int rate, std::wstring& error);
// The same, posting `msg` to `notify` as the voice moves on; then call events() to hear where it is.
bool start(const std::wstring& text, int rate, std::wstring& error, HWND notify, UINT msg);
// The voice's news since the last call: f(0, position, length) for each word it starts, f(1, 0, 0) when it has finished.
void events(const std::function<void(int kind, size_t pos, size_t len)>& f);
void stop();
bool speaking();
bool paused();
void pauseResume();

} // namespace speech
