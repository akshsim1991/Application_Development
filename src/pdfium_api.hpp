#pragma once
// Run-time binding to pdfium.dll. Loading it dynamically lets the app show a clear message
// instead of failing to start when the DLL is missing or damaged.
#include <string>
#include "fpdfview.h"
#include "fpdf_text.h"
#include "fpdf_doc.h"
#include "fpdf_annot.h"
#include "fpdf_save.h"
#include "fpdf_edit.h"
#include "fpdf_ppo.h"
#include "fpdf_transformpage.h"

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
    X(FPDFDest_GetDestPageIndex) \
    X(FPDF_SaveAsCopy)           \
    X(FPDFPage_CreateAnnot)      \
    X(FPDFPage_CloseAnnot)       \
    X(FPDFPage_GetAnnotCount)    \
    X(FPDFPage_GetAnnot)         \
    X(FPDFPage_RemoveAnnot)      \
    X(FPDFAnnot_GetSubtype)      \
    X(FPDFAnnot_SetColor)        \
    X(FPDFAnnot_GetColor)        \
    X(FPDFAnnot_SetRect)         \
    X(FPDFAnnot_GetRect)         \
    X(FPDFAnnot_AppendAttachmentPoints) \
    X(FPDFAnnot_SetBorder)       \
    X(FPDFAnnot_SetFlags)        \
    X(FPDFAnnot_SetStringValue)  \
    X(FPDFAnnot_GetStringValue)  \
    X(FPDFAnnot_AddInkStroke)    \
    X(FPDF_CreateNewDocument)    \
    X(FPDFPage_New)              \
    X(FPDFPage_Delete)           \
    X(FPDFPage_GetRotation)      \
    X(FPDFPage_SetRotation)      \
    X(FPDF_MovePages)            \
    X(FPDF_ImportPagesByIndex)   \
    X(FPDFPage_GetMediaBox)      \
    X(FPDFPage_GetCropBox)       \
    X(FPDFPage_SetCropBox)       \
    X(FPDF_RenderPage)           \
    X(FPDF_GetMetaText)          \
    X(FPDF_GetFileVersion)       \
    X(FPDFText_GetUnicode)       \
    X(FPDFText_GetCharBox)       \
    X(FPDFText_GetFontSize)      \
    X(FPDFText_GetFontWeight)    \
    X(FPDFPageObj_GetType)       \
    X(FPDFImageObj_GetBitmap)    \
    X(FPDFImageObj_LoadJpegFileInline) \
    X(FPDFBitmap_GetBuffer)      \
    X(FPDFBitmap_GetWidth)       \
    X(FPDFBitmap_GetHeight)      \
    X(FPDFBitmap_GetStride)      \
    X(FPDFBitmap_GetFormat)      \
    X(FPDFTextObj_GetText)       \
    X(FPDFTextObj_GetFont)       \
    X(FPDFTextObj_GetFontSize)   \
    X(FPDFFont_GetBaseFontName)  \
    X(FPDFFont_GetIsEmbedded)    \
    X(FPDFFont_GetFlags)         \
    X(FPDFPageObj_GetMatrix)     \
    X(FPDFPageObj_SetMatrix)     \
    X(FPDFPageObj_GetFillColor)  \
    X(FPDFPage_CountObjects)     \
    X(FPDFPage_GetObject)        \
    X(FPDFPage_RemoveObject)     \
    X(FPDFPageObj_NewImageObj)   \
    X(FPDFImageObj_SetBitmap)    \
    X(FPDFImageObj_SetMatrix)    \
    X(FPDFPage_SetMediaBox)      \
    X(FPDFPageObj_NewTextObj)    \
    X(FPDFText_SetText)          \
    X(FPDFPageObj_SetFillColor)  \
    X(FPDFPageObj_Transform)     \
    X(FPDFPageObj_GetBounds)     \
    X(FPDFPageObj_Destroy)       \
    X(FPDFPage_InsertObject)     \
    X(FPDFPage_InsertObjectAtIndex) \
    X(FPDFPage_GenerateContent)

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
