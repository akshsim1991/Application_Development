#pragma once
// A small modal form built at run time: labelled text boxes, drop-down lists and check boxes.
#include <windows.h>
#include <string>
#include <vector>

struct FormField {
    enum Kind { Text, Password, Choice, Check, Note } kind = Text;
    std::wstring label;
    std::vector<std::wstring> choices;   // for Choice
    std::wstring text;                   // Text/Password value; Note shows it as plain text
    int choice = 0;                      // selected index for Choice
    bool checked = false;                // Check
};

// Shows the form centred on `parent`. Returns true on OK; the fields then hold the user's answers.
// `validate`, when given, is called on OK; it returns an error message to keep the form open, or empty to accept.
bool showForm(HWND parent, HINSTANCE inst, const wchar_t* title, std::vector<FormField>& fields,
              const wchar_t* okText = L"OK", std::wstring (*validate)(const std::vector<FormField>&) = nullptr);
