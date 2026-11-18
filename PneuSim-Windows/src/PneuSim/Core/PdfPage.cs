using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;

namespace PneuSim;

public class PdfPage
{
	public float Width { get; }

	public float Height { get; }

	internal StringBuilder Content { get; }

	internal List<PdfImage> Images { get; }

	/// <summary>The same drawing as drawing operations, so the page can be previewed and printed without a PDF viewer. Arguments: graphics, scale (pixels per point).</summary>
	public List<Action<System.Drawing.Graphics, float>> Ops { get; } = new List<Action<System.Drawing.Graphics, float>>();

	public PdfPage(float width, float height)
	{
		Content = new StringBuilder();
		Images = new List<PdfImage>();
		Width = width;
		Height = height;
	}

	public static bool FitsStandardFont(string t)
	{
		return VectorExport.Plain(t).All([SpecialName] (char ch) => ch < '\u0080' || (ch >= '\u00a0' && ch < 'Ā'));
	}

	private void TextAsImage(float x, float baseline, float size, string txt, bool bold, Color color)
	{
		checked
		{
			using Font font = new Font("Arial", size * 4f, bold ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Pixel);
			using Bitmap image = new Bitmap(1, 1);
			using Graphics graphics = Graphics.FromImage(image);
			SizeF sizeF = graphics.MeasureString(txt, font);
			int num = Math.Max(1, (int)Math.Ceiling(sizeF.Width));
			int num2 = Math.Max(1, (int)Math.Ceiling(sizeF.Height));
			float num3 = font.Size * (float)font.FontFamily.GetCellAscent(font.Style) / (float)font.FontFamily.GetEmHeight(font.Style);
			using (Bitmap bitmap = new Bitmap(num, num2, PixelFormat.Format24bppRgb))
			{
				using (Graphics graphics2 = Graphics.FromImage(bitmap))
				{
					using SolidBrush brush = new SolidBrush(color);
					graphics2.Clear(Color.White);
					graphics2.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;
					graphics2.DrawString(txt, font, brush, 0f, 0f);
				}
				byte[] array = new byte[num * num2 * 3 - 1 + 1];
				int num4 = num2 - 1;
				for (int i = 0; i <= num4; i++)
				{
					int num5 = num - 1;
					for (int j = 0; j <= num5; j++)
					{
						Color pixel = bitmap.GetPixel(j, i);
						int num6 = (i * num + j) * 3;
						array[num6] = pixel.R;
						array[num6 + 1] = pixel.G;
						array[num6 + 2] = pixel.B;
					}
				}
				Images.Add(new PdfImage
				{
					PixelWidth = num,
					PixelHeight = num2,
					Data = PdfImage.Zlib(array)
				});
			}
			float v = (float)num / 4f;
			float num7 = (float)num2 / 4f;
			float num8 = baseline - num3 / 4f;
			Content.AppendLine($"q {PdfDocument.F(v)} 0 0 {PdfDocument.F(num7)} {PdfDocument.F(x)} {PdfDocument.F(Y(num8 + num7))} cm /Im{Images.Count - 1} Do Q");
		}
	}

	private float Y(float v)
	{
		return Height - v;
	}

	private static string Rgb(Color c)
	{
		return $"{PdfDocument.F((float)(int)c.R / 255f)} {PdfDocument.F((float)(int)c.G / 255f)} {PdfDocument.F((float)(int)c.B / 255f)}";
	}

