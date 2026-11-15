#pragma once
// Writing a page's content stream again after its objects were changed, without moving any letter.
#include "pdfium_api.hpp"

class Document;

namespace contentgen {

// Like FPDFPage_GenerateContent, but text keeps its spacing. PDFium writes each text object as one plain string, which
// drops word and letter spacing (Tw, Tc) and kerning (numbers in TJ arrays): justified and kerned lines elsewhere on the
// page would shift. Text objects whose letters are not where plain advances put them are first split into pieces that
// each start exactly where their first letter is. Objects that cannot be split faithfully (ligatures, letters without a
// Unicode value, clipped text) are left as they are.
bool generate(FPDF_DOCUMENT doc, FPDF_PAGE page);

// The same, checked: PDFium cannot write every kind of content back (a fill with a pattern - most gradients - becomes
// black; text in a Type3 font, often symbols and emoji, is left out). The page is drawn as it is held in memory, written,
// read back from a copy and drawn again; where they differ, each object PDFium could not write is set as a picture of
// itself, drawn at the same place and depth, and the page is written and checked again. False when the written page still
// does not look as it should (the caller puts the document back).
bool generate(Document& d, FPDF_PAGE page);

// Whether `o` is a picture generate() made of a drawing PDFium could not write (marked FeatherDrawing): it stands for that
// drawing, so it is not offered as a picture to move or describe, and does not make a page look scanned.
bool drawnAsPicture(FPDF_PAGEOBJECT o);

// Only the splitting, without writing the content. Object indices taken after this stay valid through generate(), as
// there is then nothing left to split. True when objects were split (indices taken before are then stale).
// With `plainText`, objects are split at kerning too: for text that is about to be set again as a plain string.
bool prepare(FPDF_DOCUMENT doc, FPDF_PAGE page, bool plainText = false);

} // namespace contentgen
