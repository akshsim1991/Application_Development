#!/usr/bin/env python3
"""Builds docs/Feather PDF User Guide.pdf from the text below and the pictures in docs/guide/.

Needs reportlab, Pillow and the Liberation fonts (fonts-liberation). Run from anywhere: python3 scripts/make_user_guide.py
"""
import os
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, Frame, Image, KeepTogether, NextPageTemplate, PageBreak, PageTemplate,
                                Paragraph, Spacer, Table, TableStyle)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PICS = os.path.join(ROOT, "docs", "guide")
OUT = os.path.join(ROOT, "docs", "Feather PDF User Guide.pdf")
VERSION = "2.14.2"

FONT_DIR = "/usr/share/fonts/truetype/liberation"   # fonts-liberation (Arial / Courier New metrics)
pdfmetrics.registerFont(TTFont("Sans", os.path.join(FONT_DIR, "LiberationSans-Regular.ttf")))
pdfmetrics.registerFont(TTFont("Sans-Bold", os.path.join(FONT_DIR, "LiberationSans-Bold.ttf")))
pdfmetrics.registerFont(TTFont("Sans-Oblique", os.path.join(FONT_DIR, "LiberationSans-Italic.ttf")))
pdfmetrics.registerFont(TTFont("Mono", os.path.join(FONT_DIR, "LiberationMono-Regular.ttf")))
pdfmetrics.registerFontFamily("Sans", normal="Sans", bold="Sans-Bold", italic="Sans-Oblique", boldItalic="Sans-Bold")

NAVY = colors.HexColor("#14246B")
BLUE = colors.HexColor("#2D7FF9")
INK = colors.HexColor("#1F2937")
GREY = colors.HexColor("#6B7280")
LIGHT = colors.HexColor("#EEF4FF")
RULE = colors.HexColor("#D1D9E6")

S = {
    "body": ParagraphStyle("body", fontName="Sans", fontSize=9.6, leading=14.2, textColor=INK, spaceAfter=5),
    "small": ParagraphStyle("small", fontName="Sans", fontSize=8.4, leading=11.5, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Sans", fontSize=8.6, leading=11.6, textColor=INK),
    "cellb": ParagraphStyle("cellb", fontName="Sans-Bold", fontSize=8.6, leading=11.6, textColor=INK),
    "head": ParagraphStyle("head", fontName="Sans-Bold", fontSize=8.6, leading=11.6, textColor=colors.white),
    "h1": ParagraphStyle("h1", fontName="Sans-Bold", fontSize=19, leading=24, textColor=NAVY, spaceBefore=4, spaceAfter=10),
    "h2": ParagraphStyle("h2", fontName="Sans-Bold", fontSize=12.5, leading=17, textColor=NAVY, spaceBefore=10, spaceAfter=5, keepWithNext=1),
    "bullet": ParagraphStyle("bullet", fontName="Sans", fontSize=9.6, leading=14, textColor=INK, leftIndent=12, bulletIndent=2, spaceAfter=2),
    "caption": ParagraphStyle("caption", fontName="Sans-Oblique", fontSize=8.2, leading=11, textColor=GREY, alignment=TA_CENTER, spaceBefore=3, spaceAfter=10),
    "mono": ParagraphStyle("mono", fontName="Mono", fontSize=8.0, leading=11, textColor=INK),
    "toc1": ParagraphStyle("toc1", fontName="Sans-Bold", fontSize=10.5, leading=17, textColor=INK, leftIndent=0),
    "toc2": ParagraphStyle("toc2", fontName="Sans", fontSize=9.4, leading=14, textColor=INK, leftIndent=16),
    "tip": ParagraphStyle("tip", fontName="Sans", fontSize=9.0, leading=13, textColor=INK),
}


class Doc(BaseDocTemplate):
    """Pages with a header and footer; headings go into the table of contents."""

    def __init__(self, path):
        super().__init__(path, pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=22 * mm, bottomMargin=20 * mm,
                         title=f"Feather PDF {VERSION} - User Guide", author="Feather PDF", subject="User guide and feature list",
                         keywords="Feather PDF, PDF reader, PDF editor, user guide")
        w, h = A4
        body = Frame(self.leftMargin, self.bottomMargin, w - self.leftMargin - self.rightMargin, h - self.topMargin - self.bottomMargin, id="body")
        self.addPageTemplates([PageTemplate(id="cover", frames=[body], onPage=self.cover),
                               PageTemplate(id="page", frames=[body], onPage=self.page)])

    def cover(self, c, doc):
        w, h = A4
        c.saveState()
        c.setFillColor(NAVY)
        c.rect(0, h - 105 * mm, w, 105 * mm, stroke=0, fill=1)
        c.setFillColor(BLUE)
        c.rect(0, h - 108 * mm, w, 3 * mm, stroke=0, fill=1)
        c.setFillColor(colors.white)
        c.setFont("Sans-Bold", 34)
        c.drawString(20 * mm, h - 48 * mm, "Feather PDF")
        c.setFont("Sans", 15)
        c.drawString(20 * mm, h - 60 * mm, f"Version {VERSION}  •  User Guide")
        c.setFont("Sans", 10.5)
        c.drawString(20 * mm, h - 74 * mm, "A free, lightweight PDF reader and editor for Windows.")
        c.drawString(20 * mm, h - 81 * mm, "Free forever: no subscriptions, no trials, no licence keys.")
        c.setFillColor(GREY)
        c.setFont("Sans", 8.5)
        c.drawString(20 * mm, 14 * mm, f"Feather PDF {VERSION}  •  You may share this guide and the program with anyone.")
        c.restoreState()

    def page(self, c, doc):
        w, h = A4
        c.saveState()
        c.setStrokeColor(RULE)
        c.setLineWidth(0.6)
        c.line(20 * mm, h - 14 * mm, w - 20 * mm, h - 14 * mm)
        c.setFillColor(NAVY)
        c.setFont("Sans-Bold", 8.5)
        c.drawString(20 * mm, h - 12 * mm, "Feather PDF")
        c.setFillColor(GREY)
        c.setFont("Sans", 8.5)
        c.drawString(20 * mm + c.stringWidth("Feather PDF ", "Sans-Bold", 8.5), h - 12 * mm, f"{VERSION}  •  User Guide")
        c.line(20 * mm, 13 * mm, w - 20 * mm, 13 * mm)
        c.drawRightString(w - 20 * mm, 8.5 * mm, f"Page {doc.page}")
        c.restoreState()

    def afterFlowable(self, f):
        if isinstance(f, Paragraph) and f.style.name in ("h1", "h2"):
            level = 0 if f.style.name == "h1" else 1
            text = f.getPlainText()
            key = "h%d" % id(f)
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(text, key, level=level, closed=level > 0)
            self.notify("TOCEntry", (level, text, self.page, key))


def P(t, s="body"):
    return Paragraph(t, S[s])


def bullets(items):
    return [Paragraph(i, S["bullet"], bulletText="•") for i in items]


def picture(name, width_mm, caption=None):
    path = os.path.join(PICS, name)
    from PIL import Image as PI
    with PI.open(path) as im:
        w, h = im.size
    width = width_mm * mm
    img = Image(path, width=width, height=width * h / w)
    framed = Table([[img]], colWidths=[width + 2])
    framed.setStyle(TableStyle([("BOX", (0, 0), (-1, -1), 0.6, RULE), ("LEFTPADDING", (0, 0), (-1, -1), 1), ("RIGHTPADDING", (0, 0), (-1, -1), 1),
                                ("TOPPADDING", (0, 0), (-1, -1), 1), ("BOTTOMPADDING", (0, 0), (-1, -1), 1)]))
    out = [framed]
    if caption:
        out.append(P(caption, "caption"))
    return KeepTogether(out)


def table(rows, widths, header=True, zebra=True, pad=3.5):
    data = []
    for r, row in enumerate(rows):
        cells = []
        for c in row:
            if isinstance(c, str):
                cells.append(Paragraph(c, S["head"] if header and r == 0 else S["cell"]))
            else:
                cells.append(c)
        data.append(cells)
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if header else 0)
    st = [("VALIGN", (0, 0), (-1, -1), "MIDDLE"), ("GRID", (0, 0), (-1, -1), 0.4, RULE),
          ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5), ("TOPPADDING", (0, 0), (-1, -1), pad), ("BOTTOMPADDING", (0, 0), (-1, -1), pad)]
    if header:
        st.append(("BACKGROUND", (0, 0), (-1, 0), NAVY))
    if zebra:
        for r in range(1 if header else 0, len(data)):
            if r % 2 == 0:
                st.append(("BACKGROUND", (0, r), (-1, r), colors.HexColor("#F6F8FC")))
    t.setStyle(TableStyle(st))
    return t


