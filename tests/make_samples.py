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
print("samples written to", out)
