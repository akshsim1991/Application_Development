using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>
/// "Whole workspace" cutaway: every component of the workspace shown from the inside (or as its symbol when it has no inside view),
/// laid out like the workspace, joined by animated tubes and wires that follow the real pressures and currents.
/// </summary>
public sealed class SystemCutawayView : Control
{
	private sealed class Panel_
	{
		public CircuitElement Element;

		public RectangleF Rect;
	}

	private const float PanelW = 250f;

	private const float PanelH = 168f;

	private const float Gap = 56f;

	private Circuit _circuit;

	private readonly List<Panel_> _panels = new List<Panel_>();

	private double _phase;

	private double _flow;

	private readonly Dictionary<DirectionalValve, float> _shifts = new Dictionary<DirectionalValve, float>();

	private int _layoutCount = -1;

	public bool Simulating { get; set; }

	public Circuit Circuit
	{
		get
		{
			return _circuit;
		}
		set
		{
			if (value != _circuit)
			{
				_circuit = value;
				_layoutCount = -1;
			}
		}
	}

	public SystemCutawayView()
	{
		SetStyle(ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, value: true);
	}

	public void Animate(double dt)
	{
		_phase = (_phase + 120.0 * dt) % 360.0;
		_flow = (_flow + 60.0 * dt) % 1000.0;
		foreach (Panel_ panel in _panels)
		{
			if (panel.Element is DirectionalValve directionalValve)
			{
				float num = CutawayView.TargetShift(directionalValve);
				_shifts.TryGetValue(directionalValve, out var value);
				float num2 = (float)(260.0 * dt);
				_shifts[directionalValve] = ((Math.Abs(num - value) <= num2) ? num : (value + (float)Math.Sign(num - value) * num2));
			}
		}
		Invalidate();
	}

	/// <summary>Places each element in a grid cell near its workspace position, so the layout resembles the workspace.</summary>
	private void Layout()
	{
		_panels.Clear();
		if (_circuit == null)
		{
			return;
		}
		List<CircuitElement> list = _circuit.Elements.Where((CircuitElement e) => !(e is TextNote) && !(e is Junction) && !(e is PageConnector)).OrderBy((CircuitElement e) => e.Y).ThenBy((CircuitElement e) => e.X).ToList();
		_layoutCount = _circuit.Elements.Count;
		if (list.Count == 0)
		{
			return;
		}
		float num = list.Min((CircuitElement e) => e.X);
		float num2 = list.Min((CircuitElement e) => e.Y);
		float num3 = list.Max((CircuitElement e) => e.X) - num;
		float num4 = list.Max((CircuitElement e) => e.Y) - num2;
		float num5 = (float)Math.Max(1.0, Math.Round(Math.Sqrt(list.Count * 1.6)));
		float num6 = Math.Max(num3 / Math.Max(1f, num5 - 1f), 120f);
		float num7 = Math.Max(num4 / Math.Max(1f, (float)Math.Ceiling((double)list.Count / (double)num5) - 1f), 100f);
		HashSet<(int, int)> hashSet = new HashSet<(int, int)>();
		Dictionary<CircuitElement, (int, int)> dictionary = new Dictionary<CircuitElement, (int, int)>();
		foreach (CircuitElement item in list)
		{
			int num8 = (int)Math.Round((item.X - num) / num6);
			int num9 = (int)Math.Round((item.Y - num2) / num7);
			(int, int) tuple = FreeCell(hashSet, num8, num9);
			hashSet.Add(tuple);
			dictionary[item] = tuple;
		}
		int num10 = dictionary.Values.Min(((int, int) v) => v.Item1);
		int num11 = dictionary.Values.Min(((int, int) v) => v.Item2);
		foreach (CircuitElement item2 in list)
		{
			(int, int) tuple2 = dictionary[item2];
			_panels.Add(new Panel_
			{
				Element = item2,
				Rect = new RectangleF((float)(tuple2.Item1 - num10) * (250f + Gap), (float)(tuple2.Item2 - num11) * (168f + Gap), 250f, 168f)
			});
		}
	}

	private static (int, int) FreeCell(HashSet<(int, int)> used, int c, int r)
	{
		if (!used.Contains((c, r)))
		{
			return (c, r);
		}
		for (int i = 1; i < 60; i++)
		{
			for (int j = -i; j <= i; j++)
			{
				for (int k = -i; k <= i; k++)
				{
					if ((Math.Abs(j) == i || Math.Abs(k) == i) && !used.Contains((c + j, r + k)))
					{
						return (c + j, r + k);
					}
				}
			}
		}
		return (c, r);
	}