def tip(text):
    t = Table([[Paragraph("<b>Tip</b>  " + text, S["tip"])]], colWidths=[170 * mm])
    t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), LIGHT), ("LINEBEFORE", (0, 0), (0, -1), 3, BLUE),
                           ("LEFTPADDING", (0, 0), (-1, -1), 8), ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    return KeepTogether([Spacer(1, 3), t, Spacer(1, 6)])


def icon(name):
    return Image(os.path.join(PICS, "icon_%s.png" % name), width=7.5 * mm, height=7.5 * mm)


story = []

# ---------- cover ----------
story += [Spacer(1, 95 * mm), picture("cover.png", 170)]
story += [Spacer(1, 6), P("This guide shows how to work with Feather PDF: the window, the toolbar icons and menus, and every "
                          "feature of version %s, with step-by-step instructions and keyboard shortcuts." % VERSION, "small")]
story += [NextPageTemplate("page"), PageBreak()]

# ---------- contents ----------
toc = TableOfContents()
toc.levelStyles = [S["toc1"], S["toc2"]]
story += [P("Contents", "h1"), toc, PageBreak()]

# ---------- 1 welcome ----------
story += [P("1. Welcome", "h1")]
story += [P("Feather PDF is a fast, compact PDF reader and editor for Windows. It opens large documents instantly, lets you "
            "read, annotate, edit, sign, fill in, print and convert PDF files, and keeps working entirely on your own computer: "
            "nothing is uploaded anywhere.")]
story += [P("It is <b>free forever</b>. There are no subscriptions, trials, licence keys, advertisements or nag screens, and you "
            "may give the program to anyone.")]
story += [P("At a glance", "h2")]
story += [table([["", ""],
                 ["Runs on", "Windows 10 and Windows 11 (64-bit)"],
                 ["Size", "A 12 MB setup program; about 25 MB installed, including the text recognition (OCR) engine"],
                 ["Installs", "In C:\\Program Files\\Feather PDF (or a folder you choose), for every user of the PC; Windows asks for administrator rights once. Run the setup again to update or repair; uninstall from Installed apps (your settings stay)"],
                 ["Opens", "PDF files (also password-protected ones), plain text and Markdown files, and pictures as new PDFs"],
                 ["Saves", "PDF, PDF/A-2b, PNG and JPEG pictures, plain text, Markdown, Word (.docx), Excel (.xlsx / .csv)"],
                 ["Your work is safe", "Unsaved changes are kept in the background and offered again after a crash"]],
                [38, 132], header=False)]
