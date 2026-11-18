using System;
using System.Collections;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Reports
{
	private const float PageW = 842f;

	private const float PageH = 595f;

	public static void SavePdfReport(Project project, string path, bool includeParts, string explanation)
	{
		BuildPdf(project, includeParts, explanation).Save(path);
	}

	/// <summary>Builds the report (sheets, parts list, explanation) without saving it, so it can be previewed first.</summary>
	public static PdfDocument BuildPdf(Project project, bool includeParts, string explanation)
	{
		PdfDocument pdfDocument = new PdfDocument();
		project.UpdateCrossReferences();
		checked
		{
			int num = (includeParts ? 1 : 0) + ((!string.IsNullOrWhiteSpace(explanation)) ? 1 : 0);
			int sheets = project.Pages.Count + num;
			int num2 = 0;
			foreach (ProjectPage page4 in project.Pages)
			{
				num2++;
				PdfPage page = pdfDocument.AddPage(842f, 595f);
				VectorExport.DrawShapes(page, VectorExport.Record(page4.Circuit), new RectangleF(30f, 30f, 782f, 451f));
				VectorExport.DrawTitleBlock(page, project.Info, page4.Name, num2, sheets);
			}
			if (includeParts)
			{
				num2++;
				PdfPage page2 = pdfDocument.AddPage(842f, 595f);
				DrawPartsTable(page2, PartsList.Build(project));
				VectorExport.DrawTitleBlock(page2, project.Info, "Parts list", num2, sheets);
			}
			if (!string.IsNullOrWhiteSpace(explanation))
			{
				num2++;
				PdfPage page3 = pdfDocument.AddPage(842f, 595f);
				DrawText(page3, "How the circuit works", explanation);
				VectorExport.DrawTitleBlock(page3, project.Info, "Explanation", num2, sheets);
			}
			return pdfDocument;
		}
	}

	private static void DrawPartsTable(PdfPage page, List<PartLine> lines)
	{
		float num = 40f;
		float num2 = 52f;
		page.Text(num, num2, 14f, "Parts list", bold: true);
		num2 += 22f;
		(float, string)[] array = new(float, string)[5]
		{
			(num, "Qty"),
			(num + 40f, "Description"),
			(num + 400f, "Labels"),
			(num + 600f, "Unit price (Rs)"),
			(num + 700f, "Total (Rs)")
		};
		for (int i = 0; i < array.Length; i = checked(i + 1))
		{
			(float, string) tuple = array[i];
			page.Text(tuple.Item1, num2, 9f, tuple.Item2, bold: true);
		}
		num2 += 4f;
		page.Path(new PointF[2]
		{
			new PointF(num, num2),
			new PointF(802f, num2)
		}, closed: false, Color.Black, 0.8f, dashed: false, Color.Empty);
		foreach (PartLine line in lines)
		{
			num2 += 14f;
			if (num2 > 465f)
			{
				page.Text(num, num2, 9f, "... (continued: export the parts list as CSV for the full list)");
				break;
			}
			page.Text(num, num2, 9f, line.Quantity.ToString());
			page.Text(num + 40f, num2, 9f, line.Description);
			page.Text(num + 400f, num2, 9f, (line.Labels.Length > 40) ? (line.Labels.Substring(0, 40) + "...") : line.Labels);
			page.Text(num + 600f, num2, 9f, line.UnitPrice.ToString("#,##0.00"));
			page.Text(num + 700f, num2, 9f, line.Total.ToString("#,##0.00"));
		}
		num2 += 8f;
		page.Path(new PointF[2]
		{
			new PointF(num, num2),
			new PointF(802f, num2)
		}, closed: false, Color.Black, 0.8f, dashed: false, Color.Empty);
		page.Text(num + 600f, num2 + 14f, 10f, "Total", bold: true);
		page.Text(num + 700f, num2 + 14f, 10f, lines.Sum([SpecialName] (PartLine l) => l.Total).ToString("#,##0.00"), bold: true);
	}

	private static void DrawText(PdfPage page, string title, string body)
	{
		float num = 52f;
		page.Text(40f, num, 14f, title, bold: true);
		num += 20f;
		string[] array = body.Replace("\r", "").Split('\n');
		foreach (string text in array)
		{
			foreach (string item in Wrap(text, 150))
			{
				num += 11f;
				if (num > 475f)
				{
					page.Text(40f, num, 8f, "...");
					return;
				}
				page.Text(40f, num, 8f, item);
			}
		}
	}

	private static IEnumerable<string> Wrap(string text, int width)
	{
		// Restored from the VB iterator (Yield) that the decompiler emitted as a state machine.
		if (text.Length <= width)
		{
			yield return text;
			yield break;
		}
		string rest = text;
		while (rest.Length > width)
		{
			int cut = rest.LastIndexOf(' ', width);
			if (cut <= 0)
			{
				cut = width;
			}
			yield return rest.Substring(0, cut);
			rest = "      " + rest.Substring(cut).TrimStart();
		}
		yield return rest;
	}
}
