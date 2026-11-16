"""The document the Android build check opens on the emulator: a heading, a paragraph to edit, a red box, three pages."""
import sys
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas

c = canvas.Canvas(sys.argv[1], pagesize=A4)
c.setTitle("Feather PDF Android check")
c.setFont("Helvetica-Bold", 18); c.drawString(72, 760, "Quarterly Report")
c.setFont("Helvetica", 12)
c.drawString(72, 700, "Sales rose in the third quarter, and costs stayed where they were.")
c.drawString(72, 685, "The new shop opened in March and did well from its first week.")
c.setFillColorRGB(0.85, 0.1, 0.1); c.rect(72, 520, 200, 100, fill=1, stroke=0); c.setFillColorRGB(0, 0, 0)
c.bookmarkPage("p1"); c.addOutlineEntry("Report", "p1", 0)
c.showPage()
for i in (2, 3):
    c.setFont("Helvetica", 12); c.drawString(72, 700, f"Page {i} of the report.")
    c.bookmarkPage(f"p{i}"); c.addOutlineEntry(f"Page {i}", f"p{i}", 0)
    c.showPage()
c.save()