story += [P("What's new in %s" % VERSION, "h2")]
story += [P("Version %s is a quality release: every reported problem was traced to its cause, fixed and tested, and a "
            "search for hidden defects found and fixed more. The full list is in the bug log that comes with the source." % VERSION)]
story += bullets([
    "<b>Editing keeps the page as it was.</b> Edit paragraph and Edit text break lines exactly where the page will, in the document's own font; "
    "applied text keeps its font, size, colour, baselines and justification. New letters a document's font lacks are set in the same installed font. "
    "A paragraph that grows moves the text below it down instead of covering it.",
    "<b>Other text no longer shifts</b> when you edit a page: justified and letter-spaced lines elsewhere keep their exact places.",
    "<b>Zoom follows the mouse pointer:</b> what is under the pointer stays there while you zoom with Ctrl and the wheel.",
    "<b>One Print window</b> with a live preview of every sheet; the preview follows the page range as you type.",
    "<b>Password protection</b> is reliable: before, about one protected file in ten could not be opened.",
    "<b>PDF/A</b> keeps spacing and kerning when it replaces fonts, and no longer turns pages into pictures needlessly.",
    "<b>The … menu</b> opens without running a command by accident and no longer repeats the toolbar.",
    "<b>A cleaner window:</b> the tabs sit in the title bar, the toolbar is grouped and highlights the tool in use, the page "
    "number and zoom can be typed in, a start page offers Open PDF and your recent files, and a status bar shows the page, its "
    "size and whether there are unsaved changes. Everything scales with the display's text size; the dark theme has more contrast.",
])
story += [PageBreak()]

# ---------- 2 the window ----------
story += [P("2. The window at a glance", "h1")]
story += [picture("overview.png", 170, "Feather PDF with a document open and the Pages panel showing.")]
story += [table([["", "Part", "What it is for"],
                 ["1", "Tabs", "The top row is also the window's title bar. Every open document has a tab: click it to switch, click its red "
                               "\u00d7 to close it, click + to open another file. Drag the empty part of the row to move the window; "
                               "double-click it to maximise."],
                 ["2", "Window buttons", "Minimise, maximise (or restore) and close Feather PDF."],
                 ["3", "File buttons", "Open and Save, then Undo and Redo. Save is available when the document has unsaved changes."],
                 ["4", "Tool menus", "Edit PDF, Annotate, Comment, Geometry and Side by side. Each opens a menu of related tools. The menu "
                                     "whose tool is in use is highlighted. On a narrow window only their icons show."],
                 ["5", "Page navigation", "Previous page, the page box (type a page number and press Enter), the number of pages, next page."],
                 ["6", "Zoom", "Zoom out, the zoom box (type a percentage and press Enter; its arrow lists preset levels and the fit "
                               "options) and zoom in."],
                 ["7", "Find and more", "Search the document, and the \u2026 menu with everything else: files, printing, view, settings."],
                 ["8", "Side panel", "Bookmarks and page thumbnails (Ctrl+B / Ctrl+Shift+B). Drag thumbnails to move pages."],
                 ["9", "Page area", "The document. Select text by dragging; right-click for the context menu. The scroll bar is at its right."],
                 ["10", "Status bar", "The page you are on and its size, the tool in use, and whether there are unsaved changes."]],
                [8, 32, 130])]
story += [P("When no document is open, the start page shows an <b>Open PDF</b> button in an area you can also drop files on, "
            "and the files you opened recently: click one to open it, right-click it to remove it from the list.")]
story += [picture("start.png", 120, "The start page.")]
story += [PageBreak()]

# ---------- 3 toolbar icons ----------
story += [P("3. Toolbar icons", "h1")]
story += [P("Every toolbar button shows what it does when you point at it. Buttons that do not apply at the moment are greyed out "
            "(for example Save when nothing has changed). Buttons with a name open a menu; the others act at once.")]
icons = [
    ("open", "Open", "Opens a PDF, text or Markdown file. Recent files are in \u2026 > Open recent.", "Ctrl+O"),
    ("save", "Save", "Saves the document. Saving is safe: Feather writes a temporary copy first, then replaces the file.", "Ctrl+S"),
    ("undo", "Undo", "Takes back the last change. Up to 30 steps, for every kind of change.", "Ctrl+Z"),
    ("redo", "Redo", "Repeats a change you took back.", "Ctrl+Y"),
    ("edit", "Edit PDF", "Editing tools: edit text and paragraphs, move and resize objects, pictures, tables, find and replace, OCR, "
                         "watermark, page numbers, forms, redaction, password, compression, signatures, pages, export, PDF/A.", ""),
    ("annotate", "Annotate", "Highlight, underline, strikethrough, text boxes, shapes, pen, polygon, cloud, callout, stamps, "
                             "signatures, pictures and links; colours and line widths.", "Ctrl+H"),
    ("comment", "Comment", "Add a sticky-note comment, comment on selected text, notes summary, delete a comment, flatten.", "Ctrl+M"),
    ("geometry", "Geometry", "Ruler and protractor, rulers along the page edges, units and drawing scale.", ""),
    ("sbs", "Side by side", "Show two documents next to each other or one above the other, compare documents.", ""),
    ("prev", "Previous page", "Goes to the previous page.", "PgUp"),
    ("next", "Next page", "Goes to the next page.", "PgDn"),
    ("zoomout", "Zoom out", "Makes the pages smaller.", "Ctrl+−"),
    ("zoomin", "Zoom in", "Makes the pages larger.", "Ctrl++"),
    ("search", "Search", "Opens the search bar; every match is highlighted. F3 and Shift+F3 step through them.", "Ctrl+F"),
    ("more", "More (…)", "Files, printing, view options, Settings, the user guide and About.", ""),
]
rows = [["Icon", "Name", "What it does", "Key"]]
for key, name, what, sc in icons:
    rows.append([icon(key), name, what, sc])
