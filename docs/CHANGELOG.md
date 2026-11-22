# Feather PDF: what changed in each version

Free forever. Windows and macOS. Newest first.

## 2.24.2 (22 November 2026)

From editing every formula of a LaTeX formula compendium (six pages of tables) one after another:

- Fixed: an edited formula that became wider ran into the next formula on its line (or a word after it). What follows
  on the line, with the tops and bottoms of its fractions, now moves along, up to the next column of the table.
- Fixed: after a formula had been edited, the next one on its line was read together with the new one.
- Better: Greek letters in equations lean as TeX sets them (they were upright); ∮ is drawn (an integral sign with a
  ring); TeX's degree sign (90°) is drawn as one.
- Fixed: a character no font of the equation can draw is refused with a message instead of printed as another letter.
- Fixed (2.24.1): the Android build of 2.24.1 did not compile (a bracket missing in the equation editor's hint).

## 2.24.1 (22 November 2026)

- Fixed: the app closed when text was clicked in edit mode on PDFs whose pages are each one form (pages put together by
  FPDI, as many university and office systems make them). Such pages now open for editing; one that cannot is left as it
  is, and is not tried again on every click.
- Fixed: on such pages, a stamp added over the page (a number at its foot) or a dash that can only be kept as a picture no
  longer stopped the whole page from being edited.
- Fixed: a bulleted list in a table's cell came back as one run-on paragraph in a bold sans font. Its items keep their
  own lines, bullets (➢ and other Wingdings bullets are set in the standard Dingbats font), indents and the cell's own
  font; a line set against the cell's right edge ("Number of Hours: 28") stays there.
- Better: equations from LaTeX (Computer Modern fonts) are read whole by the equation editor: its signs and digits, tall
  brackets, roots, sums and integrals, accents (h̄, u̅), upright words under a letter (ρ_fluid) and functions (sin, cos).
  Two equations side by side in a table's cell open one at a time.
- Fixed (2.24.0): the Mac and Android builds of 2.24.0 did not compile; 2.24.1 is the first of them with the equation
  editor.

## 2.24.0 "Maverick" (22 November 2026)

An equation editor: equations are edited as equations, not as text.

- New: the equation editor. In edit mode a click on an equation (Cambria Math from Word, LaTeX's fonts, or one made here)
  opens it written in one line: `s^8` is a power, `x_1` an index (braces for more: `e^{-st}`, `x_{n+1}`),
  `\frac{top}{bottom}` a fraction, `\sqrt{x}` and `\sqrt[3]{x}` roots, `\alpha` ... `\Omega`, `\le`, `\ne`, `\infty`,
  `\sum_{i=1}^{n}`, `\int`, `\lim_{x \to 0}`, `\text{...}`; Unicode signs and letters can be typed as they are. Buttons
  insert each of them (wrapping the selected text), and a live picture shows the equation as it will be set. Apply sets
  it on the page: powers raised and smaller, fractions over a bar as wide as their longer side, brackets as tall as what
  they hold, signs spaced, letters italic, functions upright. One undo step. Remove equation takes it off.
  An equation from Word such as G(s)H(s) = K / s(s + 2)(s + 4)(s + 6) is read whole: the left side, the fraction's top,
  bar and bottom, and its powers (s⁷ + 9s⁶ + ... reads as `s^{7} + 9s^{6} + ...`).
- New: Insert equation (edit row, Tools menu, command palette; Mac: Edit menu and edit bar; Android: the Equation button
  of Add text): written in the editor, then placed where the page is clicked.
- New: x² and x₂ buttons in the text editor's bar (Windows, Mac and Android) make the selected text superscript or
  subscript, or plain again; on Windows, with nothing selected they switch what is typed next, and Ctrl+Shift+= and
  Ctrl+= do the same. A ∑ Equation button opens the line being edited in the equation editor.

## 2.23.3 (21 November 2026)

- Fixed (since 2.23.0): two lines of one paragraph were edited apart when lines in a slightly smaller size below them
  reached further right (the column's edge was taken from them).

## 2.23.2 (21 November 2026)

- Fixed: editing the top or bottom of a fraction in an equation wrapped it onto a second line and left the fraction bar
  as it was. A line of an equation now stays on one line; the bar stretches or shrinks to the longer of its top and
  bottom, keeping its place after "=", and the other side stays centred over it.
- New: Settings > PDF Viewing > Default zoom (fit width, fit page, 50% to 200%) for documents opened without a remembered
  place, and (Windows) Default page layout (single page, continuous, two pages, two pages with the cover alone).

## 2.23.1 (21 November 2026)

- Fixed: Help > Update to new version said "you have the newest version" where raw.githubusercontent.com is blocked and
  GitHub's API has run out of requests (many people share one address), because the jsDelivr mirror's copy of the version
  file can be hours old. The app now also asks jsDelivr which commit is the newest and reads the file at that commit.

## 2.23.0 "Explorer" (21 November 2026)

Editing that holds up on PDFs from other programs, found by editing real files from six programs.

- New: scanned pages can be edited: the new words take the colour, weight and size of the old ones, over the page's own
  background. Windows offers to recognise the text of a scan first.
- New: exponents and indices (x², H₂O) stay raised or lowered and can be typed; equations from Word keep their italic
  letters; a fraction's top and bottom are edited apart.
- New: drawn shapes can be picked to move or delete; text inside stamped or merged pages can be edited where the page can
  be written back exactly.
- Fixed: edits were refused on Google Docs and journal PDFs; LaTeX paragraphs were edited a line at a time; inserting a row
  in a Word table scrambled it; find and replace broke layouts and missed words; centred headings, list numbers, italic
  words, titles and apostrophes lost their look when edited.

## 2.22.2 (21 November 2026)

Editing fixes for two PDFs a user sent: a name list made with WPS Office, and a question paper made in Word and printed
with Microsoft Print to PDF. All three apps (the engine is shared).

- Fixed: a space typed into text set in a font the PDF holds only some letters of (WPS embeds fonts without a space)
  was drawn as a box. Words are now set apart by the space's width instead.
- Fixed: in edit mode a picture could be picked but Delete (and the picture menu: turn, flip, bring to front) did nothing
  (Windows); they worked only with the Move tool.
- Fixed: a heading on a line of its own wrapped onto a second line as soon as a few letters were added. It now grows
  into the free space to its right, as far as the page's content goes and not onto anything beside it.
- Fixed: when a paragraph got longer, only what was under it in its own column moved down, so a wider table below was
  torn apart ("Course" left behind, "Name:" moved). A table now moves whole, and what is on the same line as something
  that moves (a footer's page number) moves with it.
- Fixed: text in a table was outlined and edited by lines of the page, which ran across the cells' lines ("1 Define
  turbo machine ... CO1 L2") and on into the next row. A click in a cell now edits that cell: all its text, wrapped at its
  lines, its row growing when needed.
- Fixed: the text editor showed "Serif" and bold for every font of a PDF printed with Microsoft Print to PDF (it names
  them all "CIDFont+F1" ... and describes them as serif and heavy). The font's real name (Calibri, Times New Roman
  Bold ...) is now read from the font file inside the PDF.
- Fixed: changing one cell set other rows of the table again (their text sharing a text object with another cell), which
  lost their superscripts and alignment. Cells that did not change are now left exactly as they were.
- New: justified text in table cells: read from the page (a question paper's questions), kept when a cell is changed,
  and offered in the table editor (Align: Justified). Justified lines are recognised whatever the
  font's letter shapes (Apple's Arial was read as left-aligned).
- Fixed: superscripts in a changed cell became plain letters ("1.1m3 /s", "20 0"). They now become ³, ², ¹ and ° (m³,
  20°), letters every font has.
- Fixed (WPS tables, found with the same file): a list drawn as a one-column table was not found as a table; letters of
  neighbouring words were interleaved; a whole column was taken as one paragraph; "SHAHID" was read as "SHAH I D".

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
