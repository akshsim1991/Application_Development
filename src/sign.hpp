#pragma once
// Digital signatures: signing with a certificate, and checking the signatures of an opened file.
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>

namespace sign {

struct Options {
    std::wstring reason;
    std::wstring location;
};

// `pdf` is a complete PDF as written by PDFium (plain objects, classic xref table, no password). The signature is
// added as an incremental update, so every original byte stays as it was. `cert` needs a usable private key.
bool signPdf(const std::vector<unsigned char>& pdf, PCCERT_CONTEXT cert, const Options& opt,
             std::vector<unsigned char>& out, std::wstring& error);

struct Status {
    enum Kind { Valid, ValidUntrusted, Changed, Unreadable } kind = Unreadable;
    std::wstring signer;       // who signed
    std::wstring time;         // when (as claimed by the signer)
    std::wstring reason;
    std::wstring note;         // what to make of it
};

// Checks every signature of a PDF file's bytes (as read from disk).
std::vector<Status> check(const std::vector<unsigned char>& file);

// Certificates: from a .pfx / .p12 file (returns the certificate with a private key; the store handle is kept
// in `store` and must be closed by the caller), and a new self-made certificate saved in the personal store.
PCCERT_CONTEXT certFromPfx(const std::wstring& path, const std::wstring& password, HCERTSTORE& store, std::wstring& error);
PCCERT_CONTEXT makeOwnCertificate(const std::wstring& name, const std::wstring& email, std::wstring& error);
std::wstring subjectName(PCCERT_CONTEXT cert);

} // namespace sign
