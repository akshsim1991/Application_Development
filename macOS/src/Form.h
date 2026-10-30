#pragma once
// A small form shown as a sheet on the window: labelled text boxes, password boxes, pop-up lists, check boxes and notes,
// as the Windows app's forms (Windows/src/formdialog.hpp), so every tool asks the same questions on both.
#import <Cocoa/Cocoa.h>
#include <functional>
#include <string>
#include <vector>

namespace form {

struct Field {
    enum Kind { Text, Password, Choice, Check, Note } kind = Text;
    std::wstring label;
    std::vector<std::wstring> choices;   // Choice
    std::wstring text;                   // Text / Password value; Note: the text shown
    int choice = 0;                      // Choice: selected index
    bool checked = false;                // Check
};

using Fields = std::vector<Field>;
// Shows the form on `window` (a sheet); after OK, `done` gets the answers (the fields as filled in). Cancel calls nothing.
// `validate`, when given, returns a message that keeps the form open, or empty to accept. An OK text starting with "!"
// ("!Close") makes a form with that one button.
void show(NSWindow* window, const std::wstring& title, Fields fields, const std::wstring& okText,
          std::function<std::wstring(const Fields&)> validate, std::function<void(const Fields&)> done);

// A message on the window (a sheet). `warning` shows the caution sign.
void message(NSWindow* window, const std::wstring& text, bool warning = false, std::function<void()> done = nullptr);
// A yes / no question; `done(true)` for yes.
void ask(NSWindow* window, const std::wstring& text, const std::wstring& yes, std::function<void(bool)> done);

NSString* ns(const std::wstring& s);
std::wstring ws(NSString* s);

} // namespace form
