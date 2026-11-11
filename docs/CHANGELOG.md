# Feather PDF: what changed in each version

Free forever. Windows and macOS. Newest first.

## 2.19.0 "Endeavour" (11 November 2026)

- **Convert to PDF for PowerPoint, Word and Excel files** (Windows): right-click a .ppt, .pptx, .pps, .ppsx, .doc,
  .docx, .rtf, .xls, .xlsx, .odp, .odt or .ods file. The office program on the PC makes the PDF, as its own "Save as
  PDF" would: Microsoft Office, else WPS Office, else LibreOffice; with none of them, Feather PDF says so. The PDF goes
  next to the file and opens in Feather PDF.
- Fixed: **a .jpg converted to a blank white page**. The picture was really a PNG with a .jpg name (common with
  downloaded wallpapers and AI pictures); it was put in the PDF as if it were a JPEG, which no reader could show.
  Pictures are now told apart by what is in the file, in Convert to PDF, New PDF from pictures and Insert picture.
- Fixed: photos that a phone or camera marked as turned (EXIF orientation) came out lying on their side; they now
  stand the right way up.

## 2.18.0 "Ranger" (11 November 2026)

- **Combine PDFs**: the documents open in the tabs become one new PDF, one after another in the order of the tabs.
  Untick any to leave out; a bookmark for each document (its own bookmarks inside), or their bookmarks as they are, or
  none. Unsaved changes are included. The result opens in a new tab, not yet saved. Windows: the … menu or the command
  palette; Mac: File > Combine Open PDFs (every open document, this window's tabs first).
- **Convert to PDF** from File Explorer: right-click a picture (PNG, JPEG, BMP, GIF, TIFF) or a text or Markdown file.
  The PDF goes next to the file with the same name ("Photo.jpg" -> "Photo.pdf", or "Photo (2).pdf" when that exists)
  and opens in Feather PDF; several files chosen at once open as tabs of one window. Added by the installer and removed
  by the uninstaller. On Windows 11 it is under "Show more options". On the Mac: the Finder's Quick Actions / Services
  menu, "Convert to PDF with Feather PDF".
- Fixed: a message shown before the window opens (such as "could not be converted") had its last lines cut off.
- Fixed: **Update to new version did nothing when more than one Feather PDF window was open** (Windows). The window
  that asked closed, the others stayed open, and the installer gave up without a word. Now every window closes (each
  asks about unsaved changes), and if one is still open the installer asks to close it, with Retry; a failed install
  says why, and Feather PDF opens again either way. Updating from 2.15, 2.16 or 2.17 with several windows open: the
  2.18 installer asks to close the other windows, then installs.
- Fixed: **"Could not find out whether there is a new version" although the internet works** (reported with a
  screenshot, Windows). Feather PDF now uses the proxy set in Windows (also an automatic one), and when
  raw.githubusercontent.com cannot be reached (some providers block it at times) it tries the jsDelivr mirror and then
  GitHub's API, for the version file and for the new version itself (still checked by its SHA-256). When all fail,
  the message says what each server answered.
- Fixed (Mac): an update that could not be put in place started the old version without a word; it now says why at
  the next start. Cancelling Quit (a document not saved) no longer lets the new version replace the running app later.

## 2.17.0 "Bronco" (7 November 2026)

- **Blackboard page colours**, in both apps: the page pure black (#000000, so an OLED screen shows those pixels
  switched off) with white text. Two choices:
  - **Inverted**: everything as a negative, pictures and colours included.
  - **Original colours**: only the paper and black or grey text change; pictures stay exactly as they are, and red
    text stays red, blue stays blue. A highlight behind text becomes a darker tint of its colour, with white text on
    it. A scanned page (one picture over the whole page) turns black like any other page.

  Windows: Settings > PDF Viewing > Page colours, or the command palette (Ctrl+K, "Page colours"). Mac: View > Page
  Colours, or Settings. The space around the pages turns black too.

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