	protected override void OnPaint(PaintEventArgs e)
	{
		Graphics graphics = e.Graphics;
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		CutawayPalette cutawayPalette = CutawayPalette.Current();
		graphics.Clear(cutawayPalette.Back);
		if (_circuit == null || _circuit.Elements.Count == 0)
		{
			using Font font = new Font("Segoe UI", 9.5f);
			using SolidBrush brush = new SolidBrush(cutawayPalette.SubText);
			graphics.DrawString("The workspace is empty. Place and connect some components, then start the simulation to watch them work together.", font, brush, 14f, 14f);
			return;
		}
		if (_layoutCount != _circuit.Elements.Count)
		{
			Layout();
		}
		if (_panels.Count == 0)
		{
			return;
		}
		float num = _panels.Max((Panel_ p) => p.Rect.Right);
		float num2 = _panels.Max((Panel_ p) => p.Rect.Bottom);
		float num3 = 14f;
		float num4 = Math.Min(1.4f, Math.Min(((float)base.Width - 2f * num3) / num, ((float)base.Height - 2f * num3 - 18f) / num2));
		num4 = Math.Max(0.12f, num4);
		graphics.TranslateTransform(((float)base.Width - num * num4) / 2f, num3);
		graphics.ScaleTransform(num4, num4);
		Dictionary<CircuitElement, Panel_> map = _panels.ToDictionary((Panel_ p) => p.Element);
		foreach (Tube tube in _circuit.Tubes.ToList())
		{
			try
			{
				DrawTube(graphics, tube, map, cutawayPalette);
			}
			catch (Exception)
			{
			}
		}
		using Font font2 = new Font("Segoe UI", 9f, FontStyle.Bold);
		using Font font3 = new Font("Segoe UI", 8f);
		foreach (Panel_ panel in _panels.ToList())
		{
			try
			{
				DrawPanel(graphics, panel, cutawayPalette, font2, font3);
			}
			catch (Exception)
			{
			}
		}
	}

	private static PointF Anchor(RectangleF r, PointF toward, out PointF dir)
	{
		float num = toward.X - (r.X + r.Width / 2f);
		float num2 = toward.Y - (r.Y + r.Height / 2f);
		if (Math.Abs(num) * r.Height > Math.Abs(num2) * r.Width)
		{
			dir = new PointF(Math.Sign(num), 0f);
			return new PointF((num > 0f) ? r.Right : r.Left, r.Y + r.Height / 2f);
		}
		dir = new PointF(0f, Math.Sign(num2));
		return new PointF(r.X + r.Width / 2f, (num2 > 0f) ? r.Bottom : r.Top);
	}

	private void DrawTube(Graphics g, Tube t, Dictionary<CircuitElement, Panel_> map, CutawayPalette pal)
	{
		if (!map.TryGetValue(t.A.Owner, out var value) || !map.TryGetValue(t.B.Owner, out var value2))
		{
			return;
		}
		PointF toward = new PointF(value2.Rect.X + value2.Rect.Width / 2f, value2.Rect.Y + value2.Rect.Height / 2f);
		PointF toward2 = new PointF(value.Rect.X + value.Rect.Width / 2f, value.Rect.Y + value.Rect.Height / 2f);
		PointF dir;
		PointF pointF = Anchor(value.Rect, toward, out dir);
		PointF dir2;
		PointF pointF2 = Anchor(value2.Rect, toward2, out dir2);
		PointF pointF3 = new PointF(pointF.X + dir.X * 22f, pointF.Y + dir.Y * 22f);
		PointF pointF4 = new PointF(pointF2.X + dir2.X * 22f, pointF2.Y + dir2.Y * 22f);
		PointF[] points = ((Math.Abs(dir.X) > 0f) ? new PointF[6]
		{
			pointF,
			pointF3,
			new PointF((pointF3.X + pointF4.X) / 2f, pointF3.Y),
			new PointF((pointF3.X + pointF4.X) / 2f, pointF4.Y),
			pointF4,
			pointF2
		} : new PointF[6]
		{
			pointF,
			pointF3,
			new PointF(pointF3.X, (pointF3.Y + pointF4.Y) / 2f),
			new PointF(pointF4.X, (pointF3.Y + pointF4.Y) / 2f),
			pointF4,
			pointF2
		});
		bool flag = Simulating && (t.A.IsPressurized || t.B.IsPressurized);
		Color color = (flag ? Cutaways.Fluid(t.A.IsPressurized ? t.A : t.B, pal) : pal.Vent);
		using (Pen pen = new Pen(Color.FromArgb(flag ? 90 : 255, color), 7f)
		{
			LineJoin = LineJoin.Round
		})
		{
			g.DrawLines(pen, points);
		}
		if (flag)
		{
			using Pen pen2 = new Pen(Color.White, 2.2f)
			{
				DashStyle = DashStyle.Custom,
				DashPattern = new float[2] { 1.5f, 5f },
				DashOffset = -(float)(_flow / 3.0),
				LineJoin = LineJoin.Round
			};
			g.DrawLines(pen2, points);
		}
	}

