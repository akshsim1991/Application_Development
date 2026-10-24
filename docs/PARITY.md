# 2.12 vs the 2.13 rebuild: parity checklist

Source of the 2.12 column: the installed 2.12 program run under Wine (its menus, tool tips and dialog texts), not its source code.
Status: **done** = works like 2.12 · **partial** = exists but differs · **missing** = not built yet.

## Window and navigation

| 2.12 | 2.13 | Status |
|---|---|---|
| Tabs, new tab, close tab, merge open tabs | tabs, new tab, close tab | partial (no merge of tabs, no move to new window) |
| Main menu ("..." at the right) | "Menu" button: open, recent, save, export, print, panels, view, page colours, theme, settings, about, exit | partial (no compare / side by side / command palette entries) |
| Toolbar groups: Edit PDF · Annotate · Comment · Geometry · Side by side | Annotate · Pages · Tools | partial (different grouping) |
| Page layout: continuous / single page / two pages | continuous only | **missing** |
| Rotate view left / right (Ctrl+L / Ctrl+R) | – | **missing** |
| Back / Forward (Alt+Left / Alt+Right) | – | **missing** |
| Reload from disk, "file changed by another program" | – | **missing** |
| Command palette (Ctrl+K) | – | **missing** |
| Theme: light / dark / like Windows (the window itself) | Light / Dark / Like Windows, plus Settings dialog and About | done |
| Thumbnails drag to reorder, select several pages, cut / copy / paste pages between tabs, right-click menu | all (right-click menu, Ctrl/Shift select, drag, Del, Ctrl+C/X/V, paste between tabs) | done |
| Zoom, fit width / page, actual size, full screen, presentation | all | done |
| Go to page, first / last page, bookmarks panel, thumbnails | all | done |
| Find with match case, all tabs, results across tabs | find, match case, current tab | partial (no "all tabs") |
| Recent files, drag and drop, one window | all | done |

## Viewing and reading

| 2.12 | 2.13 | Status |
|---|---|---|
| Copy page as image / copy area as image | copy page | partial (no area) |
| Look up, translate, Wikipedia on selected text | – | **missing** |
| Read aloud with voice and speed, pause | read aloud, speed | partial (no voice choice, no pause bar) |
| Print: copies, range, booklet, pages per sheet | print with range and copies | partial (no booklet / N-up) |
| Document properties (title, author, keywords editable) | read-only | partial |

## Annotating

| 2.12 | 2.13 | Status |
|---|---|---|
| Highlight, underline, strikethrough, squiggly | all | done |
| Comments, notes summary | comments, notes summary (Markdown file) | done |
| Rectangle, ellipse, line, arrow, pen, colours, widths | all | done |
| Text size for annotations | – | **missing** |
| Stamps (standard and custom) | standard and custom | done |
| Handwritten signature (draw / picture, remember) | – | **missing** |
| Insert picture, replace picture, delete picture, paste picture | insert | partial |
| Add link (drag a box, web page or page) | – | **missing** |
| Select several annotations, drag to move, change colour | select one, delete | partial |
| Measuring: ruler, protractor, units, drawing scale, keep on page | – | **missing** |

## Editing

| 2.12 | 2.13 | Status |
|---|---|---|
| Edit text, paragraph reflow (F2), bold / italic / font / size / colour / align | edit a line (same font where possible) | partial |
| Find and replace text | – | **missing** |
| Insert, edit, delete tables | – | **missing** |
| Add bookmark, rename, move, delete bookmarks | view only | **missing** |
| Fillable forms: fill in | fill in | done |
| Fillable forms: add text field, check box, option buttons, list, signature field | – | **missing** |
| Move / resize inserted pictures and text | – | **missing** |
| Undo / redo | 30 steps | done |

## Pages

| 2.12 | 2.13 | Status |
|---|---|---|
| Delete, insert blank, rotate, move, crop (also by drawing a box), extract, split, merge, insert from file | all except draw-a-box crop and split | partial |
| Pages per sheet, booklet | – | **missing** |
| New PDF from pictures | – | **missing** |

## Tools

| 2.12 | 2.13 | Status |
|---|---|---|
| Watermark, page numbers | both | done |
| Compress (by quality or target size), black and white / greyscale | compress; greyscale only as a compress option | partial |
| Password protection with limits | yes | done |
| Redaction (areas, selected text, find and mark, apply) | yes | done |
| OCR | yes (Tesseract, English bundled) | done |
| Export: pictures, text, Markdown, Word, Excel / CSV | yes | done |
| Digital signatures: sign, own certificate, check, time stamp, visible signature | sign, own certificate, check | partial (no time stamp, no visible stamp) |
| Compare two PDFs, mark the differences | – | **missing** |
| Side by side view, scroll and zoom together | – | **missing** |
| Set as default PDF viewer | open-with registration | done |

## Suggested order

1. Make the window feel like 2.12: main menu, Edit PDF / Annotate / Comment / Geometry grouping, theme, command palette.
2. Navigation: page layouts, rotate view, back / forward, reload from disk, search in all tabs.
3. Quick wins: new PDF from pictures, copy area as image, link tool, add bookmark, find and replace.
4. Larger features: tables, measuring tools, handwritten signature, form field creation, compare, side by side, booklet.
