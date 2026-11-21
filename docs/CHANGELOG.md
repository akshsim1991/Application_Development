# Feather PDF: what changed in each version

Free forever. Windows and macOS. Newest first.

## 2.22.1 (21 November 2026)

- Fixed (Android): on Android 15 the page went under the title bar (its first lines hidden) and the tools under the
  gesture bar. The app now keeps its title bar below the status bar and everything clear of the navigation bar, on
  every Android version; the emulator check measures it.
- New (Android): **Settings**, as in Feather PDF for Windows (menu > Settings): Appearance (theme: light, dark, AMOLED
  black, like the phone; where the tools are), PDF Viewing (page colours, open where last read, keep the screen on),
  Editing (highlight colour, drawing colour, line width), Performance (pages kept ready), Advanced (default PDF app,
  clear recent files, releases page, About, reset). Changes apply at once and are kept.
- New (Android): the tools (Read, Edit text, Highlight ...) are under the title bar (Settings can put them at the
  bottom). They share the width when they fit, else scroll with a fading edge.
- New (Android): About shows who made Feather PDF (the same text as the desktop apps), how to write to the developer,
  privacy, and the open-source licences (now inside the app).
- Android: one APK for each kind of phone (about 7 MB: `-android.apk` for nearly all phones, `-android-32bit.apk` for
  older ones), and an app bundle for Google Play; store texts, graphics, screenshots and a privacy policy
  (Android/store, docs/PRIVACY.md).
- Windows and Mac: no change except the version.

## 2.22.0 "Kuga" (17 November 2026)

