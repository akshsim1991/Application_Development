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
