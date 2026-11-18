using System;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace PneuSim;

/// <summary>PneuSim's own line-art icons for ribbon commands, drawn in code (2 px strokes, rounded caps).</summary>
public static class RibbonIcons
{
	public static Bitmap Command(string key, ColorScheme scheme)
	{
		Bitmap bitmap = new Bitmap(62, 42);
		using Graphics graphics = Graphics.FromImage(bitmap);
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		Color color = (scheme.IsDark ? Color.FromArgb(70, 200, 210) : Color.FromArgb(0, 118, 132));
		Color color2 = (scheme.IsDark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(40, 52, 64));
		using Pen pen = new Pen(color2, 2f)
		{
			StartCap = LineCap.Round,
			EndCap = LineCap.Round,
			LineJoin = LineJoin.Round
		};
		using Pen pen2 = new Pen(color, 2.4f)
		{
			StartCap = LineCap.Round,
			EndCap = LineCap.Round,
			LineJoin = LineJoin.Round
		};
		using SolidBrush brush = new SolidBrush(color);
		switch (key)
		{
		case "Generator":
			graphics.DrawRectangle(pen, 12, 8, 16, 12);
			graphics.DrawRectangle(pen, 34, 22, 16, 12);
			graphics.DrawLines(pen2, new PointF[3]
			{
				new PointF(28f, 14f),
				new PointF(42f, 14f),
				new PointF(42f, 22f)
			});
			graphics.FillEllipse(brush, 8, 28, 6, 6);
			break;
		case "Check":
			graphics.DrawEllipse(pen, 12, 6, 38, 30);
			graphics.DrawLines(pen2, new PointF[3]
			{
				new PointF(22f, 21f),
				new PointF(29f, 28f),
				new PointF(42f, 13f)
			});
			break;
		case "Explain":
			graphics.DrawRectangle(pen, 12, 6, 38, 30);
			graphics.DrawLine(pen2, 18, 14, 44, 14);
			graphics.DrawLine(pen, 18, 21, 44, 21);
			graphics.DrawLine(pen, 18, 28, 36, 28);
			break;
		case "Calculators":
		case "Sizing":
			graphics.DrawRectangle(pen, 18, 5, 26, 32);
			graphics.DrawRectangle(pen2, 22, 9, 18, 7);
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					graphics.FillEllipse(brush, 22 + i * 7, 21 + j * 5, 3, 3);
				}
			}
			break;
		case "Parts":
			graphics.DrawRectangle(pen, 12, 6, 38, 30);
			graphics.DrawLine(pen, 12, 14, 50, 14);
			graphics.DrawLine(pen2, 24, 14, 24, 36);
			graphics.DrawLine(pen, 12, 25, 50, 25);
			break;
		case "Sweep":
			graphics.DrawLine(pen, 12, 34, 52, 34);
			graphics.DrawLine(pen, 12, 34, 12, 6);
			graphics.DrawCurve(pen2, new PointF[4]
			{
				new PointF(14f, 30f),
				new PointF(26f, 14f),
				new PointF(38f, 24f),
				new PointF(50f, 8f)
			});
			break;
		case "Cutaway":
		case "CutawayAll":
			graphics.DrawEllipse(pen, 14, 8, 34, 26);
			graphics.DrawLine(pen2, 31, 8, 31, 34);
			graphics.DrawLine(pen, 14, 21, 48, 21);
			break;
		case "NewFault":
			graphics.DrawLines(pen2, new PointF[5]
			{
				new PointF(14f, 30f),
				new PointF(26f, 16f),
				new PointF(32f, 26f),
				new PointF(40f, 10f),
				new PointF(50f, 22f)
			});
			graphics.FillEllipse(brush, 12, 28, 6, 6);
			break;
		case "Settings":
			graphics.DrawEllipse(pen, 20, 11, 22, 22);
			graphics.DrawEllipse(pen2, 27, 18, 8, 8);
			graphics.DrawLine(pen, 31, 6, 31, 11);
			graphics.DrawLine(pen, 31, 33, 31, 38);
			graphics.DrawLine(pen, 15, 22, 20, 22);
			graphics.DrawLine(pen, 42, 22, 47, 22);
			break;
		default:
			graphics.DrawRectangle(pen, 14, 8, 34, 26);
			break;
		}
		return bitmap;
	}
}