	public void Path(PointF[] points, bool closed, Color stroke, float strokeWidth, bool dashed, Color fill)
	{
		if (points.Length >= 2 && (fill.A != 0 || stroke.A != 0))
		{
			PointF[] pts = (PointF[])points.Clone();
			Ops.Add(delegate(System.Drawing.Graphics g, float s)
			{
				PointF[] array = Array.ConvertAll(pts, (PointF p) => new PointF(p.X * s, p.Y * s));
				if (fill.A > 0 && array.Length >= 3)
				{
					using SolidBrush brush = new SolidBrush(fill);
					g.FillPolygon(brush, array);
				}
				if (stroke.A > 0)
				{
					using Pen pen = new Pen(stroke, Math.Max(0.3f, strokeWidth) * s);
					if (dashed)
					{
						pen.DashStyle = System.Drawing.Drawing2D.DashStyle.Dash;
					}
					if (closed && array.Length >= 3)
					{
						g.DrawPolygon(pen, array);
					}
					else
					{
						g.DrawLines(pen, array);
					}
				}
			});
		}
		checked
		{
			if (points.Length >= 2 && (fill.A != 0 || stroke.A != 0))
			{
				StringBuilder content = Content;
				if (stroke.A > 0)
				{
					content.Append($"{Rgb(stroke)} RG {PdfDocument.F(Math.Max(0.3f, strokeWidth))} w ");
					content.Append(dashed ? "[3 2] 0 d " : "[] 0 d ");
				}
				if (fill.A > 0)
				{
					content.Append($"{Rgb(fill)} rg ");
				}
				content.Append($"{PdfDocument.F(points[0].X)} {PdfDocument.F(Y(points[0].Y))} m ");
				int num = points.Length - 1;
				for (int i = 1; i <= num; i++)
				{
					content.Append($"{PdfDocument.F(points[i].X)} {PdfDocument.F(Y(points[i].Y))} l ");
				}
				if (closed)
				{
					content.Append("h ");
				}
				if (fill.A > 0 && stroke.A > 0)
				{
					content.AppendLine("B");
				}
				else if (fill.A > 0)
				{
					content.AppendLine("f");
				}
				else
				{
					content.AppendLine("S");
				}
			}
		}
	}

	public void Rect(float x, float top, float w, float h, Color stroke, float strokeWidth = 1f, Color fill = default(Color))
	{
		Path(new PointF[4]
		{
			new PointF(x, top),
			new PointF(x + w, top),
			new PointF(x + w, top + h),
			new PointF(x, top + h)
		}, closed: true, stroke, strokeWidth, dashed: false, fill);
	}

	public void Text(float x, float baseline, float size, string txt, bool bold = false, Color color = default(Color), float angleDeg = 0f)
	{
		if (!string.IsNullOrEmpty(txt))
		{
			Color col = ((color.A == 0) ? Color.Black : color);
			string text = VectorExport.Plain(txt);
			Ops.Add(delegate(System.Drawing.Graphics g, float s)
			{
				using Font font = new Font("Arial", Math.Max(1f, size * s), bold ? FontStyle.Bold : FontStyle.Regular, GraphicsUnit.Pixel);
				using SolidBrush brush = new SolidBrush(col);
				System.Drawing.Drawing2D.GraphicsState gstate = g.Save();
				g.TranslateTransform(x * s, baseline * s);
				if (Math.Abs(angleDeg) > 0.1f)
				{
					g.RotateTransform(0f - angleDeg);
				}
				g.DrawString(text, font, brush, 0f, (0f - size) * s * 0.88f);
				g.Restore(gstate);
			});
		}
		if (!string.IsNullOrEmpty(txt))
		{
			if (color.A == 0)
			{
				color = Color.Black;
			}
			if (!FitsStandardFont(txt) && (double)Math.Abs(angleDeg) < 0.1)
			{
				TextAsImage(x, baseline, size, txt, bold, color);
				return;
			}
			double num = (double)(0f - angleDeg) * Math.PI / 180.0;
			float v = (float)Math.Cos(num);
			float num2 = (float)Math.Sin(num);
			Content.AppendLine(string.Format("BT /{0} {1} Tf {2} rg ", bold ? "F2" : "F1", PdfDocument.F(size), Rgb(color)) + $"{PdfDocument.F(v)} {PdfDocument.F(num2)} {PdfDocument.F(0f - num2)} {PdfDocument.F(v)} {PdfDocument.F(x)} {PdfDocument.F(Y(baseline))} Tm ({Escape(txt)}) Tj ET");
		}
	}

	private static string Escape(string t)
	{
		StringBuilder stringBuilder = new StringBuilder();
		string text = VectorExport.Plain(t);
		foreach (char c in text)
		{
			if (c == '(' || c == ')' || c == '\\')
			{
				stringBuilder.Append('\\').Append(c);
			}
			else
			{
				stringBuilder.Append((c < 'Ā') ? c : '?');
			}
		}
		return stringBuilder.ToString();
	}
}
