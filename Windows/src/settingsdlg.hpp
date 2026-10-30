#pragma once
// The Settings window: categories on the left, their settings on the right. Every change applies at once (and is saved by
// the caller's setter), so there is no OK / Cancel; Close leaves.
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

namespace settingsdlg {

struct Item {
    enum Kind { Toggle, Choice, Button } kind = Toggle;
    std::wstring label, help;
    std::vector<std::wstring> choices;   // Choice
    std::function<int()> get;            // Toggle: 0 / 1; Choice: the index shown
    std::function<void(int)> set;        // called with the new value
    std::function<void()> press;         // Button
};

struct Category {
    std::wstring name;
    std::vector<Item> items;
};

// Shows the window over `parent` with `first` chosen; returns when it is closed.
void run(HWND parent, HINSTANCE inst, const std::vector<Category>& cats, int first, int dpi, HFONT font, HFONT bold);

} // namespace settingsdlg