t = table(rows, [14, 26, 108, 22], pad=1.8)
story += [t]
pal = os.path.join(PICS, "palette.png")
from PIL import Image as _PI
with _PI.open(pal) as _im:
    _pw, _ph = _im.size
pal_img = Image(pal, width=62 * mm, height=62 * mm * _ph / _pw)
box = Table([[Paragraph("<b>Tip: the command palette.</b> Press <b>Ctrl+K</b> and type a few letters of any command, for example "
                        "\u201cpicture\u201d or \u201cprint\u201d. Pick it with the arrow keys and press Enter. Every feature of Feather PDF "
                        "can be reached this way, even when you do not know which menu it is in.", S["tip"]), pal_img]],
            colWidths=[104 * mm, 66 * mm])
box.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), LIGHT), ("LINEBEFORE", (0, 0), (0, -1), 3, BLUE), ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
                         ("LEFTPADDING", (0, 0), (-1, -1), 8), ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
story += [Spacer(1, 6), box]
story += [PageBreak()]

# ---------- 4 working with ----------
story += [P("4. Working with Feather PDF", "h1")]

story += [P("Opening and reading", "h2")]
story += bullets([
    "Open files with <b>Ctrl+O</b>, by dragging them onto the window, or from Explorer (“Open with” Feather PDF). A second file opens in a new tab of the same window.",
    "Move through the document with the scroll bar, the mouse wheel, PgUp / PgDn, the arrow keys, Home / End, or by typing a page number in the page box.",
    "Zoom with the toolbar, Ctrl+wheel or Ctrl+plus / minus, or type a percentage in the zoom box. Ctrl+2 fits the width, Ctrl+0 fits the page, Ctrl+1 shows the actual size.",
    "Choose single page, continuous or two-page layout in … > Page layout. Full screen is F11, presentation mode is F5: in a presentation a click, the wheel, Space, Enter, Page Down or the arrow keys move a whole page at a time, and Esc ends it on the page you reached.",
    "Themes, page colours, tab numbers and the other preferences are in <b>Settings</b> (… > Settings, or <b>Ctrl+,</b>); see the Settings section.",
    "Back and forward (Alt+Left / Alt+Right) return to where you were after following a link or jumping to a page.",
    "Feather remembers the page you were reading in every file and opens it there next time (Settings > PDF Viewing can turn this off).",
    "Opening a PDF that is already open (by Open, a double-click in Explorer, dragging it in, or Open recent, even through another path to the same file) says which tab has it and offers to go there; that tab is never reloaded, so its unsaved changes stay.",
    "Drag a tab along the tab row to put it in another place.",
    "Read aloud (Ctrl+Shift+U) reads the selected text, or the page, with the voices installed in Windows.",
])

story += [P("Highlighting and comments", "h2")]
story += [P("Select text by dragging over it, then press <b>Ctrl+H</b> to highlight, <b>Ctrl+U</b> to underline or "
            "<b>Ctrl+Shift+K</b> to strike it through. The same choices are on the right-click menu and the Annotate menu. "
            "To add a note, choose Comment > Add comment (Ctrl+M) and click where the note belongs.")]
story += [picture("highlight.png", 150, "Highlighted text. Right-click a highlight or comment to edit or delete it.")]
story += bullets([
    "<b>Shapes:</b> Annotate > Rectangle, Ellipse, Line, Arrow, Pen, Polygon (click the corners, double-click to finish), Cloud (drag a box) and Callout (drag from the point to where the text goes; double-click a callout to change its text).",
    "<b>Stamps:</b> Annotate > Stamp offers Approved, Draft, Confidential and others, stamps with the date and time, and custom text. Click on the page where the stamp should go.",
    "<b>Colours and line widths</b> for new annotations are at the bottom of the Annotate menu.",
    "<b>Notes summary</b> (Comment menu) collects every highlight and comment into one Markdown file.",
    "<b>Flatten</b> (Comment menu) makes comments and form fields a fixed part of the page. Links keep working. Undo takes it back until you save.",
])

story += [P("Editing text in place", "h2")]
story += [P("Choose <b>Edit PDF > Edit text</b> (Ctrl+E) for a single line, or <b>Edit paragraph</b> (F2) for a whole paragraph, "
            "then click the text. An edit box opens exactly over it, on the same baseline, in the document's own font when it is installed "
            "(else a matching one), at the same size and colour, with the cursor where you clicked. Lines in the box break where they will "
            "break on the page. Type your changes directly.")]
