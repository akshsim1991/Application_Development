using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Security;
using System.Text;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class VectorExport
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__5_002D0
	{
		public StringBuilder _0024VB_0024Local_sb;

		public _Closure_0024__5_002D0(_Closure_0024__5_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_sb = arg0._0024VB_0024Local_sb;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0(int code, string value)
		{
			_0024VB_0024Local_sb.Append(code).Append("\r\n").Append(value)
				.Append("\r\n");
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__6_002D0
	{
		public float _0024VB_0024Local_ox;

		public float _0024VB_0024Local_scale;

		public float _0024VB_0024Local_oy;

		public _Closure_0024__6_002D0(_Closure_0024__6_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_ox = arg0._0024VB_0024Local_ox;
				_0024VB_0024Local_scale = arg0._0024VB_0024Local_scale;
				_0024VB_0024Local_oy = arg0._0024VB_0024Local_oy;
			}
		}

		[SpecialName]
		internal PointF _Lambda_0024__0(PointF p)
		{
			return new PointF(_0024VB_0024Local_ox + p.X * _0024VB_0024Local_scale, _0024VB_0024Local_oy + p.Y * _0024VB_0024Local_scale);
		}
	}

	public static string Plain(string t)
	{
		return t.Replace("₹", "Rs ").Replace("→", "->").Replace("←", "<-")
			.Replace("≥", ">=")
			.Replace("≤", "<=")
			.Replace("•", "-")
			.Replace("—", "-")
			.Replace("–", "-")
			.Replace("≈", "~")
			.Replace("…", "...")
			.Replace("✔", "OK")
			.Replace("✘", "X")
			.Replace("₂", "2")
			.Replace("−", "-")
			.Replace("÷", "/")
			.Replace("’", "'")
			.Replace("‘", "'")
			.Replace('“', '"')
			.Replace('”', '"');
	}

	public static VectorSurface Record(Circuit c, bool simulating = false)
	{
		VectorSurface vectorSurface = new VectorSurface();
		using CircuitCanvas circuitCanvas = new CircuitCanvas
		{
			Circuit = c,
			Simulating = simulating,
			ShowColumns = true
		};
		circuitCanvas.PaintTo(vectorSurface);
		return vectorSurface;
	}

	private static string Hex(Color c)
	{
		return $"#{c.R:X2}{c.G:X2}{c.B:X2}";
	}

	private static string N(float v)
	{
		return v.ToString("0.##", CultureInfo.InvariantCulture);
	}

	public static void SaveSvg(Circuit c, string path)
	{
		VectorSurface vectorSurface = Record(c);
		RectangleF rectangleF = vectorSurface.Bounds();
		rectangleF.Inflate(20f, 20f);
		StringBuilder stringBuilder = new StringBuilder();
		stringBuilder.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
		stringBuilder.AppendLine($"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"{N(rectangleF.Width)}\" height=\"{N(rectangleF.Height)}\" viewBox=\"{N(rectangleF.Left)} {N(rectangleF.Top)} {N(rectangleF.Width)} {N(rectangleF.Height)}\">");
		stringBuilder.AppendLine($"<rect x=\"{N(rectangleF.Left)}\" y=\"{N(rectangleF.Top)}\" width=\"{N(rectangleF.Width)}\" height=\"{N(rectangleF.Height)}\" fill=\"white\"/>");
		foreach (VectorShape shape in vectorSurface.Shapes)
		{
			if (shape.IsText)
			{
				string text = SecurityElement.Escape(shape.Text);
				string text2 = (((double)Math.Abs(shape.AngleDeg) > 0.1) ? $" transform=\"rotate({N(shape.AngleDeg)} {N(shape.TextPos.X)} {N(shape.TextPos.Y)})\"" : "");
				stringBuilder.AppendLine(string.Format("<text x=\"{0}\" y=\"{1}\" font-family=\"Segoe UI, Arial, sans-serif\" font-size=\"{2}\"{3} fill=\"{4}\"{5}>{6}</text>", N(shape.TextPos.X), N(shape.TextPos.Y), N(shape.FontSize), shape.Bold ? " font-weight=\"bold\"" : "", Hex(shape.Stroke), text2, text));
			}
			else
			{
				string text3 = string.Join(" ", shape.Points.Select([SpecialName] (PointF p) => $"{N(p.X)},{N(p.Y)}"));
				string text4 = ((shape.Fill.A > 0) ? Hex(shape.Fill) : "none");
				string text5 = ((shape.Stroke.A > 0) ? string.Format(" stroke=\"{0}\" stroke-width=\"{1}\"{2} stroke-linejoin=\"round\"", Hex(shape.Stroke), N(shape.StrokeWidth), shape.Dashed ? " stroke-dasharray=\"4 3\"" : "") : "");
				stringBuilder.AppendLine(string.Format("<{0} points=\"{1}\" fill=\"{2}\"{3}/>", shape.Closed ? "polygon" : "polyline", text3, text4, text5));
			}
		}
		stringBuilder.AppendLine("</svg>");
		File.WriteAllText(path, stringBuilder.ToString(), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
	}

	public static void SaveDxf(Circuit c, string path)
	{
		_Closure_0024__5_002D0 arg = default(_Closure_0024__5_002D0);
		_Closure_0024__5_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__5_002D0(arg);
		VectorSurface vectorSurface = Record(c);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_sb = new StringBuilder();
		VB_0024AnonymousDelegate_4<int, string> vB_0024AnonymousDelegate_ = [SpecialName] (int code, string value) =>
		{
			CS_0024_003C_003E8__locals0._0024VB_0024Local_sb.Append(code).Append("\r\n").Append(value)
				.Append("\r\n");
		};
		VB_0024AnonymousDelegate_5<float, string> vB_0024AnonymousDelegate_2 = [SpecialName] (float v) => v.ToString("0.###", CultureInfo.InvariantCulture);
		vB_0024AnonymousDelegate_(0, "SECTION");
		vB_0024AnonymousDelegate_(2, "HEADER");
		vB_0024AnonymousDelegate_(9, "$ACADVER");
		vB_0024AnonymousDelegate_(1, "AC1009");
		vB_0024AnonymousDelegate_(9, "$DWGCODEPAGE");
		vB_0024AnonymousDelegate_(3, "ANSI_1252");
		vB_0024AnonymousDelegate_(9, "$INSUNITS");
		vB_0024AnonymousDelegate_(70, "4");
		vB_0024AnonymousDelegate_(0, "ENDSEC");
		vB_0024AnonymousDelegate_(0, "SECTION");
		vB_0024AnonymousDelegate_(2, "ENTITIES");
		checked
		{
			foreach (VectorShape shape in vectorSurface.Shapes)
			{
				string value2 = (shape.IsText ? "TEXT" : ((shape.Fill.A > 0 && shape.Stroke.A == 0) ? "FILL" : "LINES"));
				if (shape.IsText)
				{
					vB_0024AnonymousDelegate_(0, "TEXT");
					vB_0024AnonymousDelegate_(8, value2);
					vB_0024AnonymousDelegate_(10, vB_0024AnonymousDelegate_2(shape.TextPos.X * 0.25f));
					vB_0024AnonymousDelegate_(20, vB_0024AnonymousDelegate_2((0f - shape.TextPos.Y) * 0.25f));
					vB_0024AnonymousDelegate_(30, "0");
					vB_0024AnonymousDelegate_(40, vB_0024AnonymousDelegate_2(shape.FontSize * 0.72f * 0.25f));
					vB_0024AnonymousDelegate_(1, Plain(shape.Text));
					if ((double)Math.Abs(shape.AngleDeg) > 0.1)
					{
						vB_0024AnonymousDelegate_(50, vB_0024AnonymousDelegate_2(0f - shape.AngleDeg));
					}
					continue;
				}
				List<PointF> list = shape.Points.ToList();
				if (shape.Closed)
				{
					list.Add(list[0]);
				}
				int num = list.Count - 2;
				for (int i = 0; i <= num; i++)
				{
					vB_0024AnonymousDelegate_(0, "LINE");
					vB_0024AnonymousDelegate_(8, value2);
					vB_0024AnonymousDelegate_(10, vB_0024AnonymousDelegate_2(list[i].X * 0.25f));
					vB_0024AnonymousDelegate_(20, vB_0024AnonymousDelegate_2((0f - list[i].Y) * 0.25f));
					vB_0024AnonymousDelegate_(30, "0");
					vB_0024AnonymousDelegate_(11, vB_0024AnonymousDelegate_2(list[i + 1].X * 0.25f));
					vB_0024AnonymousDelegate_(21, vB_0024AnonymousDelegate_2((0f - list[i + 1].Y) * 0.25f));
					vB_0024AnonymousDelegate_(31, "0");
				}
			}
			vB_0024AnonymousDelegate_(0, "ENDSEC");
			vB_0024AnonymousDelegate_(0, "EOF");
			File.WriteAllText(path, CS_0024_003C_003E8__locals0._0024VB_0024Local_sb.ToString(), Encoding.GetEncoding(28591));
		}
	}

	public static void DrawShapes(PdfPage page, VectorSurface vs, RectangleF area)
	{
		_Closure_0024__6_002D0 arg = default(_Closure_0024__6_002D0);
		_Closure_0024__6_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__6_002D0(arg);
		RectangleF rectangleF = vs.Bounds();
		if (rectangleF.IsEmpty)
		{
			return;
		}
		rectangleF.Inflate(10f, 10f);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_scale = Math.Min(1f, Math.Min(area.Width / rectangleF.Width, area.Height / rectangleF.Height));
		CS_0024_003C_003E8__locals0._0024VB_0024Local_ox = area.Left + (area.Width - rectangleF.Width * CS_0024_003C_003E8__locals0._0024VB_0024Local_scale) / 2f - rectangleF.Left * CS_0024_003C_003E8__locals0._0024VB_0024Local_scale;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_oy = area.Top - rectangleF.Top * CS_0024_003C_003E8__locals0._0024VB_0024Local_scale;
		VB_0024AnonymousDelegate_6<PointF, PointF> vB_0024AnonymousDelegate_ = [SpecialName] (PointF p) => new PointF(CS_0024_003C_003E8__locals0._0024VB_0024Local_ox + p.X * CS_0024_003C_003E8__locals0._0024VB_0024Local_scale, CS_0024_003C_003E8__locals0._0024VB_0024Local_oy + p.Y * CS_0024_003C_003E8__locals0._0024VB_0024Local_scale);
		foreach (VectorShape shape in vs.Shapes)
		{
			if (shape.IsText)
			{
				PointF pointF = vB_0024AnonymousDelegate_(shape.TextPos);
				page.Text(pointF.X, pointF.Y, shape.FontSize * CS_0024_003C_003E8__locals0._0024VB_0024Local_scale * 0.75f, shape.Text, shape.Bold, shape.Stroke, shape.AngleDeg);
			}
			else
			{
				PointF[] points = shape.Points;
				VB_0024AnonymousDelegate_6<PointF, PointF> vB_0024AnonymousDelegate_2 = vB_0024AnonymousDelegate_;
				page.Path(points.Select((vB_0024AnonymousDelegate_2 == null) ? null : new Func<PointF, PointF>(vB_0024AnonymousDelegate_2.Invoke)).ToArray(), shape.Closed, shape.Stroke, shape.StrokeWidth * CS_0024_003C_003E8__locals0._0024VB_0024Local_scale * 0.75f, shape.Dashed, shape.Fill);
			}
		}
	}

	public static void DrawTitleBlock(PdfPage page, ProjectInfo info, string sheetName, int sheet, int sheets)
	{
		Color color = Color.FromArgb(90, 90, 90);
		page.Rect(20f, 20f, page.Width - 40f, page.Height - 40f, Color.Black, 1.2f);
		float num = 330f;
		float num2 = 74f;
		float num3 = page.Width - 20f - num;
		float num4 = page.Height - 20f - num2;
		page.Rect(num3, num4, num, num2, Color.Black, 1.2f, Color.White);
		page.Path(new PointF[2]
		{
			new PointF(num3, num4 + 26f),
			new PointF(num3 + num, num4 + 26f)
		}, closed: false, Color.Black, 0.8f, dashed: false, Color.Empty);
		page.Path(new PointF[2]
		{
			new PointF(num3, num4 + 50f),
			new PointF(num3 + num, num4 + 50f)
		}, closed: false, Color.Black, 0.8f, dashed: false, Color.Empty);
		page.Path(new PointF[2]
		{
			new PointF(num3 + 200f, num4 + 26f),
			new PointF(num3 + 200f, num4 + num2)
		}, closed: false, Color.Black, 0.8f, dashed: false, Color.Empty);
		page.Text(num3 + 4f, num4 + 9f, 6f, "TITLE", bold: false, color);
		page.Text(num3 + 4f, num4 + 21f, 11f, info.Title ?? "", bold: true);
		page.Text(num3 + 4f, num4 + 33f, 6f, "DRAWN BY", bold: false, color);
		page.Text(num3 + 4f, num4 + 45f, 9f, info.Author);
		page.Text(num3 + 204f, num4 + 33f, 6f, "DATE", bold: false, color);
		page.Text(num3 + 204f, num4 + 45f, 9f, info.DateText);
		page.Text(num3 + 4f, num4 + 57f, 6f, "COMPANY / INSTITUTE", bold: false, color);
		page.Text(num3 + 4f, num4 + 69f, 9f, info.Company);
		page.Text(num3 + 204f, num4 + 57f, 6f, "DRAWING NO. / REV / SHEET", bold: false, color);
		page.Text(num3 + 204f, num4 + 69f, 9f, $"{info.DrawingNumber}  rev {info.Revision}  {sheet}/{sheets}");
		page.Text(num3 + 120f, num4 + 9f, 6f, sheetName, bold: false, color);
		page.Text(26f, page.Height - 20f - 6f, 6f, "Drawn with PneuSim", bold: false, color);
	}
}