/// <summary>Line-art glyphs for ribbon commands, drawn in code on a 16 x 16 grid and scaled (small 16 px, large 36 px, quick access 16 px).</summary>
public static class RibbonGlyphs
{
	public static Bitmap Make(string key, ColorScheme scheme, int size)
	{
		int num = size * 4;
		using Bitmap bitmap = new Bitmap(num, num);
		using (Graphics graphics = Graphics.FromImage(bitmap))
		{
			graphics.SmoothingMode = SmoothingMode.AntiAlias;
			graphics.ScaleTransform((float)num / 16f, (float)num / 16f);
			Color color = (scheme.IsDark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(44, 56, 68));
			Color color2 = (scheme.IsDark ? Color.FromArgb(70, 200, 210) : Color.FromArgb(0, 118, 132));
			Draw(graphics, key, color, color2);
		}
		Bitmap bitmap2 = new Bitmap(size, size);
		using Graphics graphics2 = Graphics.FromImage(bitmap2);
		graphics2.InterpolationMode = InterpolationMode.HighQualityBicubic;
		graphics2.PixelOffsetMode = PixelOffsetMode.HighQuality;
		graphics2.DrawImage(bitmap, 0, 0, size, size);
		return bitmap2;
	}

	private static void Draw(Graphics g, string key, Color line, Color acc)
	{
		using Pen pen = new Pen(line, 1.3f)
		{
			StartCap = LineCap.Round,
			EndCap = LineCap.Round,
			LineJoin = LineJoin.Round
		};
		using Pen pen2 = new Pen(acc, 1.6f)
		{
			StartCap = LineCap.Round,
			EndCap = LineCap.Round,
			LineJoin = LineJoin.Round
		};
		using SolidBrush brush = new SolidBrush(acc);
		using SolidBrush brush2 = new SolidBrush(line);
		Action<float, float, float, float> L = delegate(float a, float b, float c, float d)
		{
			g.DrawLine(pen, a, b, c, d);
		};
		Action<float, float, float, float> A = delegate(float a, float b, float c, float d)
		{
			g.DrawLine(pen2, a, b, c, d);
		};
		Action<float, float, float, float> R = delegate(float a, float b, float c, float d)
		{
			g.DrawRectangle(pen, a, b, c, d);
		};
		Action<float, float, float, float> E = delegate(float a, float b, float c, float d)
		{
			g.DrawEllipse(pen, a, b, c, d);
		};
		Action Page = delegate
		{
			g.DrawLines(pen, new PointF[6]
			{
				new PointF(3f, 1.5f),
				new PointF(10f, 1.5f),
				new PointF(13f, 4.5f),
				new PointF(13f, 14.5f),
				new PointF(3f, 14.5f),
				new PointF(3f, 1.5f)
			});
		};
		switch (key)
		{
		case "New":
			Page();
			A(8f, 7f, 8f, 12f);
			A(5.5f, 9.5f, 10.5f, 9.5f);
			break;
		case "Open":
			g.DrawLines(pen, new PointF[6]
			{
				new PointF(1.5f, 4f),
				new PointF(6f, 4f),
				new PointF(7.5f, 5.5f),
				new PointF(13f, 5.5f),
				new PointF(13f, 13f),
				new PointF(1.5f, 13f)
			});
			g.DrawLine(pen2, 1.5f, 13f, 4f, 8f);
			g.DrawLine(pen2, 4f, 8f, 15f, 8f);
			break;
		case "Save":
			R(2.5f, 2.5f, 11f, 11f);
			R(5f, 2.5f, 5f, 3.5f);
			R(4.5f, 8.5f, 7f, 5f);
			break;
		case "SaveAs":
			R(2.5f, 2.5f, 11f, 11f);
			R(5f, 2.5f, 5f, 3.5f);
			A(10f, 12f, 15f, 12f);
			A(12.5f, 9.5f, 12.5f, 14.5f);
			break;
		case "Close":
			A(4f, 4f, 12f, 12f);
			A(12f, 4f, 4f, 12f);
			break;
		case "CloseAll":
			R(2.5f, 2.5f, 8f, 8f);
			A(8f, 8f, 14f, 14f);
			A(14f, 8f, 8f, 14f);
			break;
		case "Info":
		case "About":
			E(2f, 2f, 12f, 12f);
			A(8f, 7f, 8f, 11.5f);
			g.FillEllipse(brush, 7.2f, 4.2f, 1.6f, 1.6f);
			break;
		case "Export":
			R(2.5f, 7.5f, 11f, 6f);
			A(8f, 10f, 8f, 1.5f);
			A(5.5f, 4f, 8f, 1.5f);
			A(10.5f, 4f, 8f, 1.5f);
			break;
		case "Pdf":
		case "Svg":
		case "Dxf":
		case "Png":
		case "Csv":
			Page();
			if (key == "Png")
			{
				A(5f, 12f, 8f, 8f);
				A(8f, 8f, 10f, 11f);
				A(10f, 11f, 11.5f, 9.5f);
			}
			else if (key == "Csv")
			{
				L(5f, 8f, 11f, 8f);
				L(5f, 11f, 11f, 11f);
				L(8f, 6f, 8f, 13f);
			}
			else if (key == "Svg")
			{
				g.DrawBezier(pen2, 5f, 12f, 6f, 6f, 10f, 14f, 11f, 8f);
			}
			else if (key == "Dxf")
			{
				A(5f, 12f, 8f, 7f);
				A(8f, 7f, 11f, 12f);
				A(5f, 12f, 11f, 12f);
			}
			else
			{
				A(5.5f, 8f, 10.5f, 8f);
				A(5.5f, 10.5f, 10.5f, 10.5f);
			}
			break;
		case "Exit":
			g.DrawLines(pen, new PointF[4]
			{
				new PointF(7f, 2f),
				new PointF(2.5f, 2f),
				new PointF(2.5f, 14f),
				new PointF(7f, 14f)
			});
			A(6f, 8f, 14f, 8f);
			A(11f, 5f, 14f, 8f);
			A(11f, 11f, 14f, 8f);
			break;
		case "Undo":
		case "Redo":
			if (key == "Redo")
			{
				g.TranslateTransform(16f, 0f);
				g.ScaleTransform(-1f, 1f);
			}
			g.DrawArc(pen2, 3f, 5f, 10f, 8f, 180f, 180f);
			A(1.5f, 6f, 4.5f, 3f);
			A(1.5f, 6f, 5f, 8f);
			break;
		case "Cut":
			E(2.5f, 10f, 3.5f, 3.5f);
			E(9.5f, 10f, 3.5f, 3.5f);
			L(4f, 10f, 11f, 2f);
			L(11.5f, 10f, 4.5f, 2f);
			break;
		case "Copy":
			R(2.5f, 2.5f, 7f, 8f);
			R(6.5f, 5.5f, 7f, 8f);
			break;
		case "Paste":
			R(3f, 3.5f, 10f, 10f);
			g.FillRectangle(brush, 6f, 1.5f, 4f, 3f);
			L(5.5f, 8f, 10.5f, 8f);
			L(5.5f, 10.5f, 10.5f, 10.5f);
			break;
		case "Duplicate":
			R(2.5f, 5.5f, 7f, 8f);
			R(6.5f, 2.5f, 7f, 8f);
			A(12f, 10f, 12f, 14f);
			A(10f, 12f, 14f, 12f);
			break;
		case "SelectAll":
			g.DrawRectangle(pen, 2.5f, 2.5f, 11f, 11f);
			g.FillRectangle(brush, 1.5f, 1.5f, 2.5f, 2.5f);
			g.FillRectangle(brush, 12f, 1.5f, 2.5f, 2.5f);
			g.FillRectangle(brush, 1.5f, 12f, 2.5f, 2.5f);
			g.FillRectangle(brush, 12f, 12f, 2.5f, 2.5f);
			break;
		case "Delete":
			R(4f, 5.5f, 8f, 8.5f);
			L(2.5f, 4f, 13.5f, 4f);
			L(6f, 4f, 6f, 2f);
			L(10f, 4f, 10f, 2f);
			L(6f, 2f, 10f, 2f);
			A(6.5f, 7.5f, 6.5f, 12f);
			A(9.5f, 7.5f, 9.5f, 12f);
			break;
		case "Rotate":
			g.DrawArc(pen2, 3f, 3f, 10f, 10f, 200f, 270f);
			A(11.5f, 1.5f, 13f, 5f);
			A(13f, 5f, 9f, 5f);
			break;
		case "AddPage":
		case "RenamePage":
		case "DeletePage":
			Page();
			if (key == "AddPage")
			{
				A(8f, 7f, 8f, 12f);
				A(5.5f, 9.5f, 10.5f, 9.5f);
			}
			else if (key == "DeletePage")
			{
				A(5.5f, 7f, 10.5f, 12f);
				A(10.5f, 7f, 5.5f, 12f);
			}
			else
			{
				A(5f, 11f, 11f, 7f);
				L(5f, 13f, 9f, 13f);
			}
			break;
		case "PrevPage":
			A(10f, 3f, 5f, 8f);
			A(5f, 8f, 10f, 13f);
			break;
		case "NextPage":
			A(6f, 3f, 11f, 8f);
			A(11f, 8f, 6f, 13f);
			break;
		case "ZoomIn":
		case "ZoomOut":
		case "ActualSize":
			E(2f, 2f, 8.5f, 8.5f);
			L(9f, 9f, 14f, 14f);
			A(4f, 6.2f, 8.5f, 6.2f);
			if (key == "ZoomIn")
			{
				A(6.2f, 4f, 6.2f, 8.5f);
			}
			else if (key == "ActualSize")
			{
				A(6.2f, 4f, 6.2f, 8.5f);
			}
			break;
		case "Fit":
			A(2f, 5.5f, 2f, 2f);
			A(2f, 2f, 5.5f, 2f);
			A(10.5f, 2f, 14f, 2f);
			A(14f, 2f, 14f, 5.5f);
			A(2f, 10.5f, 2f, 14f);
			A(2f, 14f, 5.5f, 14f);
			A(14f, 10.5f, 14f, 14f);
			A(14f, 14f, 10.5f, 14f);
			R(5f, 5f, 6f, 6f);
			break;
		case "Pan":
			A(8f, 1.5f, 8f, 14.5f);
			A(1.5f, 8f, 14.5f, 8f);
			A(6f, 3.5f, 8f, 1.5f);
			A(10f, 3.5f, 8f, 1.5f);
			A(6f, 12.5f, 8f, 14.5f);
			A(10f, 12.5f, 8f, 14.5f);
			A(3.5f, 6f, 1.5f, 8f);
			A(3.5f, 10f, 1.5f, 8f);
			A(12.5f, 6f, 14.5f, 8f);
			A(12.5f, 10f, 14.5f, 8f);
			break;
		case "Grid":
		{
			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					g.FillEllipse(brush2, 2.5f + (float)i * 4.5f, 2.5f + (float)j * 4.5f, 2f, 2f);
				}
			}
			break;
		}
		case "Labels":
			g.DrawLines(pen, new PointF[5]
			{
				new PointF(2f, 3f),
				new PointF(9f, 3f),
				new PointF(14f, 8f),
				new PointF(9f, 13f),
				new PointF(2f, 13f)
			});
			L(2f, 3f, 2f, 13f);
			A(5f, 8f, 9f, 8f);
			break;
		case "Dark":
			E(2f, 2f, 12f, 12f);
			g.FillPie(brush, 2f, 2f, 12f, 12f, 90f, 180f);
			break;
		case "Start":
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(4f, 2.5f),
				new PointF(13f, 8f),
				new PointF(4f, 13.5f)
			});
			break;
		case "Pause":
			g.FillRectangle(brush, 3.5f, 2.5f, 3.5f, 11f);
			g.FillRectangle(brush, 9f, 2.5f, 3.5f, 11f);
			break;
		case "Stop":
			g.FillRectangle(brush, 3f, 3f, 10f, 10f);
			break;
		case "StepBack":
			g.FillRectangle(brush2, 3f, 3f, 2f, 10f);
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(13f, 3f),
				new PointF(6f, 8f),
				new PointF(13f, 13f)
			});
			break;
		case "StepFwd":
			g.FillRectangle(brush2, 11f, 3f, 2f, 10f);
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(3f, 3f),
				new PointF(10f, 8f),
				new PointF(3f, 13f)
			});
			break;
		case "PrevEvent":
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(8f, 3f),
				new PointF(2f, 8f),
				new PointF(8f, 13f)
			});
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(14f, 3f),
				new PointF(8f, 8f),
				new PointF(14f, 13f)
			});
			break;
		case "NextEvent":
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(2f, 3f),
				new PointF(8f, 8f),
				new PointF(2f, 13f)
			});
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(8f, 3f),
				new PointF(14f, 8f),
				new PointF(8f, 13f)
			});
			break;
		case "Real":
			g.DrawArc(pen, 2f, 3f, 12f, 12f, 180f, 180f);
			A(8f, 9f, 11.5f, 5f);
			break;
		case "Record":
			g.FillEllipse(new SolidBrush(Color.FromArgb(214, 40, 40)), 3f, 3f, 10f, 10f);
			break;
		case "Speed":
			E(2f, 2f, 12f, 12f);
			A(8f, 8f, 8f, 4.5f);
			A(8f, 8f, 11f, 9.5f);
			break;
		case "Diagram":
			g.DrawLines(pen2, new PointF[6]
			{
				new PointF(2f, 12f),
				new PointF(5f, 12f),
				new PointF(5f, 4f),
				new PointF(10f, 4f),
				new PointF(10f, 12f),
				new PointF(14f, 12f)
			});
			L(2f, 14f, 14f, 14f);
			break;
		case "Grafcet":
			R(5f, 1.5f, 6f, 4f);
			L(8f, 5.5f, 8f, 7.5f);
			L(6f, 7.5f, 10f, 7.5f);
			L(8f, 7.5f, 8f, 9.5f);
			R(5f, 9.5f, 6f, 4f);
			break;
		case "Plotter":
			L(2f, 2f, 2f, 14f);
			L(2f, 14f, 14f, 14f);
			g.DrawCurve(pen2, new PointF[4]
			{
				new PointF(3f, 11f),
				new PointF(6f, 4f),
				new PointF(10f, 11f),
				new PointF(14f, 4f)
			});
			break;
		case "Check":
		case "CheckPanel":
			E(2f, 2f, 12f, 12f);
			A(5f, 8f, 7.2f, 10.5f);
			A(7.2f, 10.5f, 11f, 5.5f);
			break;
		case "Explain":
		case "ExplainPanel":
			Page();
			L(5.5f, 6f, 10.5f, 6f);
			L(5.5f, 8.5f, 10.5f, 8.5f);
			L(5.5f, 11f, 8.5f, 11f);
			break;
		case "Tutorial":
			R(2f, 3f, 5.5f, 10f);
			R(8.5f, 3f, 5.5f, 10f);
			A(3.5f, 6f, 6f, 6f);
			A(10f, 6f, 12.5f, 6f);
			break;
		case "Inspector":
			R(2f, 2f, 9f, 9f);
			E(8f, 8f, 5.5f, 5.5f);
			L(12.5f, 12.5f, 15f, 15f);
			break;
		case "Fault":
			g.DrawLines(pen2, new PointF[5]
			{
				new PointF(9.5f, 1.5f),
				new PointF(4f, 9f),
				new PointF(8f, 9f),
				new PointF(6.5f, 14.5f),
				new PointF(12f, 6.5f)
			});
			break;
		case "Hint":
			E(4f, 1.5f, 8f, 8f);
			L(6f, 11f, 10f, 11f);
			L(6.5f, 13f, 9.5f, 13f);
			break;
		case "Found":
			L(4f, 14f, 4f, 2f);
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(4f, 2.5f),
				new PointF(13f, 5f),
				new PointF(4f, 8f)
			});
			break;
		case "ShowFault":
			g.DrawBezier(pen, 1.5f, 8f, 5f, 2f, 11f, 2f, 14.5f, 8f);
			g.DrawBezier(pen, 1.5f, 8f, 5f, 14f, 11f, 14f, 14.5f, 8f);
			g.FillEllipse(brush, 6.2f, 6.2f, 3.6f, 3.6f);
			break;
		case "AddFault":
			g.DrawPolygon(pen, new PointF[3]
			{
				new PointF(8f, 2f),
				new PointF(14.5f, 13.5f),
				new PointF(1.5f, 13.5f)
			});
			A(8f, 6f, 8f, 10f);
			g.FillEllipse(brush, 7.2f, 11f, 1.6f, 1.6f);
			break;
		case "Repair":
			E(2f, 2f, 12f, 12f);
			A(8f, 4.5f, 8f, 11.5f);
			A(4.5f, 8f, 11.5f, 8f);
			break;
		case "Cutaway":
		case "CutawayAll":
			E(2f, 2f, 12f, 12f);
			A(8f, 2f, 8f, 14f);
			L(2f, 8f, 14f, 8f);
			break;
		case "Examples":
			g.DrawLines(pen, new PointF[6]
			{
				new PointF(1.5f, 4f),
				new PointF(6f, 4f),
				new PointF(7.5f, 5.5f),
				new PointF(13f, 5.5f),
				new PointF(13f, 13f),
				new PointF(1.5f, 13f)
			});
			L(1.5f, 4f, 1.5f, 13f);
			A(5f, 9f, 10f, 9f);
			break;
		case "Guide":
			R(2.5f, 1.5f, 11f, 13f);
			A(6f, 6f, 6f, 4.5f);
			A(6f, 4.5f, 9f, 4.5f);
			A(9f, 4.5f, 9f, 7f);
			g.FillEllipse(brush, 7.2f, 9.7f, 1.6f, 1.6f);
			break;
		case "Update":
			g.DrawArc(pen2, 2.5f, 2.5f, 11f, 11f, 30f, 280f);
			A(12f, 1.5f, 13.5f, 5f);
			A(13.5f, 5f, 9.5f, 4.5f);
			break;
		case "Settings":
		case "Options":
			E(4.5f, 4.5f, 7f, 7f);
			E(6.5f, 6.5f, 3f, 3f);
			for (int k = 0; k < 8; k++)
			{
				double num = (double)k * Math.PI / 4.0;
				A(8f + (float)Math.Cos(num) * 4.8f, 8f + (float)Math.Sin(num) * 4.8f, 8f + (float)Math.Cos(num) * 6.8f, 8f + (float)Math.Sin(num) * 6.8f);
			}
			break;
		case "Recent":
			E(2f, 2f, 12f, 12f);
			A(8f, 4.5f, 8f, 8f);
			A(8f, 8f, 10.5f, 10f);
			break;
		case "Collapse":
			A(4f, 10f, 8f, 6f);
			A(8f, 6f, 12f, 10f);
			break;
		case "Calculators":
		case "Sizing":
			R(3f, 1.5f, 10f, 13f);
			A(5f, 4.5f, 11f, 4.5f);
			for (int l = 0; l < 3; l++)
			{
				for (int m = 0; m < 3; m++)
				{
					g.FillEllipse(brush2, 4.6f + (float)l * 2.6f, 7.2f + (float)m * 2.3f, 1.3f, 1.3f);
				}
			}
			break;
		case "Parts":
			R(2f, 3f, 12f, 10f);
			L(2f, 6.5f, 14f, 6.5f);
			L(6f, 3f, 6f, 13f);
			break;
		case "Sweep":
			L(2f, 2f, 2f, 14f);
			L(2f, 14f, 14f, 14f);
			g.DrawCurve(pen2, new PointF[3]
			{
				new PointF(3f, 12f),
				new PointF(8f, 4f),
				new PointF(14f, 8f)
			});
			break;
		case "Generator":
			R(2f, 2f, 5f, 4f);
			R(9f, 10f, 5f, 4f);
			A(7f, 4f, 11.5f, 4f);
			A(11.5f, 4f, 11.5f, 10f);
			break;
		case "NewFault":
			g.DrawLines(pen2, new PointF[5]
			{
				new PointF(2f, 12f),
				new PointF(5f, 6f),
				new PointF(8f, 11f),
				new PointF(11f, 3f),
				new PointF(14f, 9f)
			});
			break;
		default:
			R(3f, 3f, 10f, 10f);
			break;
		}
	}
}
