"""Creates the sample PDFs used by smoke.sh. Needs: pip install reportlab pillow"""
import sys, os, random
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, ListFlowable, ListItem
from reportlab.lib.styles import getSampleStyleSheet
from reportlab.lib import colors
from PIL import Image, ImageDraw, ImageFont

out = sys.argv[1]
os.makedirs(out, exist_ok=True)
st = getSampleStyleSheet()

# a report with headings, a bullet list and a table (export test)
doc = SimpleDocTemplate(f"{out}/rich.pdf", pagesize=A4)
doc.build([Paragraph("Quarterly Report", st['Title']), Paragraph("1. Summary", st['Heading1']),
           Paragraph("A normal paragraph of body text that is long enough to wrap onto a second line when it is rendered.", st['BodyText']),
           Paragraph("2. Details", st['Heading2']),
           ListFlowable([ListItem(Paragraph("First point", st['BodyText'])), ListItem(Paragraph("Second point", st['BodyText']))], bulletType='bullet'),
           Spacer(1, 12),
           Table([["Item", "Qty", "Price"], ["Apples", "3", "1.50"], ["Pears", "12", "2,250.75"]],
                 style=TableStyle([('GRID', (0, 0), (-1, -1), 0.5, colors.grey)]), colWidths=[150, 100, 100])])

# a big picture (compress test)
random.seed(1)
im = Image.new("RGB", (3000, 2000)); d = ImageDraw.Draw(im)
for _ in range(400):
    x0, x1 = sorted((random.randint(0, 3000), random.randint(0, 3000))); y0, y1 = sorted((random.randint(0, 2000), random.randint(0, 2000)))
    d.ellipse([x0, y0, x1, y1], fill=tuple(random.randint(0, 255) for _ in range(3)))
im.save(f"{out}/big.png")
c = canvas.Canvas(f"{out}/img.pdf", pagesize=A4); c.drawString(72, 780, "Picture test"); c.drawImage(f"{out}/big.png", 72, 420, width=450, height=300); c.save()

# a scanned page, picture only (OCR test)
fonts = [f for f in ["/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"] if os.path.exists(f)]
font = ImageFont.truetype(fonts[0], 54)
scan = Image.new("L", (2480, 3508), 255); d = ImageDraw.Draw(scan)
for i, line in enumerate(["This agreement is made between the parties on the first day of May.", "The supplier shall deliver 500 units of the product to the buyer."]):
    d.text((250, 400 + i * 110), line, font=font, fill=0)
scan.save(f"{out}/scan.png")
c = canvas.Canvas(f"{out}/scan.pdf", pagesize=A4); c.drawImage(f"{out}/scan.png", 0, 0, width=595.27, height=841.89); c.save()
# paragraphs in an embedded font: ragged, justified and serif (paragraph editing test), and one at the foot of a page
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_JUSTIFY
sans = next((f for f in ["/usr/share/fonts/truetype/crosextra/Carlito-Regular.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"] if os.path.exists(f)), None)
serif = next((f for f in ["/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf"] if os.path.exists(f)), None)
pdfmetrics.registerFont(TTFont("BodySans", sans)); pdfmetrics.registerFont(TTFont("BodySerif", serif))
t = ("The quarterly maintenance review covers every pump, valve and compressor on the north site. "
     "Inspectors found that most equipment is in good condition, although two cooling pumps show early "
     "signs of bearing wear and should be replaced before the summer peak. The budget for this work was "
     "approved in March and the parts have already been ordered from the usual supplier.")
ps = ParagraphStyle("p", fontName="BodySans", fontSize=11, leading=13.4)
doc = SimpleDocTemplate(f"{out}/para.pdf", pagesize=A4, leftMargin=72, rightMargin=72, topMargin=72)
doc.build([Paragraph(t, ps), Spacer(1, 18), Paragraph(t, ParagraphStyle("j", parent=ps, alignment=TA_JUSTIFY)), Spacer(1, 18),
           Paragraph(t, ParagraphStyle("s", fontName="BodySerif", fontSize=12, leading=14.5))])
