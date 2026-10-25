#pragma once
// Writing a page's content stream again after its objects were changed, without moving any letter.
#include "pdfium_api.hpp"

namespace contentgen {

// Like FPDFPage_GenerateContent, but text keeps its spacing. PDFium writes each text object as one plain string, which
// drops word and letter spacing (Tw, Tc) and kerning (numbers in TJ arrays): justified and kerned lines elsewhere on the
// page would shift. Text objects whose letters are not where plain advances put them are first split into pieces that
// each start exactly where their first letter is. Objects that cannot be split faithfully (ligatures, letters without a
// Unicode value, clipped text) are left as they are.
bool generate(FPDF_DOCUMENT doc, FPDF_PAGE page);

// Only the splitting, without writing the content. Object indices taken after this stay valid through generate(), as
// there is then nothing left to split. True when objects were split (indices taken before are then stale).
// With `plainText`, objects are split at kerning too: for text that is about to be set again as a plain string.
bool prepare(FPDF_DOCUMENT doc, FPDF_PAGE page, bool plainText = false);

} // namespace contentgen