story += [picture("para.png", 160, "Editing a paragraph in place, with the style bar above it.")]
story += bullets([
    "For paragraphs, the <b>style bar</b> above the box changes the font, size, bold, italic, colour and alignment. The box shows the result at once.",
    "<b>Enter</b> applies a line; <b>Ctrl+Enter</b> or <b>Apply</b> applies a paragraph. <b>Esc</b> cancels.",
    "Clicking anywhere else applies the change, so you can click straight on the next line to keep editing.",
    "The paragraph is set again in the same width and on the same baselines, in its own font, size and colour; a justified paragraph stays justified. "
    "Letters the document's font does not contain are set in the same font installed on your PC (or the closest standard font).",
    "When a paragraph gets longer, the text below it moves down. If the page has no room for it, Feather PDF says so and leaves the page unchanged.",
    "Find and replace (Ctrl+Shift+H) changes text across the whole document.",
])

story += [P("Pictures", "h2")]
story += bullets([
    "Insert a picture with Edit PDF > Insert picture, then click where its top-left corner should go. Ctrl+V pastes a picture from the clipboard.",
    "Choose <b>Edit PDF > Move or resize pictures, paragraphs and tables</b>, then click a picture to select it. Drag it to move it, drag a corner to resize it; guide lines help you line it up. Delete removes it.",
    "<b>Right-click</b> the selected picture (or use Edit PDF > Picture) to turn it, flip it, crop it (drag a box), adjust brightness and contrast, make it black and white, replace it, save it as PNG, or bring it to the front / send it to the back.",
])

story += [P("Settings", "h2")]
story += [P("Open Settings from … > Settings or with <b>Ctrl+,</b>. Each change applies at once and is kept; there is nothing to confirm. "
            "Ctrl+Tab moves to the next group.")]
story += [table([["Group", "What is there"],
                 ["Appearance", "Theme: Light, Modern Light, Dark, AMOLED Dark (true black), Midnight Blue, Graphite, or Like Windows (follows Windows' light or dark mode). "
                                "The theme colours the window, menus, dialogs, message boxes and scroll bars and changes without a restart. "
                                "Colourful toolbar icons (on: each icon in its own colour). Random tab colours (on: each new tab gets a colour of its own and keeps it until it is closed)."],
                 ["Tabs", "Show tab numbers (\u201c1. Report.pdf\u201d; the numbers follow when tabs are opened, closed or dragged). "
                          "What happens when a PDF that is already open is opened again: ask first, or go to its tab."],
                 ["PDF Viewing", "Page colours (normal, dark for night reading, dimmed), opening documents where they were last read, reading speed of Read aloud."],
                 ["Editing", "Highlight colour, drawing colour and line width."],
                 ["Performance", "How many drawn pages are kept ready in memory, and how often unsaved changes are copied for recovery after a crash."],
                 ["Advanced", "Set as the default PDF application (opens Windows' Default apps), clear the recent files, open the settings folder, reset all settings."]],
                [30, 140])]

story += [P("Tables", "h2")]
story += bullets([
    "Choose <b>Edit PDF > Edit a table</b>. Every table Feather PDF finds on the page is outlined with a dashed blue box: tables with lines, tables drawn with horizontal rules only, tables of lined-up text without any lines, and tables on scanned pages (recognise their text first with Edit PDF > Recognise text).",
    "Click an outlined table to open it, or right-click it and choose <b>Edit this table</b>. If a table is not outlined, drag a box around it.",
    "In the Edit table window, double-click a cell (or just start typing) to change its text: Enter finishes, Ctrl+Enter starts a new line, Tab moves to the next cell. Drag across cells to select several.",
    "The buttons add and delete rows and columns, merge and split cells, and set the alignment, bold, size, cell colour and the lines (all, outside only, rows only, none). Drag a line of the grid to change a column's width or a row's height; the fields at the bottom move and resize the whole table.",
    "<b>Apply</b> writes the table back into the page: the old text and lines are removed, not covered, and what is below the table moves down when it gets longer. Undo (Ctrl+Z) brings the old table back.",
    "To delete a table with its lines, choose <b>Edit PDF > Delete table</b> (the tables turn red) and click one, or use Delete table in the Edit table window.",
])

story += [P("Delete header and footer", "h2")]
story += bullets([
    "Choose <b>Edit PDF > Delete header and footer</b> (Add headers and footers is just above it). Feather PDF looks at the top and bottom of every page and lists what repeats there: running titles, dates, page numbers (recognised because they follow the pages), lines and logos, also when odd and even pages differ. Text that runs on into the page's body is never taken, and with a single page nothing can be seen to repeat, so nothing is listed.",
    "Everything that will go is outlined in red on the page; what stays has grey dashes. Untick a line in the list, or click the item on the page, to keep it. Before / After shows the page as it will be; the arrows (or Page Up / Page Down) go through the pages.",
    "<b>What to delete</b>: header only, footer only, both, page numbers only, or the items ticked one by one. <b>Pages</b>: all pages (the default), the current page, odd or even pages, or a list such as 2,5,8, a range such as 3-15, or both (1-3, 7, 9-12).",
    "<b>Draw an area</b> to choose by hand: drag a rectangle on the page, then drag its edges or corners to adjust it. It applies to every chosen page (an area near the foot stays at the same distance from the foot). If it also takes in the page's own content, or cuts through something, a warning says so; things across its edge are kept.",
    "<b>Delete</b> asks first, then removes the items from the pages: they are not covered, so their text can no longer be found or copied. Bookmarks, links and comments stay. Undo (Ctrl+Z) brings everything back.",
    "Scanned pages: recognise their text first (Edit PDF > Recognise text); the header and footer are then whitened in the picture and their recognised text is removed. Lines across a scan are found without it.",
    "A header that is part of a page template (drawn together with the page's own content) is listed but kept, so nothing else is damaged.",
])

