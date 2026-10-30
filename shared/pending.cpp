// Off Windows: the parts of the engine that still use Windows' own services (its certificates, for signatures). Until
// their Mac versions are written they say so plainly instead of doing something half-way.
#ifndef _WIN32
#include "sign.hpp"

namespace sign {
std::vector<Status> check(const std::vector<unsigned char>&) {
    Status s;
    s.kind = Status::Unreadable;
    s.note = L"This file is signed, but checking signatures is not available in the Mac version yet.";
    return {s};
}
} // namespace sign
#endif