- New (Windows): **Convert to PDF and combine all the selected file(s)** in File Explorer's right-click menu. Select
  pictures, text, Markdown, Word, PowerPoint and Excel files (and PDFs) together: each is made into a PDF and all of
  them become one PDF, in the order of their names (as Explorer lists them: 2 before 10), with a bookmark for each file
  (a PDF's own bookmarks inside its one), saved next to the first file as "<its name> (combined).pdf". Offered for any
  number of selected files. A file that cannot be converted is left out and named.
- New (Windows): after **Convert to PDF** (and the combining one) Feather PDF asks: open it, show it in its folder, or
  close; "Do this every time" keeps the answer. Before, the PDF always opened.
- New (Windows): **Settings > File Explorer**: what happens after converting (ask, open, show in its folder, nothing),
  and each of the two right-click entries on or off (Windows asks for permission, as they are for every user of the PC;
  an update keeps the choice).

## 2.21.0 "Puma" (16 November 2026)

- New: **Feather PDF for Android** (Android 7 or later; arm64, 32-bit ARM, x86_64). The essential reading and editing,
  with the same PDF engine as Windows and Mac: open (file picker, "Open with", "Share to", password files, recent files
  with the page last read), continuous scrolling, pinch and double-tap zoom with sharp pages, go to page, bookmarks,
  find with every match highlighted, page colours (normal, night, dimmed, blackboard); select text to copy, highlight,
  underline or strike out; edit text (paragraphs outlined, tap to change words, size, bold, italic); add text and
  notes; draw with a pen; erase annotations; turn and delete pages; undo and redo; save, save a copy, share. See
  Android/README.md.
- Engine: the cryptography and pictures can be given by the app (Android's own classes); PDFium for Android in
  scripts/fetch-pdfium.sh.
- Windows and Mac: no change except the version.

## 2.20.4 (15 November 2026)

- Fixed: after a change to a page (editing text, deleting, moving, redacting, a table, a picture), **a gradient
  background turned black and symbols such as arrows and emoji disappeared** - in PDFs printed from a browser or made by
  presentation programs. The page is now checked after it is written: anything that would not come back as it was is
  kept as a sharp picture of itself (300 dots per inch), in the same place, and the page is checked again; if it still
  would not look the same, the change is not made and you are told why.
- Fixed: **"there is no room on this page to move the text below it down"** when there was plenty of room - a footer or
  page number near the bottom of the page counted as text that had to move. Now only what the longer paragraph would
  run into moves down (and what that runs into), as in a word processor; text far below stays where it is.
- Fixed: a one-line list item that wraps onto more lines now continues under its text, not under the bullet or number.

## 2.20.3 (13 November 2026)

- Fixed (Windows): **recent files disappeared after updating**, after closing several windows, and after Windows shut
  down or restarted. Every window (each a program of its own since 2.15) wrote the recent list from the copy it read when
  it opened, so the last window to close put back its old list; and nothing was saved when Windows ended the session.
  Now the list is written the moment a file is opened, changed one window at a time, and settings, reading positions and
  recovery copies are kept when Windows shuts down. A window no longer undoes preferences changed in another window.
- Fixed (Windows): **file names in Kannada, Hindi and other scripts** were stored as "?????" in the settings, so they
  could not be opened again from the recent list ("not found"). The settings file, workspaces and recovery files are
  Unicode now; an existing settings file is converted once, everything in it kept (names already damaged cannot be
  recovered, and are left out of the list).
- Updating, uninstalling and reinstalling keep recent files and settings (checked: the settings file unchanged byte for
  byte through all three; the Mac's recent list and settings kept when the app is replaced by an update).

## 2.20.2 (13 November 2026)

- Fixed (both apps): **a paragraph that starts with a bold label** ("License and cost controls: Paid third-party ...")
  opened for editing in bold and, on the Mac, with the rest of its first line still drawn under the box. The line was
  split in two when its bold start came second; now its parts join left or right. The paragraph is edited in the style
  of most of its text, and **every word keeps its own font when it is set again**: the label stays bold, the rest
  regular; new words take the style of the text before them. A style chosen in the style bar still applies to all.
- Fixed: **"Paragraphs can use letters, numbers and common symbols of Western languages"** for text with a dash (—, –),
  curly quotes, a bullet, an ellipsis, € or ™: the standard PDF fonts have them, and they are accepted now.
- Fixed: after one edit, the other justified paragraphs of the page fell apart into pieces (a word such as "to" or
  "a" outlined on its own) and could not be edited whole.

## 2.20.1 (13 November 2026)

- Fixed: **editing text of LaTeX documents** (question papers, theses ...): a bold serif heading opened as plain
  sans-serif, its box wider than the line, so it broke onto two lines and covered the line below. Fonts such as
  "NimbusRomNo9L-Medi" (LaTeX's bold Times), "CMBX12" or "LMRoman12-Bold" are now known as serif and bold; the weight and
  slant the font itself gives count too.
- A line on its own (a heading, a short item) now grows along its line as you type, to the right edge of its column,
  before it wraps; before, it wrapped at its old length. (Both apps.)

## 2.20.0 "Aspire" (13 November 2026)

- **Edit PDF in one click**, as in other modern PDF editors (both apps). Clicking Edit PDF turns editing on at once:
  every paragraph gets a thin outline and every picture a dashed one, and the one under the pointer lights up. Click
  into any text and type: it is edited in place, with the cursor where you clicked and the font, size, bold, italic,
  colour and alignment bar above it. Click a picture to pick it, then drag it to move it or a corner to resize it.
  There is no longer a separate "edit a line" and "edit a paragraph": Feather PDF finds the block itself (a line in
  another colour is a block of its own).
- While editing, a second toolbar row (on the Mac a bar under the toolbar) holds Add text, Insert picture, Insert
  table, Find and replace, Redact an area and More editing tools (every other tool, as the Edit PDF menu had them),
  with Done on the right. Esc, Done or Edit PDF again ends editing. Ctrl+E / F2 (Mac: ⌘E) do the same.

## 2.19.2 (13 November 2026)

- Fixed: **Feather PDF did not start on Windows 7** ("The procedure entry point BCryptHash could not be located in
  the dynamic link library bcrypt.dll"). It used a Windows function that exists only from Windows 10; it now uses the
  older ones that do the same. Feather PDF runs on Windows 7 SP1, 8.1, 10 and 11 (64-bit), and the tests now fail if
  a function of a newer Windows creeps in again.
- Windows 7: the update check needs Windows 7's TLS 1.2 update (KB3140245, offered by Windows Update) to reach the
  update servers; without it, download new versions from the releases page.

## 2.19.1 (11 November 2026)

- Fixed (Windows): **swiping left or right on a touchpad did not scroll the page** sideways. It now does, as do a
  tilting mouse wheel and Shift + wheel, whenever the page is wider than the window. (The Mac already did.)

## 2.19.0 "Endeavour" (11 November 2026)

- **Convert to PDF for PowerPoint, Word and Excel files** (Windows): right-click a .ppt, .pptx, .pps, .ppsx, .doc,
  .docx, .rtf, .xls, .xlsx, .odp, .odt or .ods file. The office program on the PC makes the PDF, as its own "Save as
  PDF" would: Microsoft Office, else WPS Office, else LibreOffice; with none of them, Feather PDF says so. The PDF goes
  next to the file and opens in Feather PDF.
- Fixed: **a .jpg converted to a blank white page**. The picture was really a PNG with a .jpg name (common with
  downloaded wallpapers and AI pictures); it was put in the PDF as if it were a JPEG, which no reader could show.
  Pictures are now told apart by what is in the file, in Convert to PDF, New PDF from pictures and Insert picture.
- Fixed: **Help > Update to new version took about 20 seconds to answer** where raw.githubusercontent.com does not
  answer. The servers are now asked at the same moment and the first good answer is taken; the download of a new
  version starts with the server that answered. "Looking for a new version…" shows at once.
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