story += [P("Pages", "h2")]
story += [P("Most page tools are in <b>Edit PDF > Edit pages</b> and on the right-click menu of the page thumbnails.")]
story += bullets([
    "Delete, insert blank pages, insert pages from another file, merge PDFs, rotate, move, crop (also by drawing a box), extract and split.",
    "Reverse the page order, remove blank pages, and find duplicate pages.",
    "Headers and footers: text on the left, centre and right, at the top and bottom, with fields for the page number, page count, date, file name and title.",
    "Background colour for the whole page, watermark and page numbers.",
    "In the thumbnails panel, select several pages with Ctrl or Shift, drag them to a new place, or use Ctrl+C / Ctrl+X / Ctrl+V to copy and move pages.",
])

story += [P("Forms", "h2")]
story += bullets([
    "Fill in text fields, check boxes, radio buttons, list boxes and drop-down lists by clicking them. The answers are saved into the file.",
    "Create fields with <b>Edit PDF > Fillable form</b>: text, check box, radio buttons, list box, drop-down and date fields. Drag a box where the field should go.",
    "<b>Find fields</b> turns the blanks of a flat, paper-style form into fillable fields automatically.",
    "Fill many copies from a CSV sheet (one filled-in PDF per row), export all answers to CSV or Excel, reset a form, or flatten it when it is complete.",
])

story += [P("Measuring", "h2")]
story += [P("The <b>Geometry</b> menu has a ruler and a protractor. Choose units (mm, cm, inches or points) and a drawing scale "
            "(for example 1:50) there; measurements then show real-world sizes.")]
story += bullets([
    "<b>Ruler:</b> drag from one point to another. The distance is shown on the line.",
    "<b>Protractor:</b> click the corner of the angle, then a point on the first side, then a point on the second side. "
    "The lines, an arc and the angle follow the mouse, with a hint next to the cursor at each step. Esc cancels.",
    "<b>Show rulers on the page</b> adds rulers along the page edges. <b>Keep the measurements on the page</b> turns them into annotations; <b>Clear</b> removes them.",
])
story += [picture("prot.png", 150, "Measuring an angle: the angle updates as the mouse moves.")]

story += [P("Side by side and comparing", "h2")]
story += [P("Use the <b>Side by side</b> button to show another open tab, or another file, next to the current document. "
            "Click either side to work in it: the side you clicked becomes the active one (a blue line marks it), and it has every "
            "tool, the right-click menu, the page box and the zoom.")]
story += [picture("sbs.png", 150, "Two documents side by side; the blue line marks the active side.")]
story += bullets([
    "<b>Link scrolling</b> (on or off) makes both documents scroll and zoom together, or lets each move on its own.",
    "<b>Left and right</b> or <b>Top and bottom</b> arranges the two sides; <b>Swap the two sides</b> exchanges them.",
    "Closing either document returns to the normal one-document view. <b>Compare with another open tab</b> lists the differences in the text and can overlay pages to show visual changes.",
])

story += [P("Printing", "h2")]
story += [P("Press <b>Ctrl+P</b>. One window holds everything: the settings on the left and, on the right, every sheet exactly as "
            "it will print. All pages are shown at first; type pages such as <b>1-3, 5, 8-</b> and the preview follows as you type. "
            "Choose the printer, copies, how many pages per sheet (1, 2, 4, 6 or 9), fit or actual size, orientation, colour or "
            "black and white, and one-sided or two-sided printing; the preview changes at once. <b>Printer settings</b> opens the "
            "printer's own options, such as the paper tray. Scroll through the sheets with the scroll bar, the mouse wheel or "
            "Page Up / Page Down; each shows which pages it carries. While a long document prints, a small window counts the "
            "sheets and <b>Cancel</b> stops the job.")]
story += [picture("print.png", 150, "The Print window: settings on the left, every sheet as it will print on the right.")]

story += [P("Security, signatures and redaction", "h2")]
story += bullets([
    "<b>Password protection</b> (AES-256) with optional limits on printing, copying and changing; open password-protected files by typing the password.",
    "<b>Digital signatures:</b> sign with a certificate from Windows or a .pfx file, optionally with a visible stamp and a time stamp; make your own certificate; check the signatures of any document. Feather warns when a signed file has been changed since it was signed.",
    "<b>Handwritten signatures:</b> type, draw or use a picture of your signature, then click where it goes.",
    "<b>Redaction:</b> mark areas, selected text or every search result, then apply. The text underneath is removed, and the redaction check confirms it is really gone.",
])

story += [P("Privacy and archiving", "h2")]
story += bullets([
    "<b>Remove hidden data</b> (Edit PDF menu) removes metadata, attachments, scripts, hidden comments and other hidden content, and shows a report of everything it removed.",
    "<b>Save a PDF/A copy</b> (Edit PDF menu) writes a PDF/A-2b file for long-term archiving: fonts embedded, colours defined, no scripts or passwords. It works for any open document, including text, Markdown and picture files converted to PDF.",
    "<b>Fonts used in this PDF</b> (Edit PDF menu) lists every font with its type, whether it is embedded, and the pages it appears on. It also shows which pages have no text (scans) and which already have recognised text.",
])
story += [picture("fonts.png", 140, "The fonts report. Copy puts the table on the clipboard.")]

story += [P("Text recognition (OCR)", "h2")]
story += [P("<b>Edit PDF > Recognise text (OCR)</b> finds the words on scanned pages and adds them invisibly, so the pages can be searched, "
            "selected and copied while looking exactly the same. Choose the language and the pages; pages that already have text are skipped. "
            "Tick <b>First show the fonts used in this PDF</b> to see which pages are scans before you start.")]

