#pragma once
// Handwritten signatures as transparent PNG pictures: typed in a script font, or drawn with the mouse.
#include <windows.h>
#include <string>

namespace sigpad {

bool fromText(const std::wstring& name, const std::wstring& pngPath);
// Shows a drawing pad (modal). Returns true when the user pressed "Use this signature" and the PNG was written.
bool draw(HWND parent, HINSTANCE inst, const std::wstring& pngPath);

} // namespace sigpad
