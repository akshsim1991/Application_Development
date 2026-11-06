#pragma once
// Sensitive information in a document's text: e-mail addresses, phone numbers, Aadhaar numbers, PAN, bank account
// numbers (and IFSC codes). Found on every page, shown to the user, and the chosen ones marked for redaction (the
// redaction itself is redact::apply, which removes the text from the file for good).
#include <string>
#include <vector>
#include "document.hpp"

namespace sensitive {

enum Kind : unsigned { Email = 1, Phone = 2, Aadhaar = 4, Pan = 8, Bank = 16, All = 31 };

struct Hit {
    Kind kind = Email;
    int page = 0, start = 0, count = 0;   // characters of the page's text layer
    std::wstring text;                    // as it appears on the page
};

// What each kind is called, for lists and reports ("E-mail address", ...).
const wchar_t* name(Kind k);
// The kinds in the order they are offered.
const std::vector<Kind>& kinds();

// Every match of the chosen kinds, in page order. A number found as one kind is not reported again as another.
std::vector<Hit> scan(Document& d, unsigned kinds = All);
// The same, on a plain string (page 0): for the self-check and for text that is not in a document.
std::vector<Hit> scanText(const std::wstring& text, unsigned kinds = All);

// Adds redaction marks over each hit. Returns how many marks were added.
int mark(Document& d, const std::vector<Hit>& hits);

// "98•••••210": the first and last characters kept, for lists that should not show the whole number again.
std::wstring masked(const std::wstring& s);

// Checks used by scan, exposed for tests.
bool verhoeffValid(const std::wstring& digits);
bool panValid(const std::wstring& s);

} // namespace sensitive
