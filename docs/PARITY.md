# 2.12 vs 2.13: parity checklist

Source of the 2.12 column: the installed 2.12 program run under Wine and the screenshots of the real 2.12 (menus, tool tips and
dialogs), not its source code. Status: **done** · **partial** (exists but differs) · **missing**.

## Window and navigation
| 2.12 | 2.13 | Status |
|---|---|---|
| Tabs, new tab, close tab | yes | done |
| Icon toolbar: Open, Save, Undo, Redo, Edit PDF, Annotate, Comment, Geometry, Side by side, page box, zoom, search, "..." menu | same layout, every item has an icon, hover highlight | done |
| Zoom drop-down (50 % ... 400 %, fit width / page, actual size) | yes | done |
| Main "..." menu (open, new from pictures, recent, save, export, compare, side by side, print, reload, read aloud, sign, copy, select all, copy page / area, navigation, panels, page layout, rotate, fit, full screen, page colours, theme, command palette, default viewer, About, exit) | yes | done |
| Page layout: single / continuous / two pages, cover page | yes | done |
| Rotate view left / right (Ctrl+L / Ctrl+R) | rotates the pages (an undoable change that Save keeps) | partial |
| Back / Forward (Alt+Left / Alt+Right) | yes | done |
| Reload from disk | yes (asks first when there are unsaved changes) | partial: no automatic "file changed" notice |
| Command palette (Ctrl+K) | yes, about 70 commands | done |
| Theme light / dark / like Windows, Settings, About | yes | done |
| Thumbnails: select several, drag to reorder, right-click menu, cut / copy / paste pages between tabs | yes | done |
| Find (match case) | current tab | partial: no "all tabs" |
| Merge open tabs into one document, move a tab to a new window | - | missing |

## Viewing
| 2.12 | 2.13 | Status |
|---|---|---|
| Right-click menu (copy, select all, search online, look up, highlight..., add link, add comment here, edit text, copy page / area as image, read aloud) | yes | done |
| Links inside documents (web pages and pages) are clickable | yes (web pages ask first) | done |
| Read aloud with voice choice, pause bar | read aloud, speed | partial |
| Print: printer, range, copies, pages per sheet (1, 2, 4, 6, 9), size, orientation | yes, no preview | partial |
| Document properties editable | read-only | partial |

## Annotating
| 2.12 | 2.13 | Status |
|---|---|---|
| Highlight, underline, strikethrough, squiggly, comments, shapes, arrow, pen, colours, widths, text size | yes | done |
| Stamps, insert picture | yes | done |
| Handwritten signature: type, draw, picture, remembered | yes | done |
| Link (drag a box) | web addresses | partial: no link to a page |
| Move and resize pictures, paragraphs and tables with alignment guides | yes (corners keep the proportions) | done |
| Select several annotations | one annotation | missing |

## Editing
| 2.12 | 2.13 | Status |
|---|---|---|
| Edit text line, add text box | yes | done |
| Paragraph editing (F2): reflow, font, size, bold, italic, colour, alignment | yes (standard fonts: Sans, Serif, Mono) | done |
| Find and replace text | yes (inside one text run) | partial |
| Insert, edit, delete tables | yes (tables made by Feather PDF) | partial |
| Add bookmark | yes | done |
| Fill in forms | yes | done |
| Create form fields (text, check box, drop-down), find blanks in flat forms, fill from a spreadsheet, export answers to Excel / CSV | yes (written by Feather PDF itself) | done |
| Undo / redo | 30 steps | done |

## Pages and tools
| 2.12 | 2.13 | Status |
|---|---|---|
| Delete, insert, rotate, move, crop, crop by drawing a box, extract, split, merge, insert from file, new PDF from pictures | yes | done |
| Pages per sheet, booklet (printing) | pages per sheet; no booklet | partial |
| Watermark, page numbers, compress, greyscale pictures, protect, redact, OCR, export | yes | done |
| Digital signatures: sign, own certificate, check, visible stamp, time stamp (RFC 3161) | yes | done |
| Geometry: ruler, protractor, units, drawing scale, rulers, keep / clear | yes | done |
| Compare two PDFs | words highlighted in a copy, plus a visual overlay of how the pages differ | done |
| Side by side, scroll and zoom together | yes | done |

## Beyond 2.12
Session restore after a crash (unsaved edits come back), remembered reading position per file, search all PDFs in a folder (with a saved
text index), a check that redacted text is really gone, batch mode with saved presets.

Picture tools (turn, flip, crop, brightness and contrast, black and white, replace, save as PNG, paste, front / back), page
tools (reverse, remove blank pages, find duplicates, headers and footers, background colour), remove hidden data with a
report, radio button / list box / date fields, form reset, flatten (links kept), polygon / cloud / callout / date stamps,
print preview with greyscale and two-sided printing, text and Markdown to PDF, and PDF/A-2b copies checked with veraPDF.
