"""The document the Android build check opens on the emulator (its pictures also serve as the store's screenshots):
a short report with a heading, a paragraph to edit, a chart and a list, three pages. The test taps the paragraph at
(100, 700) and sweeps the heading at y 765 (page points from the bottom left)."""
import sys
from reportlab.lib.pagesizes import A4
from reportlab.pdfgen import canvas

c = canvas.Canvas(sys.argv[1], pagesize=A4)
c.setTitle("Quarterly Report")
c.setFillColorRGB(0.78, 0.16, 0.16); c.rect(0, 800, 595, 42, fill=1, stroke=0)
c.setFillColorRGB(1, 1, 1); c.setFont("Helvetica-Bold", 11); c.drawString(72, 816, "NORTHWIND TRADING  ·  Q3 2026")
c.setFillColorRGB(0.1, 0.12, 0.16); c.setFont("Helvetica-Bold", 18); c.drawString(72, 760, "Quarterly Report")
c.setFont("Helvetica", 12)
c.drawString(72, 700, "Sales rose in the third quarter, and costs stayed where they were.")
c.drawString(72, 685, "The new shop opened in March and did well from its first week.")
c.setFont("Helvetica", 11); c.setFillColorRGB(0.25, 0.27, 0.3)
for i, line in enumerate(["Online orders grew by a third, most of them from returning customers. Deliveries",
                          "now reach every district within two days, and returns fell to under two per cent.",
                          "The team expects a busy fourth quarter and has added staff for the festive season."]):
    c.drawString(72, 650 - 15 * i, line)
c.setFillColorRGB(0.1, 0.12, 0.16); c.setFont("Helvetica-Bold", 12); c.drawString(72, 590, "Sales by month (lakh rupees)")
for i, (m, v) in enumerate([("Jul", 42), ("Aug", 55), ("Sep", 71)]):
    x = 90 + i * 70
    c.setFillColorRGB(0.12, 0.47, 0.71); c.rect(x, 470, 40, v * 1.5, fill=1, stroke=0)
    c.setFillColorRGB(0.2, 0.2, 0.2); c.setFont("Helvetica", 10); c.drawString(x + 10, 456, m); c.drawString(x + 10, 474 + v * 1.5, str(v))
c.setFillColorRGB(0.1, 0.12, 0.16); c.setFont("Helvetica-Bold", 12); c.drawString(330, 590, "Next steps")
c.setFont("Helvetica", 11)
for i, t in enumerate(["Open two more shops", "Train the new staff", "Review the price list", "Plan the year-end sale"]):
    c.drawString(330, 565 - 18 * i, "•  " + t)
c.setFont("Helvetica", 9); c.setFillColorRGB(0.5, 0.5, 0.5); c.drawString(72, 40, "Northwind Trading · Quarterly Report · Page 1")
c.bookmarkPage("p1"); c.addOutlineEntry("Report", "p1", 0)
c.showPage()
for i in (2, 3):
    c.setFont("Helvetica-Bold", 14); c.drawString(72, 760, f"Part {i}")
    c.setFont("Helvetica", 12); c.drawString(72, 700, f"Page {i} of the report.")
    c.bookmarkPage(f"p{i}"); c.addOutlineEntry(f"Part {i}", f"p{i}", 0)
    c.showPage()
c.save()