c = canvas.Canvas(f"{out}/foot.pdf", pagesize=A4); c.setFont("Helvetica", 12)
c.drawString(72, 60, "This paragraph sits at the very bottom of the page so that"); c.drawString(72, 45.6, "making it longer leaves no room for another line below it."); c.save()
# word spacing, letter spacing, kerning and rise on one page (other text must keep them when the page is edited)
import pikepdf
sp = pikepdf.new()
helv = sp.make_indirect(pikepdf.Dictionary(Type=pikepdf.Name.Font, Subtype=pikepdf.Name.Type1, BaseFont=pikepdf.Name.Helvetica, Encoding=pikepdf.Name.WinAnsiEncoding))
body = (b"BT /F1 12 Tf 72 760 Td (Edit this first line of text here.) Tj ET\n"
        b"BT /F1 12 Tf 72 700 Td [(AV) -800 (WA) -300 (To) 500 (Kerned)] TJ ET\n"
        b"BT /F1 12 Tf 72 680 Td 6 Tw (Word spaced line here) Tj ET\n"
        b"BT /F1 12 Tf 72 660 Td 2 Tc (Letter spaced) Tj ET\n"
        b"BT /F1 12 Tf 72 640 Td 5 Ts (Raised) Tj 0 Ts ( base) Tj ET\n")
sp.pages.append(pikepdf.Page(pikepdf.Dictionary(Type=pikepdf.Name.Page, MediaBox=[0, 0, 595, 842], Contents=sp.make_stream(body),
                                                Resources=pikepdf.Dictionary(Font=pikepdf.Dictionary(F1=helv)))))
sp.save(f"{out}/spacing.pdf")
print("samples written to", out)

# ---- tables (detection and editing tests) ----
# 1. ruled: a grid with a shaded, bold header, a cell merged across two columns and a total right-aligned
doc = SimpleDocTemplate(f"{out}/tbl_ruled.pdf", pagesize=A4)
doc.build([Paragraph("Ruled table", st['Heading1']),
           Table([["Item", "Qty", "Unit price", "Total"],
                  ["Electrodes E7018", "1,200", "0.45", "540.00"],
                  ["Shielding gas (cyl.)", "36", "42.00", "1,512.00"],
                  ["Grinding discs", "400", "1.10", "440.00"],
                  ["Grand total", "", "", "2,492.00"]],
                 colWidths=[180, 70, 90, 90],
                 style=TableStyle([('GRID', (0, 0), (-1, -1), 0.75, colors.black),
                                   ('BACKGROUND', (0, 0), (-1, 0), colors.HexColor("#1F3864")),
                                   ('TEXTCOLOR', (0, 0), (-1, 0), colors.white),
                                   ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
                                   ('ALIGN', (1, 1), (-1, -1), 'RIGHT'),
                                   ('SPAN', (0, 4), (2, 4))])),
           Spacer(1, 12), Paragraph("Text after the table stays where it is.", st['BodyText'])])

# 2. rules only: a line above and below the header and one at the bottom (no vertical lines)
doc = SimpleDocTemplate(f"{out}/tbl_rules.pdf", pagesize=A4)
doc.build([Paragraph("Rules only", st['Heading1']),
           Table([["Month", "Hours", "Welds", "Rejects"], ["January", "412", "1,830", "12"], ["February", "398", "1,744", "9"],
                  ["March", "441", "1,902", "15"], ["April", "405", "1,795", "8"]],
                 colWidths=[120, 80, 80, 80],
                 style=TableStyle([('LINEABOVE', (0, 0), (-1, 0), 1, colors.black), ('LINEBELOW', (0, 0), (-1, 0), 0.5, colors.black),
                                   ('LINEBELOW', (0, -1), (-1, -1), 1, colors.black), ('ALIGN', (1, 0), (-1, -1), 'RIGHT')]))])

# 3. no lines: columns of text only
c = canvas.Canvas(f"{out}/tbl_plain.pdf", pagesize=A4)
c.setFont("Helvetica-Bold", 16); c.drawString(72, 780, "Plain columns")
rows = [("Code", "Description", "Stock", "Location"), ("A-101", "Hex bolt M8", "1,250", "Shelf 3"), ("A-102", "Hex nut M8", "2,400", "Shelf 3"),
        ("B-220", "Washer 8 mm", "3,100", "Shelf 4"), ("C-310", "Rivet 4 mm", "900", "Shelf 7")]
for i, r in enumerate(rows):
    c.setFont("Helvetica-Bold" if i == 0 else "Helvetica", 11)
    y = 740 - i * 18
    c.drawString(72, y, r[0]); c.drawString(150, y, r[1]); c.drawRightString(330, y, r[2]); c.drawString(360, y, r[3])
c.setFont("Helvetica", 11)
c.drawString(72, 600, "A paragraph below the table, which is not part of it and must not be taken in.")
c.save()

