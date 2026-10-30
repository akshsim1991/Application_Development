#pragma once
// Read-aloud through the Windows speech voices (SAPI).
#include <string>

namespace speech {

// Starts reading `text`. `rate` runs from -10 (slow) to 10 (fast). On failure returns false and fills
// `error` with a message that says what to do about it.
bool start(const std::wstring& text, int rate, std::wstring& error);
void stop();
bool speaking();
bool paused();
void pauseResume();

} // namespace speech
