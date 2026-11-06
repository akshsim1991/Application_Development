#pragma once
// Comment threads and review: replies to a comment, its review status (Accepted, Rejected, Cancelled, Completed), and a
// review summary. Stored as the PDF standard has it, so other readers show them too: a reply is a note "in reply to"
// (/IRT) the comment, not drawn on the page; a status is a hidden note with /State and /StateModel /Review.
#include <string>
#include <vector>
#include "document.hpp"

namespace review {

enum class State { None, Accepted, Rejected, Cancelled, Completed };
const wchar_t* stateName(State s);

struct Reply {
    std::wstring author, text, date;
};

struct Thread {
    int page = 0;
    int object = -1;           // the comment's object number in the file (stable until the next change)
    std::wstring kind;         // Note, Highlight, Text box, Stamp, Drawing ...
    std::wstring author, text, date;
    std::wstring quoted;       // the text under a highlight, underline or strike-out
    State state = State::None;
    std::wstring stateBy;      // who set the status
    std::vector<Reply> replies;
};

std::vector<Thread> threads(Document& d);
// One undo step each. `author`: the name shown with it.
bool reply(Document& d, int object, const std::wstring& author, const std::wstring& text);
bool setState(Document& d, int object, State s, const std::wstring& author);
// The review summary in Markdown (for textpdf::build or to save): counts, then every comment by page with its replies.
std::wstring summary(Document& d);

} // namespace review