# 4. two columns of running text: not a table
c = canvas.Canvas(f"{out}/tbl_twocol.pdf", pagesize=A4)
c.setFont("Helvetica", 10)
words = ("Welding joins metals by melting them together and letting them cool so that they fuse into one piece of metal "
         "that is as strong as the parts it was made from when it is done well by a trained welder").split()
for col, x in enumerate((72, 320)):
    line, y = "", 780
    for w in words * 3:
        if c.stringWidth(line + " " + w, "Helvetica", 10) > 210:
            c.drawString(x, y, line); y -= 13; line = w
        else:
            line = (line + " " + w).strip()
c.save()

# 5. a scanned ruled table (a picture of tbl_ruled's grid with its words)
scanT = Image.new("L", (2480, 3508), 255); d = ImageDraw.Draw(scanT)
f2 = ImageFont.truetype(fonts[0], 44)
cols = [300, 1050, 1350, 1700, 2100]; rws = [500, 610, 720, 830]
for x in cols: d.line([(x, rws[0]), (x, rws[-1])], fill=0, width=4)
for y in rws: d.line([(cols[0], y), (cols[-1], y)], fill=0, width=4)
cells = [["Item", "Qty", "Price", "Total"], ["Bolts", "120", "0.40", "48.00"], ["Nuts", "300", "0.10", "30.00"]]
for r, row in enumerate(cells):
    for k, txt in enumerate(row):
        d.text((cols[k] + 20, rws[r] + 30), txt, font=f2, fill=0)
scanT.save(f"{out}/tbl_scan.png")
c = canvas.Canvas(f"{out}/tbl_scan.pdf", pagesize=A4); c.drawImage(f"{out}/tbl_scan.png", 0, 0, width=595.27, height=841.89); c.save()

# ---- headers and footers (Delete header and footer) ----
def logo(path):
    im = Image.new("RGB", (60, 60), (220, 50, 50)); dd = ImageDraw.Draw(im)
    dd.rectangle([18, 12, 26, 48], fill=(255, 255, 255)); dd.rectangle([18, 12, 44, 20], fill=(255, 255, 255)); dd.rectangle([18, 28, 38, 35], fill=(255, 255, 255))
    im.save(path)
logo(f"{out}/hf_logo.png")
body = ("Welding joins metals by melting them together and letting them cool so that they fuse into one piece. "
        "Inspection planning and distortion control matter for large structures such as draglines and shovels. ").split()

def body_text(c, page, top=748):
    y = top; line = f"Body text of page {page}:"
    for w in body * 4:
        if c.stringWidth(line + " " + w, "Helvetica", 11) > 440:
            c.drawString(70, y, line); y -= 15; line = w
        else:
            line = (line + " " + w).strip()
    c.drawString(70, y, line)

# 1. a report: logo, running title, date and a rule at the top; a rule and "Page n of 6" at the foot; body text that
#    starts inside the top strip (different on every page: it must stay); a link and bookmarks that must survive
c = canvas.Canvas(f"{out}/hf_basic.pdf", pagesize=A4)
for p in range(1, 7):
    c.drawImage(f"{out}/hf_logo.png", 50, 790, width=28, height=28)
    c.setFont("Helvetica", 10); c.drawString(90, 800, "Annual Plant Report 2026"); c.drawRightString(545, 800, "30 October 2026")
    c.setLineWidth(0.8); c.line(50, 782, 545, 782)
    c.setFont("Helvetica", 11); body_text(c, p)
    c.linkURL("https://example.com/p%d" % p, (70, 735, 300, 760))
    c.bookmarkPage("p%d" % p); c.addOutlineEntry("Page %d" % p, "p%d" % p)
    c.setLineWidth(0.5); c.line(50, 48, 545, 48)
    c.setFont("Helvetica", 9); c.drawCentredString(297.6, 30, "Page %d of 6" % p)
    c.showPage()
c.save()

# 2. a book: odd pages have the chapter on the right and the number bottom right, even pages the title on the left and
#    the number bottom left
c = canvas.Canvas(f"{out}/hf_oddeven.pdf", pagesize=A4)
for p in range(1, 7):
    c.setFont("Helvetica-Oblique", 9)
    if p % 2: c.drawRightString(545, 805, "Chapter 1: Getting started"); c.drawRightString(545, 28, str(p))
    else: c.drawString(50, 805, "Feather PDF Handbook"); c.drawString(50, 28, str(p))
    c.setFont("Helvetica", 11); body_text(c, p, 760)
    c.showPage()
