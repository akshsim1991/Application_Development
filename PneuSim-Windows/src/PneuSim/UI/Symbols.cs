using System;
using System.Collections.Generic;
using System.Drawing;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Symbols
{
	public static void EnergyTriangle(DrawSurface g, RenderContext r, PointF[] pts, bool hydraulic, bool active)
	{
		Color color = ((!active) ? Color.Black : (hydraulic ? RenderContext.HydraulicColor : RenderContext.PressureColor));
		if (hydraulic)
		{
			using (SolidBrush brush = new SolidBrush(color))
			{
				g.FillPolygon(brush, pts);
				return;
			}
		}
		g.FillPolygon(r.BodyBrush, pts);
		using Pen pen = new Pen(color, 1.4f);
		g.DrawPolygon(pen, pts);
	}

	public static void Arrow(DrawSurface g, Pen pen, PointF p1, PointF p2, float head = 6f)
	{
		g.DrawLine(pen, p1, p2);
		float num = p2.X - p1.X;
		float num2 = p2.Y - p1.Y;
		float num3 = (float)Math.Sqrt(num * num + num2 * num2);
		if (!(num3 < 0.01f))
		{
			num /= num3;
			num2 /= num3;
			float num4 = p2.X - num * head;
			float num5 = p2.Y - num2 * head;
			float num6 = head * 0.45f;
			using SolidBrush brush = new SolidBrush(pen.Color);
			g.FillPolygon(brush, new PointF[3]
			{
				p2,
				new PointF(num4 - num2 * num6, num5 + num * num6),
				new PointF(num4 + num2 * num6, num5 - num * num6)
			});
		}
	}

	public static void Blocked(DrawSurface g, Pen pen, PointF edge, PointF inward)
	{
		PointF b = new PointF(edge.X + inward.X * 9f, edge.Y + inward.Y * 9f);
		g.DrawLine(pen, edge, b);
		float num = (0f - inward.Y) * 5f;
		float num2 = inward.X * 5f;
		g.DrawLine(pen, b.X - num, b.Y - num2, b.X + num, b.Y + num2);
	}

	public static void Spring(DrawSurface g, Pen pen, float x1, float x2, float cy, float amp)
	{
		List<PointF> list = new List<PointF>
		{
			new PointF(x1, cy)
		};
		int num = 1;
		do
		{
			float x3 = x1 + (x2 - x1) * ((float)num - 0.5f) / 6f;
			list.Add(new PointF(x3, cy + ((num % 2 == 0) ? amp : (0f - amp))));
			num = checked(num + 1);
		}
		while (num <= 6);
		list.Add(new PointF(x2, cy));
		g.DrawLines(pen, list.ToArray());
	}

	public static void Exhaust(DrawSurface g, Pen pen, PointF pt, PointF dir)
	{
		PointF pointF = new PointF(pt.X + dir.X * 9f, pt.Y + dir.Y * 9f);
		float num = (0f - dir.Y) * 5f;
		float num2 = dir.X * 5f;
		g.DrawPolygon(pen, new PointF[3]
		{
			pointF,
			new PointF(pt.X + num + dir.X, pt.Y + num2 + dir.Y),
			new PointF(pt.X - num + dir.X, pt.Y - num2 + dir.Y)
		});
	}

	public static PointF Lerp(PointF a, PointF b, float t)
	{
		return new PointF(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t);
	}
}