story += [P("Converting and exporting", "h2")]
story += bullets([
    "<b>Export</b> (Edit PDF menu) saves pages as PNG or JPEG pictures, or the text as plain text, Markdown, Word (.docx) or Excel (.xlsx / .csv, tables only).",
    "<b>New PDF from pictures</b> and <b>New PDF from a text or Markdown file</b> are in the … menu. Opening a .txt or .md file converts it too.",
    "<b>Compress</b> makes files smaller by recompressing large pictures. <b>Black and white</b> converts pictures to greyscale.",
    "<b>Batch</b> runs OCR, watermark, compression, password or signing on a whole folder, with saved presets.",
    "<b>Search all my PDFs in a folder</b> finds a phrase in every PDF of a folder and keeps an index for fast repeat searches.",
])
story += [PageBreak()]

# ---------- 5 feature list ----------
story += [P("5. Complete feature list", "h1")]
features = [
    ("Reading", ["Tabs and one window for every launch", "Single page, continuous and two-page layouts", "Zoom, fit width, fit page, actual size",
                 "Go to page, back and forward", "Bookmarks panel and page thumbnails", "Search with every match highlighted",
                 "Search all PDFs in a folder", "Text selection and copy", "Clickable links", "Seven themes, including AMOLED Dark, Midnight Blue and Graphite",
                 "Dark and dimmed page colours", "Full screen and presentation mode", "Read aloud", "Remembered reading position",
                 "Password-protected files", "Command palette (Ctrl+K)"]),
    ("Annotating", ["Highlight, underline, strikethrough, squiggly", "Comments on text and sticky notes", "Rectangle, ellipse, line, arrow, pen",
                    "Polygon, cloud and callout", "Stamps, date stamps and custom stamps", "Handwritten signatures", "Links",
                    "Colours and line widths", "Notes summary as Markdown", "Flatten comments (links kept)"]),
    ("Editing", ["Edit text and paragraphs in place", "Font, size, bold, italic, colour, alignment", "Text boxes", "Find and replace",
                 "Tables", "Move and resize objects with guides", "Insert, paste and replace pictures",
                 "Turn, flip, crop pictures", "Brightness, contrast, black and white", "Save pictures as PNG", "Bring to front, send to back",
                 "Undo and redo (30 steps)"]),
    ("Pages", ["Delete, insert, rotate, move", "Crop (also by drawing a box)", "Insert from file, merge", "Extract and split",
               "Reverse the order", "Remove blank pages", "Find duplicate pages", "Headers and footers", "Background colour",
               "Watermark and page numbers", "Copy, cut and paste pages"]),
    ("Forms", ["Fill text, check, radio, list and drop-down fields", "Create text, check, radio, list, drop-down and date fields",
               "Find blanks in flat forms", "Fill copies from a CSV sheet", "Export answers to CSV or Excel", "Reset a form", "Flatten a form"]),
    ("Measuring and comparing", ["Ruler and protractor", "Units and drawing scale", "Rulers on the page", "Keep measurements",
                                 "Side by side (left / right, top / bottom)", "Linked or separate scrolling", "Compare text", "Visual overlay compare"]),
    ("Printing", ["Live preview of every sheet while choosing settings", "1, 2, 4, 6 or 9 pages per sheet", "Fit or actual size", "Greyscale printing",
                  "Two-sided printing", "Printer Properties", "Copies and page ranges"]),
    ("Security and privacy", ["AES-256 password protection", "Permission limits", "Digital signatures with time stamps", "Visible signature stamps",
                              "Make your own certificate", "Check signatures", "Redaction with a check", "Remove hidden data with a report"]),
    ("Converting and tools", ["OCR for scanned pages", "Fonts report", "PDF/A-2b copies", "Export to PNG, JPEG, text, Markdown, Word, Excel",
                              "Text and Markdown to PDF", "Pictures to PDF", "Compress", "Batch processing with presets",
                              "Command line for automation", "Crash recovery of unsaved work"]),
]
cells = []
for title, items in features:
    block = [Paragraph(title, ParagraphStyle("ft", parent=S["cellb"], textColor=NAVY, fontSize=9.6, spaceAfter=3))]
    block += [Paragraph(i, S["cell"], bulletText="•") for i in items]
    cells.append(block)
rows = [cells[i:i + 3] for i in range(0, len(cells), 3)]
ft = Table(rows, colWidths=[56.6 * mm] * 3)
ft.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 6), ("RIGHTPADDING", (0, 0), (-1, -1), 6),
                        ("TOPPADDING", (0, 0), (-1, -1), 7), ("BOTTOMPADDING", (0, 0), (-1, -1), 9), ("BOX", (0, 0), (-1, -1), 0.5, RULE),
                        ("INNERGRID", (0, 0), (-1, -1), 0.5, RULE), ("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#FBFCFE"))]))
story += [ft, PageBreak()]