c.save()

# 3. shared form XObjects: a letterhead form at the top of every page, and a whole-page template (a header line and a note
#    in the middle of the page) on every page; the page number is ordinary page content
c = canvas.Canvas(f"{out}/hf_xobject.pdf", pagesize=A4)
c.beginForm("letterhead"); c.setFont("Helvetica-Bold", 12); c.drawString(50, 805, "Shared Letterhead Ltd"); c.setLineWidth(1); c.line(50, 798, 545, 798); c.endForm()
c.beginForm("template"); c.setFont("Helvetica", 8); c.drawString(400, 820, "Template header line"); c.drawString(70, 420, "Template note in the body"); c.endForm()
for p in range(1, 5):
    c.doForm("letterhead"); c.doForm("template")
    c.setFont("Helvetica", 11); body_text(c, p, 740)
    c.setFont("Helvetica", 9); c.drawCentredString(297.6, 30, "- %d -" % p)
    c.showPage()
c.save()

# 4. scanned pages: a header and a page number drawn into each page's picture (smoke.sh recognises the text first)
fh = ImageFont.truetype(fonts[0], 40); fb = ImageFont.truetype(fonts[0], 44)
c = canvas.Canvas(f"{out}/hf_scan.pdf", pagesize=A4)
for p in range(1, 4):
    im = Image.new("L", (1654, 2339), 255); dd = ImageDraw.Draw(im)
    dd.text((140, 110), "ACME CONFIDENTIAL REPORT", font=fh, fill=0)
    dd.line([(140, 170), (1514, 170)], fill=0, width=3)
    for k, ln in enumerate(["Scanned body text on page %d with some words" % p, "that continue on a second line of the scan", "and a third line to make it a real page."]):
        dd.text((140, 420 + 70 * k), ln, font=fb, fill=0)
    dd.text((780, 2230), "Page %d" % p, font=fh, fill=0)
    im.save(f"{out}/hf_scan{p}.png")
    c.drawImage(f"{out}/hf_scan{p}.png", 0, 0, width=595.27, height=841.89); c.showPage()
c.save()

# 5. one page: nothing repeats, so nothing may be found
c = canvas.Canvas(f"{out}/hf_one.pdf", pagesize=A4)
c.setFont("Helvetica", 10); c.drawString(90, 800, "A title at the top"); c.setFont("Helvetica", 11); body_text(c, 1); c.drawCentredString(297.6, 30, "1")
c.save()

# 6. a document with two identical pages (1 and 5) among five: their title and closing lines are not a header or footer
c = canvas.Canvas(f"{out}/hf_dup.pdf", pagesize=A4)
for p in [1, 2, 3, 4, 1]:
    heads = {1: "Annual Plant Report 2026", 2: "Production figures", 3: "Maintenance work", 4: "Inspection results"}
    ends = {1: "Prepared by the plant office.", 2: "Figures are rounded.", 3: "See the appendix for the schedule.", 4: "Results checked in March."}
    c.setFont("Helvetica-Bold", 16); c.drawString(70, 770, heads[p])
    c.setFont("Helvetica", 11); body_text(c, p, 730)
    c.drawString(70, 60, ends[p])
    c.showPage()
c.save()

# 7. a Word document (header with three parts, footer "Page X of Y"), made into a PDF by LibreOffice when it is installed
try:
    import docx, shutil, subprocess
    from docx.oxml.ns import qn
    from docx.oxml import OxmlElement
    from docx.enum.text import WD_ALIGN_PARAGRAPH
    if shutil.which("soffice"):
        dd = docx.Document()
        sec = dd.sections[0]
        sec.header.paragraphs[0].text = "Quarterly Operations Review\tConfidential\t30 October 2026"
        def field(run, code):
            a = OxmlElement('w:fldChar'); a.set(qn('w:fldCharType'), 'begin')
            t = OxmlElement('w:instrText'); t.set(qn('xml:space'), 'preserve'); t.text = code
            b = OxmlElement('w:fldChar'); b.set(qn('w:fldCharType'), 'end')
            run._r.append(a); run._r.append(t); run._r.append(b)
        fp = sec.footer.paragraphs[0]; fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
        fp.add_run("Page "); field(fp.add_run(), "PAGE"); fp.add_run(" of "); field(fp.add_run(), "NUMPAGES")
        random.seed(3)
        ws = "the plant output rose in the third quarter while maintenance costs fell steadily across all sites".split()
        for i in range(40): dd.add_paragraph(" ".join(random.choice(ws) for _ in range(60)).capitalize() + ".")
        dd.save(f"{out}/hf_word.docx")
        subprocess.run(["soffice", "--headless", "--convert-to", "pdf", "--outdir", out, f"{out}/hf_word.docx"], capture_output=True, timeout=180)