	private void DrawPanel(Graphics g, Panel_ p, CutawayPalette pal, Font bold, Font small)
	{
		using (SolidBrush brush = new SolidBrush(pal.Cavity))
		{
			g.FillRectangle(brush, p.Rect);
		}
		using (Pen pen = new Pen(pal.Edge, 1.2f))
		{
			g.DrawRectangle(pen, p.Rect.X, p.Rect.Y, p.Rect.Width, p.Rect.Height);
		}
		CircuitElement element = p.Element;
		using (SolidBrush brush2 = new SolidBrush(pal.Text))
		{
			g.DrawString((string.IsNullOrEmpty(element.Label) ? "" : (element.Label + "  ")) + element.DisplayName, bold, brush2, p.Rect.X + 6f, p.Rect.Y + 3f, new StringFormat
			{
				Trimming = StringTrimming.EllipsisCharacter,
				FormatFlags = StringFormatFlags.NoWrap
			});
		}
		RectangleF area = new RectangleF(p.Rect.X + 6f, p.Rect.Y + 24f, p.Rect.Width - 12f, p.Rect.Height - 24f - 40f);
		bool flag = false;
		if (element is DirectionalValve directionalValve)
		{
			try
			{
				_shifts.TryGetValue(directionalValve, out var value);
				float num = directionalValve.BoxSize * 3.2f + 120f;
				float num2 = Math.Max(num + 140f, 320f);
				float num3 = Math.Min(area.Width / num2, area.Height / 215f);
				GraphicsState state = g.Save();
				g.TranslateTransform(area.X, area.Y);
				g.ScaleTransform(num3, num3);
				using (Font font = new Font("Segoe UI", 8.5f))
				{
					using Font font2 = new Font("Segoe UI", 9f, FontStyle.Bold);
					CutawayView.DrawValveCutaway(g, directionalValve, pal, font, font2, num2, value);
				}
				g.Restore(state);
				flag = true;
			}
			catch (Exception)
			{
			}
		}
		else if (Cutaways.Supports(element))
		{
			try
			{
				string text2 = Cutaways.Draw(g, element, area, pal, _phase, Simulating);
				flag = text2 == null || text2.IndexOf("nothing inside", StringComparison.OrdinalIgnoreCase) < 0;
			}
			catch (Exception)
			{
			}
		}
		if (!flag)
		{
			DrawSymbolFallback(g, element, area, pal);
		}
		string text = null;
		try
		{
			text = string.Join("   ", element.InspectValues().Take(3).Select(((string Name, string Value) v) => v.Name + ": " + v.Value));
		}
		catch (Exception)
		{
		}
		if (!string.IsNullOrEmpty(text) && Simulating)
		{
			using SolidBrush brush3 = new SolidBrush(pal.SubText);
			g.DrawString(text, small, brush3, new RectangleF(p.Rect.X + 6f, p.Rect.Bottom - 36f, p.Rect.Width - 12f, 34f));
		}
	}

	private void DrawSymbolFallback(Graphics g, CircuitElement e, RectangleF area, CutawayPalette pal)
	{
		GraphicsState state = g.Save();
		try
		{
			RectangleF localBounds = e.LocalBounds;
			float num = Math.Min(2.2f, Math.Min(area.Width / localBounds.Width, area.Height / localBounds.Height));
			g.TranslateTransform(area.X + area.Width / 2f, area.Y + area.Height / 2f);
			g.ScaleTransform(num, num);
			g.TranslateTransform(0f - (localBounds.Left + localBounds.Width / 2f), 0f - (localBounds.Top + localBounds.Height / 2f));
			using RenderContext renderContext = new RenderContext
			{
				Simulating = Simulating
			};
			using GdiSurface g2 = new GdiSurface(g, AppSettings.DarkMode ? ColorScheme.Dark : ColorScheme.Light);
			e.DrawSymbol(g2, renderContext);
		}
		catch (Exception)
		{
		}
		finally
		{
			g.Restore(state);
		}
	}
}
