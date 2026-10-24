#pragma once
// Run-time binding to pdfium.dll. Loading it dynamically lets the app show a clear message
// instead of failing to start when the DLL is missing or damaged.
#include <string>
#include "fpdfview.h"

namespace pdfium {

struct Api {
    void (*InitLibrary)();
    void (*DestroyLibrary)();
    unsigned long (*GetLastError)();
    FPDF_DOCUMENT (*LoadMemDocument64)(const void*, size_t, FPDF_BYTESTRING);
    void (*CloseDocument)(FPDF_DOCUMENT);
    int (*GetPageCount)(FPDF_DOCUMENT);
    int (*GetPageSizeByIndexF)(FPDF_DOCUMENT, int, FS_SIZEF*);
    FPDF_PAGE (*LoadPage)(FPDF_DOCUMENT, int);
    void (*ClosePage)(FPDF_PAGE);
    void (*RenderPageBitmap)(FPDF_BITMAP, FPDF_PAGE, int, int, int, int, int, int);
    FPDF_BITMAP (*BitmapCreateEx)(int, int, int, void*, int);
    void (*BitmapFillRect)(FPDF_BITMAP, int, int, int, int, FPDF_DWORD);
    void (*BitmapDestroy)(FPDF_BITMAP);
};

// Looks for pdfium.dll next to the exe. On failure returns false and fills `error`.
bool load(std::wstring& error);
const Api& api();

} // namespace pdfium