except Exception:
    pass

# 8. headings "Section 2" on page 2, "Section 3" on page 3...: large headings, not page numbers
c = canvas.Canvas(f"{out}/hf_section.pdf", pagesize=A4)
for p in range(1, 6):
    c.setFont("Helvetica-Bold", 16); c.drawString(70, 770, "Section %d" % p)
    c.setFont("Helvetica", 11); body_text(c, p, 730)
    c.showPage()
c.save()

# 9. a table on a page that is full below it: an edit that needs more room must still work (the text is set smaller)
c = canvas.Canvas(f"{out}/tbl_full.pdf", pagesize=A4)
c.setLineWidth(0.5)
xs = [70, 200, 520]; ys = [780, 762, 744, 726]
for x in xs: c.line(x, ys[-1], x, ys[0])
for y in ys: c.line(xs[0], y, xs[-1], y)
c.setFont("Helvetica", 11)
for r, (a, b) in enumerate([("Name", "Value"), ("Plant", "North site"), ("Shift", "Night")]):
    c.drawString(xs[0] + 4, ys[r] - 13, a); c.drawString(xs[1] + 4, ys[r] - 13, b)
y = 706
while y > 40:
    c.drawString(70, y, "Body text that fills the page down to its foot, so nothing below the table can move down."); y -= 14
c.save()

# 10. one drawing for the borders of two tables and a rule across the page (as Word and LibreOffice often draw them)
c = canvas.Canvas(f"{out}/tbl_sharedpath.pdf", pagesize=A4)
c.setLineWidth(0.6)
p = c.beginPath()
def grid(p, x0, x1, y0, rows, cols, h=18):
    for k in range(rows + 1): p.moveTo(x0, y0 - k * h); p.lineTo(x1, y0 - k * h)
    for k in range(cols + 1):
        x = x0 + (x1 - x0) * k / cols; p.moveTo(x, y0); p.lineTo(x, y0 - rows * h)
grid(p, 70, 400, 780, 3, 2)
grid(p, 70, 400, 640, 2, 3)
p.moveTo(70, 560); p.lineTo(525, 560)
c.drawPath(p, stroke=1, fill=0)
c.setFont("Helvetica", 11)
for r, row in enumerate([("Item", "Qty"), ("Bolts", "120"), ("Nuts", "300")]):
    for k, v in enumerate(row): c.drawString(74 + k * 165, 780 - r * 18 - 13, v)
for r, row in enumerate([("A", "B", "C"), ("1", "2", "3")]):
    for k, v in enumerate(row): c.drawString(74 + k * 110, 640 - r * 18 - 13, v)
c.drawString(70, 545, "Text under the rule.")
c.save()

