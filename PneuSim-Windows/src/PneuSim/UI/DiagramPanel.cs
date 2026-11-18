using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows.Forms;

namespace PneuSim;

public class DiagramPanel : Control
{
	private const int LabelWidth = 90;

	private const int MaxRowHeight = 44;

	private const int MinRowHeight = 24;

	private const double TimeWindow = 10.0;

	public Simulator Simulator { get; set; }

	public ColorScheme Scheme { get; set; }

	public DiagramPanel()
	{
		Scheme = ColorScheme.Light;
		SetStyle(ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, value: true);
		BackColor = Color.White;
	}

	protected override void OnPaint(PaintEventArgs e)
	{
		e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;
		e.Graphics.Clear(Scheme.Background);
		using GdiSurface g = new GdiSurface(e.Graphics, Scheme);
		PaintDiagram(g);
	}

	private void PaintDiagram(DrawSurface g)
	{
		using Font font = new Font("Segoe UI", 9f, FontStyle.Bold);
		using Font font2 = new Font("Segoe UI", 8f);
		g.DrawString("Displacement-step diagram", font, Brushes.Black, 6f, 4f);
		List<CircuitElement> list = DiagramRows();
		if (Simulator == null || list.Count == 0)
		{
			g.DrawString("Start the simulation to record cylinder movements and valve states.", font2, Brushes.Gray, 6f, 26f);
			return;
		}
		int num = Math.Max(24, Math.Min(44, checked(base.Height - 46) / list.Count));
		int num2 = 90;
		checked
		{
			int num3 = base.Width - 16;
			if (num3 - num2 < 40)
			{
				return;
			}
			double num4 = Math.Max(10.0, Simulator.Time);
			double num5 = num4 - 10.0;
			int num6 = 26 + list.Count * num;
			using (Pen pen = new Pen(Color.FromArgb(225, 228, 235)))
			{
				double num7 = Math.Ceiling(num5);
				double num8 = Math.Floor(num4);
				for (double num9 = num7; num9 <= num8; num9 += 1.0)
				{
					float num10 = (float)((double)num2 + (num9 - num5) / 10.0 * (double)(num3 - num2));
					g.DrawLine(pen, num10, 24f, num10, num6);
					g.DrawString($"{num9:0} s", font2, Brushes.Gray, num10 - 8f, num6 + 2);
				}
			}
			int num11 = 26;
			foreach (CircuitElement item in list)
			{
				bool flag = item is CylinderBase;
				int num12 = num11 + 6;
				int num13 = num11 + num - 6;
				g.DrawString(item.Label, font, Brushes.Black, 6f, num11 + 3);
				if (num >= 36)
				{
					g.DrawString(flag ? "cylinder" : "valve", font2, Brushes.Gray, 6f, num11 + 19);
				}
				g.DrawString(flag ? "1" : "a", font2, Brushes.Gray, num2 - 14, num12 - 7);
				g.DrawString(flag ? "0" : "b", font2, Brushes.Gray, num2 - 14, num13 - 7);
				g.DrawLine(Pens.LightGray, num2, num13, num3, num13);
				g.DrawLine(Pens.LightGray, num2, num12, num3, num12);
				List<PointF> value = null;
				if (Simulator.History.TryGetValue(item, out value) && value.Count > 1)
				{
					List<PointF> list2 = new List<PointF>();
					foreach (PointF item2 in value)
					{
						if (!((double)item2.X < num5 - 0.1))
						{
							float num14 = (float)((double)num2 + ((double)item2.X - num5) / 10.0 * (double)(num3 - num2));
							float y = (float)num13 - item2.Y * (float)(num13 - num12);
							if (!flag && list2.Count > 0)
							{
								list2.Add(new PointF(num14, list2[list2.Count - 1].Y));
							}
							list2.Add(new PointF(Math.Max(num2, num14), y));
						}
					}
					if (list2.Count > 1)
					{
						using Pen pen2 = new Pen(flag ? RenderContext.PressureColor : Color.FromArgb(200, 90, 20), 2f);
						g.DrawLines(pen2, list2.ToArray());
					}
				}
				num11 += num;
			}
		}
	}

	private List<CircuitElement> DiagramRows()
	{
		List<CircuitElement> list = new List<CircuitElement>();
		if (Simulator == null)
		{
			return list;
		}
		List<CircuitElement> elements = Simulator.Circuit.Elements;
		list.AddRange(elements.OfType<CylinderBase>());
		list.AddRange(from v in elements.OfType<DirectionalValve>()
			where !string.IsNullOrEmpty(v.Label)
			select v);
		return list;
	}
}
