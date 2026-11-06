# Feather PDF: what changed in each version

Free forever. Windows and macOS. Newest first.

## 2.16.0 "Mustang" (6 November 2026)

Thirteen new features, in both apps (Windows and Mac):

- **Find sensitive information and redact it**: e-mail addresses, phone numbers, Aadhaar numbers (their check digit is
  verified), PAN and bank account numbers or IFSC codes. Untick what should stay; the rest is marked, and Apply
  Redactions removes it for good. Dates, amounts, version numbers and other look-alikes are left alone.
- **Search**: whole words only, wildcards (`*` any letters, `?` one letter, `\*` a real star), and the text of comments.
  A hit in a comment frames the comment.
- **Split** at the bookmarks (each part named after its chapter), into files no larger than a size, or every N pages;
  each part keeps its own bookmarks. **Merge** keeping every file's bookmarks, or under a bookmark per file.
  **Two-sided scans**: fronts and backs put in order (backs reversed or not, from the same file or another).
- **Comment threads**: reply to a comment; mark it Accepted, Rejected, Completed or Cancelled; a review summary of every
  comment and reply. Stored as the PDF standard has it, so other readers show the same.
- **Attached files**: open, save a copy, add, remove. **Layers**: show or hide each one.
- **Undo history**: every step named (Rotate pages, Highlight, Edit a paragraph ...) with its time; go back or forward
  to any step in one click (Windows: the History tab of the side panel; Mac: Window > Undo History).
- **Form calculations and checks**: totals, products, averages, smallest and largest; fields that accept only a
  number, a range, a date, an e-mail address or a phone number. A refused answer is put back, with the reason.
  Written as Acrobat's standard form scripts, so other readers do the same.
- **Accessibility check and fixes**: 13 checks (title, language, tags, text on every page, alternate text, links,
  field tooltips, tab order, bookmarks, contrast, reading order ...) and fixes for the six that need no person.
  A report as a PDF.
- **Text compare, word by word**, across the whole documents (a line that moved to another page is not a change): a
  report of every change with its pages, and a copy of the first document with each change marked.
- **Repair** a damaged PDF (a cut-off download, a broken index), saying what was wrong. **Fast Web View**: a linearized
  copy, so the first page shows while the rest is still downloading (checked with qpdf).
- **Pen and tablet**: strokes as thin or thick as the pen was pressed; palm rejection (a hand on the screen does not
  draw while the pen is in use).
- **Read aloud** with the spoken word highlighted on the page, which follows the reading; back or forward a sentence,
  pause, slower or faster, stop.
- **Saved workspaces**: save the open tabs under a name and open them again later, at the same pages and zoom.

Also:

- About: the development history (Visual Basic .NET, then C#, then C++), development and AI contributions, credits and
  acknowledgements, and the address for feedback and suggestions (akshsimha@gmail.com).
- The command palette lists the exact name and names starting with what was typed first.

## 2.15.0 "Falcon" (5 November 2026)

- Separate windows in both apps: open a tab in a new window (right-click the tab, or drag it off the tab row), open a
  new window (Ctrl+N; on the Mac Shift+Command+N), or have every file open in its own window (Settings > Tabs).
- Each Windows window keeps its own recovery copies; a crash of any window is offered back.
- The version name in About and the status bar.

## 2.14.7 (5 November 2026)

- About names Vinod Kumar C S, Athith D and Mamatha H R under Courtesy (both apps).

## 2.14.6 (5 November 2026)

- Two-sided printing: flip on the long edge or the short edge (landscape pages turn the right way).
- Right-click a tab: open the folder with its file, copy the path, close it or the others (both apps).

## 2.14.5 (31 October 2026)

- Editing lines and paragraphs: list items, hyphenated words, justified paragraphs; the Mac editor rebuilt (the
  document's own font, the text in place).

## 2.14.4 (31 October 2026)

- Side by side in one window; form notes keep their place; fonts from collections and from Office kept on the Mac.

## 2.14.3 (30 October 2026)

- A crash after text recognition (OCR) fixed; grouped toolbar buttons follow the colourful icon setting; OCR languages
  by name.

## 2.14.2 (26 October 2026)

- The first Mac version, with the same engine as Windows; About with the version and build date.
- Automatic updates on Windows and macOS (Help > Update to new version).
- Tables inside form objects edited in place; the Mac embeds installed fonts for edited text.

## 2.13.0 (24 October 2026)

- The rebuilt Windows app in C++ on PDFium: tabs, search, bookmarks and thumbnails, annotations, forms, signatures,
  redaction, OCR, compression, conversion to Word and Excel, page tools, and an installer.