# 11. an exam paper made in Word (LibreOffice): USN boxes, a letterhead with a framed note, a details table and a
#     question table with a thick yellow border round one question, all touching or close
try:
    import docx, shutil, subprocess
    from docx.shared import Pt, Cm
    from docx.enum.text import WD_ALIGN_PARAGRAPH
    from docx.enum.table import WD_TABLE_ALIGNMENT
    from docx.oxml.ns import qn
    from docx.oxml import OxmlElement
    if shutil.which("soffice"):
        im = Image.new("RGB", (240, 260), "white"); ImageDraw.Draw(im).polygon([(120, 10), (20, 200), (220, 200)], fill=(40, 70, 160)); im.save(f"{out}/qp_logo.png")
        dd = docx.Document(); dd.styles['Normal'].font.name = 'Calibri'; dd.styles['Normal'].font.size = Pt(12)
        sec = dd.sections[0]; sec.top_margin = Cm(1.5); sec.bottom_margin = Cm(1.5); sec.left_margin = Cm(2); sec.right_margin = Cm(2)
        def borders(cell, **kw):
            pr = cell._tc.get_or_add_tcPr(); b = OxmlElement('w:tcBorders')
            for edge in ('top', 'left', 'bottom', 'right'):
                e = OxmlElement('w:' + edge); e.set(qn('w:val'), kw.get(edge, kw.get('all', 'single'))); e.set(qn('w:sz'), str(kw.get('sz', 8))); e.set(qn('w:color'), kw.get('color', '000000')); b.append(e)
            pr.append(b)
        def put(cell, text, bold=False, size=12, align=None):
            p = cell.paragraphs[0]; r = p.add_run(text); r.bold = bold; r.font.size = Pt(size)
            if align is not None: p.alignment = align
        def widths(t, ws):
            t.autofit = False
            for row in t.rows:
                for cc, w in zip(row.cells, ws): cc.width = Cm(w)
            for gc, w in zip(t._tbl.tblGrid.findall(qn('w:gridCol')), ws): gc.set(qn('w:w'), str(int(w * 567)))
        p = dd.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.RIGHT; r = p.add_run("AIT/IQAC/Aca/2026-27/IA-QP"); r.bold = True; r.font.size = Pt(14)
        usn = dd.add_table(rows=1, cols=11); usn.alignment = WD_TABLE_ALIGNMENT.RIGHT; usn.style = 'Table Grid'; put(usn.cell(0, 0), "USN", True, 14)
        top = dd.add_table(rows=1, cols=3); c0, c1, c2 = top.rows[0].cells
        borders(c0, all='nil'); borders(c1, all='nil'); borders(c2)
        c0.paragraphs[0].add_run().add_picture(f"{out}/qp_logo.png", width=Cm(2.6))
        put(c1, "ACHARYA INSTITUTE OF TECHNOLOGY", True, 13, WD_ALIGN_PARAGRAPH.CENTER)
        q = c1.add_paragraph(); q.alignment = WD_ALIGN_PARAGRAPH.CENTER; rr = q.add_run("Bengaluru – 560107"); rr.bold = True; rr.font.size = Pt(13)
        put(c2, "For P cycle branches only", True, 18)
        t = dd.add_table(rows=4, cols=4); t.style = 'Table Grid'
        put(t.cell(0, 0).merge(t.cell(0, 3)), "INTERNAL ASSESSMENT – I [Academic Year: 2026-27]", True, 16, WD_ALIGN_PARAGRAPH.CENTER)
        for i, row in enumerate([("Course Name:", "Essentials of Computers", "Course Code:", "1BESC204E"), ("Semester/Section:", "", "Max Marks:", "25"), ("Date:", "", "Duration:", "60 mins")], 1):
            for j, v in enumerate(row): put(t.cell(i, j), v, j == 3, 14, WD_ALIGN_PARAGRAPH.RIGHT if j in (0, 2) else WD_ALIGN_PARAGRAPH.CENTER if j == 3 else None)
        dd.add_paragraph()
        qt = dd.add_table(rows=6, cols=5); qt.style = 'Table Grid'
        put(qt.cell(0, 0).merge(qt.cell(0, 4)), "Note: Answer ONE question from each OR pair", True, 14, WD_ALIGN_PARAGRAPH.CENTER)
        for j, h in enumerate(["Q. No.", "Questions", "CO", "RBT", "Marks"]): put(qt.cell(1, j), h, True, 14, WD_ALIGN_PARAGRAPH.CENTER)
        rws = [("1", "Explain how the CPU, main memory and input/output units cooperate while a computer executes a program."), None,
               ("2", "Explain signed integer representation using n‑bit two’s complement. Describe how the sign and value are obtained from a bit pattern."),
               ("3", "Explain the roles of memory addresses and read/write operations in storing and retrieving data from main memory.")]
        for k, qq in enumerate(rws, 2):
            if qq is None: put(qt.cell(k, 0).merge(qt.cell(k, 4)), "OR", True, 14, WD_ALIGN_PARAGRAPH.CENTER); continue
            put(qt.cell(k, 0), qq[0], True, 14, WD_ALIGN_PARAGRAPH.CENTER); put(qt.cell(k, 1), qq[1], False, 14)
            for j, v in ((2, "CO1"), (3, "L2"), (4, "5")): put(qt.cell(k, j), v, False, 14, WD_ALIGN_PARAGRAPH.CENTER)
        for cc in qt.rows[5].cells: borders(cc, sz=24, color='F0B400')
        widths(top, [3.4, 9.0, 4.6]); widths(t, [4.2, 5.0, 4.0, 3.8]); widths(qt, [1.4, 11.0, 1.4, 1.3, 1.9])
        dd.save(f"{out}/qp.docx")
        subprocess.run(["soffice", "--headless", "--convert-to", "pdf", "--outdir", out, f"{out}/qp.docx"], capture_output=True, timeout=180)
except Exception:
    pass
