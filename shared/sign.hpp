#pragma once
// Digital signatures: signing with a certificate, and checking the signatures of an opened file.
#include "platform.hpp"
#ifdef _WIN32
#include <wincrypt.h>
#endif
#include <functional>
#include <string>
#include <vector>

namespace sign {

struct Options {
    std::wstring reason;
    std::wstring location;
    // A visible signature: a stamp with the signer, date and reason, in this rectangle (page coordinates) of page `page`.
    bool visible = false;
    int page = 0;
    double left = 0, bottom = 0, right = 0, top = 0;
    // A trusted time stamp (RFC 3161) from this server, for example http://timestamp.digicert.com; empty for none.
    std::string timestampUrl;
};

// Who signs: the name shown in the signature, and the system's way of making a detached CMS (PKCS#7) signature of the
// given bytes with SHA-256, the certificate chain included (Windows: CryptSignMessage; the Mac: CMSEncoder).
struct Signer {
    std::wstring name;
    std::function<bool(const std::vector<unsigned char>& data, std::vector<unsigned char>& cms, std::wstring& error)> cms;
};

// `pdf` is a complete PDF as written by PDFium (plain objects, classic xref table, no password). The signature is
// added as an incremental update, so every original byte stays as it was.
bool signPdf(const std::vector<unsigned char>& pdf, const Signer& signer, const Options& opt,
             std::vector<unsigned char>& out, std::wstring& error);

// Off Windows the app gives the services the signatures need: sending a time stamp request, and checking a detached
// signature (2 valid and trusted here, 1 valid but not trusted here, 0 not matching; `signer` gets the signer's name).
struct System {
    std::function<bool(const std::string& url, const std::vector<unsigned char>& body, std::vector<unsigned char>& response,
                       std::wstring& error)> httpPost;
    std::function<int(const std::vector<unsigned char>& cms, const std::vector<unsigned char>& data, std::wstring& signer)> verify;
    // a certificate file with its key (.p12 / .pfx) as a signer (the command line, batch mode)
    std::function<bool(const std::wstring& path, const std::wstring& password, Signer& signer, std::wstring& error)> signerFromFile;
};
// The system's services (setSystem) as given; empty functions where there are none.
const System& system();
void setSystem(System system);

#ifdef _WIN32   // `cert` needs a usable private key
bool signPdf(const std::vector<unsigned char>& pdf, PCCERT_CONTEXT cert, const Options& opt,
             std::vector<unsigned char>& out, std::wstring& error);
#endif

struct Status {
    enum Kind { Valid, ValidUntrusted, Changed, Unreadable } kind = Unreadable;
    std::wstring signer;       // who signed
    std::wstring time;         // when (as claimed by the signer)
    std::wstring reason;
    std::wstring note;         // what to make of it
};

// Checks every signature of a PDF file's bytes (as read from disk).
std::vector<Status> check(const std::vector<unsigned char>& file);

#ifdef _WIN32
// Certificates: from a .pfx / .p12 file (returns the certificate with a private key; the store handle is kept
// in `store` and must be closed by the caller), and a new self-made certificate saved in the personal store.
PCCERT_CONTEXT certFromPfx(const std::wstring& path, const std::wstring& password, HCERTSTORE& store, std::wstring& error);
PCCERT_CONTEXT makeOwnCertificate(const std::wstring& name, const std::wstring& email, std::wstring& error);
std::wstring subjectName(PCCERT_CONTEXT cert);
#endif

} // namespace sign