# ---------- 6 shortcuts ----------
story += [P("6. Keyboard shortcuts", "h1")]
keys = [
    ("Files and tabs", [("Ctrl+O", "Open"), ("Ctrl+S", "Save"), ("Ctrl+Shift+S", "Save as"), ("Ctrl+W", "Close tab"), ("Ctrl+Tab", "Next tab"),
                        ("Ctrl+P", "Print (with preview)"), ("Ctrl+D", "Document properties"), ("F1", "This user guide"), ("Ctrl+Q", "Quit")]),
    ("Moving around", [("PgUp / PgDn", "Previous / next page"), ("Left / Right", "Previous / next page"), ("Home / End", "First / last page"), ("Ctrl+G", "Go to page"),
                       ("Alt+Left / Alt+Right", "Back / forward"), ("Ctrl+B", "Bookmarks panel"), ("Ctrl+Shift+B", "Thumbnails panel")]),
    ("View", [("Ctrl+2", "Fit width"), ("Ctrl+0", "Fit page"), ("Ctrl+1", "Actual size"), ("Ctrl+wheel", "Zoom"), ("Ctrl+L / Ctrl+R", "Rotate view"),
              ("F11", "Full screen"), ("F5", "Presentation")]),
    ("Finding", [("Ctrl+F", "Find"), ("F3 / Shift+F3", "Next / previous match"), ("Ctrl+Shift+H", "Find and replace"), ("Ctrl+K", "Command palette")]),
    ("Editing", [("Ctrl+Z / Ctrl+Y", "Undo / redo"), ("Ctrl+E", "Edit text (a line)"), ("F2", "Edit paragraph"),
                 ("Enter / Ctrl+Enter", "Apply a line / a paragraph"), ("Esc", "Cancel"), ("Ctrl+C / Ctrl+A", "Copy / select all"),
                 ("Ctrl+V", "Paste a picture (or pages in the thumbnails)"), ("Del", "Delete the selected annotation or object")]),
    ("Annotating", [("Ctrl+H", "Highlight"), ("Ctrl+U", "Underline"), ("Ctrl+Shift+K", "Strikethrough"), ("Ctrl+M", "Comment"),
                    ("Ctrl+Shift+M", "Comment on selected text"), ("Ctrl+Shift+U", "Read aloud")]),
]
rows = [["Group", "Keys", "Action"]]
for group, items in keys:
    for i, (k, a) in enumerate(items):
        rows.append([group if i == 0 else "", "<b>%s</b>" % k, a])
story += [table(rows, [34, 46, 90], pad=1.6)]
story += [PageBreak()]

# ---------- 7 command line ----------
story += [P("7. Command line", "h1")]
story += [P("Every command below works without opening a window, so Feather PDF can be used in scripts and scheduled tasks. "
            "An exit code of 0 means success. Paths with spaces need quotes.")]
cmds = [
    ("FeatherPDF.exe file.pdf", "Open the file in the running window"),
    ("--export png|jpg|txt|md|docx|xlsx|csv in.pdf out [dpi]", "Export pages or text"),
    ("--compress in.pdf out.pdf [dpi]", "Make a file smaller"),
    ("--protect in.pdf out.pdf open-password [owner-password]", "Add AES-256 password protection"),
    ("--ocr in.pdf out.pdf [language]", "Recognise text on scanned pages"),
    ("--sign in.pdf out.pdf cert.pfx password [timestamp-server|-] [visible]", "Sign digitally"),
    ("--checksig file.pdf report.txt", "Check the signatures"),
    ("--batch preset.ini", "Run a saved batch preset"),
    ("--search folder text report.txt", "Search every PDF in a folder"),
    ("--redact in.pdf out.pdf text", "Redact every occurrence and check it is gone"),
    ("--formdetect in.pdf out.pdf", "Make fillable fields where a flat form has blanks"),
    ("--formfill form.pdf data.csv outdir", "One filled-in copy per CSV row"),
    ("--formexport form.pdf answers.csv|xlsx", "Export the answers"),
    ("--visualdiff a.pdf b.pdf overlay.png [page]", "Overlay two pages to show differences"),
    ("--topdf in.txt|in.md out.pdf", "Plain text or Markdown to PDF"),
    ("--sanitize in.pdf out.pdf [comments] [bookmarks]", "Remove hidden data (report in out.pdf.txt)"),
    ("--pages in.pdf out.pdf reverse|noblanks|nodupes|header|background", "Page tools"),
    ("--pdfa in.pdf out.pdf [pictures]", "PDF/A-2b copy (report in out.pdf.txt)"),
    ("--fonts in.pdf report.txt", "Fonts report, and pages without text"),
]
rows = [["Command", "What it does"]] + [[Paragraph(c.replace("|", "|​"), S["mono"]), d] for c, d in cmds]
story += [table(rows, [104, 66], pad=2)]

# ---------- 8 tips ----------
story += [P("8. Tips and good to know", "h1")]
story += bullets([
    "Right-click almost anything (the page, selected text, an annotation, a picture, a thumbnail) for the commands that fit it.",
    "Undo (up to 30 steps) works for every kind of change: annotations, text edits, page changes, pictures, flattening.",
    "Feather never changes the original file until you save. Save as keeps the original and writes a new file.",
    "If Windows or the computer stops unexpectedly, Feather offers your open documents and unsaved changes the next time it starts.",
    "Edited and newly added text uses Western characters with the standard PDF fonts. Text in other scripts can be read, searched and copied, but not typed in.",
    "Pages made by Redact become pictures, so the removed text cannot be recovered.",
    "To make Feather your default PDF viewer, choose Settings > Advanced > Set as default.",
    "This guide is always at hand: press <b>F1</b>, or choose … > User guide.",
])
story += bullets(["<b>Free software inside:</b> PDFium (BSD licence) reads and draws PDF files and Tesseract (Apache licence) recognises text. "
                   "Their licences are installed next to the program in the <i>licenses</i> folder and listed in THIRD_PARTY_NOTICES.md."])


doc = Doc(OUT)
doc.multiBuild(story)
print("wrote", OUT)
