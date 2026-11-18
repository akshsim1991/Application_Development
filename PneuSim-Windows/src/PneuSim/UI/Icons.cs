using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Icons
{
	private static Bitmap Draw(Action<Graphics> paint)
	{
		Bitmap bitmap = new Bitmap(16, 16);
		using Graphics graphics = Graphics.FromImage(bitmap);
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		paint(graphics);
		return bitmap;
	}

	public static Image NewFile()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.FillPolygon(Brushes.White, new Point[5]
			{
				new Point(3, 1),
				new Point(10, 1),
				new Point(13, 4),
				new Point(13, 15),
				new Point(3, 15)
			});
			g.DrawPolygon(Pens.DimGray, new Point[5]
			{
				new Point(3, 1),
				new Point(10, 1),
				new Point(13, 4),
				new Point(13, 15),
				new Point(3, 15)
			});
		});
	}

	public static Image Open()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.FillRectangle(Brushes.Goldenrod, 1, 4, 14, 10);
			g.FillRectangle(Brushes.Gold, 1, 6, 14, 8);
			g.DrawRectangle(Pens.DarkGoldenrod, 1, 4, 14, 10);
		});
	}

	public static Image Save()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.FillRectangle(Brushes.SteelBlue, 1, 1, 14, 14);
			g.FillRectangle(Brushes.White, 4, 2, 8, 5);
			g.FillRectangle(Brushes.LightGray, 4, 10, 8, 5);
		});
	}

	public static Image Play()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.FillPolygon(Brushes.ForestGreen, new Point[3]
			{
				new Point(3, 1),
				new Point(14, 8),
				new Point(3, 15)
			});
		});
	}

	public static Image StepIcon(bool forward, bool toEvent)
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			if (!forward)
			{
				g.TranslateTransform(16f, 0f);
				g.ScaleTransform(-1f, 1f);
			}
			if (toEvent)
			{
				g.FillPolygon(Brushes.SteelBlue, new Point[3]
				{
					new Point(1, 2),
					new Point(8, 8),
					new Point(1, 14)
				});
				g.FillPolygon(Brushes.SteelBlue, new Point[3]
				{
					new Point(7, 2),
					new Point(14, 8),
					new Point(7, 14)
				});
			}
			else
			{
				g.FillPolygon(Brushes.SteelBlue, new Point[3]
				{
					new Point(3, 2),
					new Point(10, 8),
					new Point(3, 14)
				});
				g.FillRectangle(Brushes.SteelBlue, 11, 2, 3, 12);
			}
		});
	}

	public static Image Pause()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.FillRectangle(Brushes.DarkOrange, 3, 2, 4, 12);
			g.FillRectangle(Brushes.DarkOrange, 9, 2, 4, 12);
		});
	}

	public static Image StopIcon()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.FillRectangle(Brushes.Firebrick, 2, 2, 12, 12);
		});
	}

	public static Image RotateIcon()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			using (Pen pen = new Pen(Color.DimGray, 2f))
			{
				g.DrawArc(pen, 2, 2, 12, 12, 30, 280);
			}
			g.FillPolygon(Brushes.DimGray, new Point[3]
			{
				new Point(10, 0),
				new Point(15, 4),
				new Point(9, 6)
			});
		});
	}

	public static Image DeleteIcon()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			using Pen pen = new Pen(Color.Firebrick, 2.5f);
			g.DrawLine(pen, 3, 3, 13, 13);
			g.DrawLine(pen, 13, 3, 3, 13);
		});
	}

	public static Image UndoIcon(bool redo)
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.SmoothingMode = SmoothingMode.AntiAlias;
			if (redo)
			{
				g.TranslateTransform(16f, 0f);
				g.ScaleTransform(-1f, 1f);
			}
			Color color = Color.FromArgb(0, 118, 132);
			using Pen pen = new Pen(color, 2.2f)
			{
				StartCap = LineCap.Round,
				EndCap = LineCap.Round,
				LineJoin = LineJoin.Round
			};
			g.DrawLines(pen, new PointF[4]
			{
				new PointF(7f, 6.5f),
				new PointF(11f, 6.5f),
				new PointF(13.5f, 9f),
				new PointF(13.5f, 12.5f)
			});
			using SolidBrush brush = new SolidBrush(color);
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(1.5f, 6.5f),
				new PointF(7.5f, 2f),
				new PointF(7.5f, 11f)
			});
		});
	}

	public static Image Wand()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			using (Pen pen = new Pen(Color.DimGray, 2.5f))
			{
				g.DrawLine(pen, 2, 14, 10, 6);
			}
			g.FillPolygon(Brushes.Gold, new Point[8]
			{
				new Point(12, 0),
				new Point(13, 3),
				new Point(16, 4),
				new Point(13, 5),
				new Point(12, 8),
				new Point(11, 5),
				new Point(8, 4),
				new Point(11, 3)
			});
		});
	}

	public static Image ZoomIn()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			Magnifier(g, plus: true);
		});
	}

	public static Image PanIcon()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.SmoothingMode = SmoothingMode.AntiAlias;
			using Pen pen = new Pen(Color.FromArgb(0, 118, 132), 1.8f)
			{
				StartCap = LineCap.Round,
				EndCap = LineCap.Round
			};
			g.DrawLine(pen, 8, 2, 8, 14);
			g.DrawLine(pen, 2, 8, 14, 8);
			g.DrawLines(pen, new PointF[3]
			{
				new PointF(6f, 4f),
				new PointF(8f, 2f),
				new PointF(10f, 4f)
			});
			g.DrawLines(pen, new PointF[3]
			{
				new PointF(6f, 12f),
				new PointF(8f, 14f),
				new PointF(10f, 12f)
			});
			g.DrawLines(pen, new PointF[3]
			{
				new PointF(4f, 6f),
				new PointF(2f, 8f),
				new PointF(4f, 10f)
			});
			g.DrawLines(pen, new PointF[3]
			{
				new PointF(12f, 6f),
				new PointF(14f, 8f),
				new PointF(12f, 10f)
			});
		});
	}

	public static Image ZoomOut()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			Magnifier(g, plus: false);
		});
	}

	private static void Magnifier(Graphics g, bool plus)
	{
		using (Pen pen = new Pen(Color.DimGray, 1.6f))
		{
			g.DrawEllipse(pen, 1, 1, 10, 10);
			g.DrawLine(pen, 4, 6, 8, 6);
			if (plus)
			{
				g.DrawLine(pen, 6, 4, 6, 8);
			}
		}
		using Pen pen2 = new Pen(Color.DimGray, 3f);
		g.DrawLine(pen2, 10, 10, 14, 14);
	}

	public static Image CheckIcon()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.FillEllipse(Brushes.ForestGreen, 1, 1, 14, 14);
			using Pen pen = new Pen(Color.White, 2f);
			g.DrawLines(pen, new Point[3]
			{
				new Point(4, 8),
				new Point(7, 11),
				new Point(12, 4)
			});
		});
	}

	public static Image RecordIcon()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.DrawEllipse(Pens.DimGray, 1, 1, 14, 14);
			g.FillEllipse(Brushes.Red, 4, 4, 8, 8);
		});
	}

	public static Image GaugeIcon()
	{
		return Draw([SpecialName] (Graphics g) =>
		{
			g.DrawEllipse(Pens.DimGray, 1, 1, 14, 14);
			using Pen pen = new Pen(Color.Firebrick, 2f);
			g.DrawLine(pen, 8, 8, 12, 4);
		});
	}

	public static Icon AppIcon()
	{
		using Bitmap bitmap = new Bitmap(32, 32);
		using (Graphics graphics = Graphics.FromImage(bitmap))
		{
			graphics.SmoothingMode = SmoothingMode.AntiAlias;
			graphics.FillEllipse(new SolidBrush(RenderContext.PressureColor), 1, 1, 30, 30);
			graphics.FillRectangle(Brushes.White, 6, 11, 16, 10);
			graphics.FillRectangle(Brushes.White, 22, 14, 6, 4);
			graphics.FillRectangle(new SolidBrush(RenderContext.PressureColor), 13, 12, 3, 8);
		}
		return Icon.FromHandle(bitmap.GetHicon());
	}
}
