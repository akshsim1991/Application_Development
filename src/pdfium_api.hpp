#pragma once
// Run-time binding to pdfium.dll. Loading it dynamically lets the app show a clear message
// instead of failing to start when the DLL is missing or damaged.
#include <string>
#include "fpdfview.h"
#include "fpdf_text.h"
#include "fpdf_doc.h"

// Every PDFium function the app calls. Add a name here and it is available as pdfium::api().NAME.
#define PDFIUM_FUNCS(X)          \
    X(FPDF_InitLibrary)          \
    X(FPDF_DestroyLibrary)       \
    X(FPDF_GetLastError)         \
    X(FPDF_LoadMemDocument64)    \
    X(FPDF_CloseDocument)        \
    X(FPDF_GetPageCount)         \
    X(FPDF_GetPageSizeByIndexF)  \
    X(FPDF_LoadPage)             \
    X(FPDF_ClosePage)            \
    X(FPDF_RenderPageBitmap)     \
    X(FPDF_PageToDevice)         \
    X(FPDF_DeviceToPage)         \
    X(FPDFBitmap_CreateEx)       \
    X(FPDFBitmap_FillRect)       \
    X(FPDFBitmap_Destroy)        \
    X(FPDFText_LoadPage)         \
    X(FPDFText_ClosePage)        \
    X(FPDFText_CountChars)       \
    X(FPDFText_GetText)          \
    X(FPDFText_GetCharIndexAtPos) \
    X(FPDFText_CountRects)       \
    X(FPDFText_GetRect)          \
    X(FPDFText_FindStart)        \
    X(FPDFText_FindNext)         \
    X(FPDFText_FindClose)        \
    X(FPDFText_GetSchResultIndex) \
    X(FPDFText_GetSchCount)      \
    X(FPDFBookmark_GetFirstChild) \
    X(FPDFBookmark_GetNextSibling) \
    X(FPDFBookmark_GetTitle)     \
    X(FPDFBookmark_GetDest)      \
    X(FPDFBookmark_GetAction)    \
    X(FPDFAction_GetDest)        \
    X(FPDFDest_GetDestPageIndex)

namespace pdfium {

struct Api {
#define PDFIUM_DECL(name) decltype(&name) name;
    PDFIUM_FUNCS(PDFIUM_DECL)
#undef PDFIUM_DECL
};

// Looks for pdfium.dll next to the exe. On failure returns false and fills `error`.
bool load(std::wstring& error);
const Api& api();

} // namespace pdfium
