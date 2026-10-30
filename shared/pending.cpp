// Off Windows: the parts of the engine that still use Windows' own services (its cryptography for passwords and
// signatures). Until their Mac versions are written they say so plainly instead of doing something half-way.
#ifndef _WIN32
#include "protect.hpp"
#include "sign.hpp"

namespace protect {
bool encrypt(const std::vector<unsigned char>&, const Options&, std::vector<unsigned char>&, std::wstring& error) {
    error = L"Protecting a PDF with a password is not available in the Mac version yet. The file was not saved.";
    return false;
}
} // namespace protect

namespace sign {
std::vector<Status> check(const std::vector<unsigned char>&) {
    Status s;
    s.kind = Status::Unreadable;
    s.note = L"This file is signed, but checking signatures is not available in the Mac version yet.";
    return {s};
}
} // namespace sign
#endif
