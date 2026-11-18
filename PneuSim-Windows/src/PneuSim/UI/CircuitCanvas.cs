using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Text;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class CircuitCanvas : ScrollableControl
{
	public const float GridSize = 10f;

	private static readonly SizeF WorkArea = new SizeF(3000f, 2000f);

	private const string ClipboardHeader = "PneuSimClipboard:";

	private static string _internalClipboard;

	private Circuit _circuit;

	private float _zoom;

	private readonly List<CircuitElement> _selection;

	private Tube _selectedTube;

	private bool _dragging;

	private PointF _dragStart;

	private Dictionary<CircuitElement, PointF> _dragOrigins;

	private bool _dragMoved;

	private Tube _draggingTube;

	private Port _connectFrom;

	private bool _connectMoved;

	private RectangleF? _rubberBand;

	private PointF _rubberStart;

	private PointF _mouseWorld;

	private Port _hoverPort;

	private CircuitElement _pressedElement;

	private CircuitElement _hoverElement;

	[CompilerGenerated]
	private object _InspectedObject;

	private ColorScheme _scheme;

	public bool PickingTarget { get; set; }

	public object InspectedObject
	{
		[CompilerGenerated]
		get
		{
			return _InspectedObject;
		}
		[CompilerGenerated]
		set
		{
			_InspectedObject = RuntimeHelpers.GetObjectValue(value);
		}
	}

	public Tube SelectedTube => _selectedTube;

	public CircuitElement HoverElement => _hoverElement;

	public CircuitElement LastHoveredElement { get; set; }

	public Circuit Circuit
	{
		get
		{
			return _circuit;
		}
		set
		{
			_circuit = value;
			ClearSelection();
			UpdateScrollSize();
			Invalidate();
		}
	}

	public Simulator Simulator { get; set; }

	public bool Simulating { get; set; }

	public Func<IEnumerable<CircuitElement>> NameScope { get; set; }

	public LibraryPreset PlacingPreset { get; set; }

	public float Zoom
	{
		get
		{
			return _zoom;
		}
		set
		{
			if (!_fitting)
			{
				KeepFitted = false;
			}
			_zoom = Math.Max(0.1f, Math.Min(3f, value));
			UpdateScrollSize();
			Invalidate();
			ZoomChanged?.Invoke(this, EventArgs.Empty);
		}
	}

	public CircuitElement SelectedElement
	{
		get
		{
			if (_selection.Count != 1)
			{
				return null;
			}
			return _selection[0];
		}
	}

	public IReadOnlyList<CircuitElement> SelectedElements => _selection;

	public ColorScheme Scheme
	{
		get
		{
			return _scheme;
		}
		set
		{
			_scheme = value;
			BackColor = value.Background;
			Invalidate();
		}
	}

	public bool ShowColumns { get; set; }

	public bool RevealFaults { get; set; }

	public event EventHandler SelectionChanged;

	public event EventHandler CircuitModified;

	public event EventHandler ElementOperated;

	public event EventHandler<string> StatusMessage;

	public event EventHandler<CircuitElement> HoverElementChanged;

	public event EventHandler<object> InspectRequested;

	public event EventHandler<object> TargetPicked;

	public event EventHandler ZoomChanged;

	private object ObjectAt(PointF w)
	{
		CircuitElement circuitElement = _circuit.FindElementAt(w);
		if (circuitElement != null)
		{
			return circuitElement;
		}
		return _circuit.FindTubeAt(w, 5f / _zoom + 1f);
	}

	private void TrackHover(PointF w)
	{
		CircuitElement circuitElement = _circuit.FindElementAt(w);
		if (circuitElement != _hoverElement)
		{
			_hoverElement = circuitElement;
			if (circuitElement != null && Cutaways.Supports(circuitElement))
			{
				LastHoveredElement = circuitElement;
			}
			HoverElementChanged?.Invoke(this, circuitElement);
		}
	}

	public void SelectElements(IEnumerable<CircuitElement> items)
	{
		SetSelection(items.Where([SpecialName] (CircuitElement e) => _circuit.Elements.Contains(e)));
	}

	/// <summary>Scrolls the drawing so that the element is in the middle of the view (used when a check result is clicked).</summary>
	public void ScrollToElement(CircuitElement element)
	{
		if (element == null || !_circuit.Elements.Contains(element))
		{
			return;
		}
		RectangleF rectangleF = element.WorldBounds();
		int x = (int)Math.Round((rectangleF.X + rectangleF.Width / 2f) * _zoom) - base.ClientSize.Width / 2;
		int y = (int)Math.Round((rectangleF.Y + rectangleF.Height / 2f) * _zoom) - base.ClientSize.Height / 2;
		base.AutoScrollPosition = new Point(Math.Max(0, x), Math.Max(0, y));
		Invalidate();
	}

	public CircuitCanvas()
	{
		_circuit = new Circuit();
		_zoom = 1f;
		_selection = new List<CircuitElement>();
		_dragOrigins = new Dictionary<CircuitElement, PointF>();
		_scheme = ColorScheme.Light;
		SetStyle(ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.Selectable | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, value: true);
		BackColor = Color.White;
		AutoScroll = true;
		AllowDrop = true;
		UpdateScrollSize();
	}

	private IEnumerable<CircuitElement> ScopeElements()
	{
		if (NameScope != null)
		{
			return NameScope();
		}
		return _circuit.Elements;
	}

	private void UpdateScrollSize()
	{
		float num = WorkArea.Width;
		float num2 = WorkArea.Height;
		if (_circuit != null)
		{
			RectangleF rectangleF = _circuit.Bounds();
			if (!rectangleF.IsEmpty)
			{
				num = Math.Max(num, rectangleF.Right + 400f);
				num2 = Math.Max(num2, rectangleF.Bottom + 400f);
			}
		}
		base.AutoScrollMinSize = checked(new Size((int)Math.Round(num * _zoom), (int)Math.Round(num2 * _zoom) + 16));
	}

	public void ClearSelection()
	{
		_selection.Clear();
		_selectedTube = null;
		SelectionChanged?.Invoke(this, EventArgs.Empty);
		Invalidate();
	}

	private void SetSelection(IEnumerable<CircuitElement> items)
	{
		_selection.Clear();
		_selection.AddRange(items);
		_selectedTube = null;
		SelectionChanged?.Invoke(this, EventArgs.Empty);
		Invalidate();
	}

	public void SelectAll()
	{
		if (!Simulating)
		{
			SetSelection(_circuit.Elements);
		}
	}

	public void DeleteSelection()
	{
		if (Simulating)
		{
			return;
		}
		if (_selection.Count > 0)
		{
			foreach (CircuitElement item in _selection.ToList())
			{
				_circuit.Remove(item);
			}
		}
		else
		{
			if (_selectedTube == null)
			{
				return;
			}
			_circuit.RemoveTube(_selectedTube);
		}
		ClearSelection();
		CircuitModified?.Invoke(this, EventArgs.Empty);
	}

	/// <summary>If the given components lie on top of other components, moves them (together, in steps of one grid square) to the nearest free place.</summary>
	public void ResolveOverlaps(IEnumerable<CircuitElement> moved)
	{
		List<CircuitElement> mine = moved.Where((CircuitElement e) => !(e is TextNote) && !(e is Junction)).ToList();
		HashSet<CircuitElement> set = new HashSet<CircuitElement>(moved);
		List<CircuitElement> others = _circuit.Elements.Where((CircuitElement e) => !set.Contains(e) && !(e is TextNote) && !(e is Junction)).ToList();
		if (mine.Count == 0 || others.Count == 0)
		{
			return;
		}
		Func<float, float, int> overlaps = (float dx, float dy) =>
		{
			int n = 0;
			foreach (CircuitElement m in mine)
			{
				RectangleF a = m.WorldBounds();
				a.Offset(dx, dy);
				a.Inflate(-3f, -3f);
				foreach (CircuitElement o in others)
				{
					RectangleF b = o.WorldBounds();
					b.Inflate(-3f, -3f);
					if (a.Width > 0f && b.Width > 0f && a.IntersectsWith(b))
					{
						n++;
					}
				}
			}
			return n;
		};
		if (overlaps(0f, 0f) == 0)
		{
			return;
		}
		float bestDx = 0f;
		float bestDy = 0f;
		bool found = false;
		for (int ring = 1; ring <= 60 && !found; ring++)
		{
			float bestDist = float.MaxValue;
			for (int i = -ring; i <= ring; i++)
			{
				for (int j = -ring; j <= ring; j++)
				{
					if (Math.Max(Math.Abs(i), Math.Abs(j)) != ring)
					{
						continue;
					}
					float dx = i * 10f;
					float dy = j * 10f;
					if (overlaps(dx, dy) == 0 && dx * dx + dy * dy < bestDist)
					{
						bestDist = dx * dx + dy * dy;
						bestDx = dx;
						bestDy = dy;
						found = true;
					}
				}
			}
		}
		if (found)
		{
			foreach (CircuitElement m in moved)
			{
				m.X += bestDx;
				m.Y += bestDy;
			}
		}
	}

	/// <summary>Re-routes the lines of the given components so they do not overlap other components.</summary>
	public void TidyTubes(IEnumerable<CircuitElement> moved, bool force)
	{
		HashSet<CircuitElement> set = new HashSet<CircuitElement>(moved);
		foreach (Tube tube in _circuit.Tubes.ToList())
		{
			if (set.Contains(tube.A.Owner) || set.Contains(tube.B.Owner))
			{
				tube.AutoRoute(_circuit.Elements, force);
			}
		}
	}

	public void RotateSelection()
	{
		if (Simulating || _selection.Count == 0)
		{
			return;
		}
		if (_selection.Count == 1)
		{
			CircuitElement single = _selection[0];
			RectangleF before = single.WorldBounds();
			single.Rotation = checked(single.Rotation + 1) % 4;
			RectangleF after = single.WorldBounds();
			single.X = Snap(single.X + (before.X + before.Width / 2f) - (after.X + after.Width / 2f));
			single.Y = Snap(single.Y + (before.Y + before.Height / 2f) - (after.Y + after.Height / 2f));
		}
		else
		{
			RectangleF groupBox = _selection[0].WorldBounds();
			foreach (CircuitElement item in _selection)
			{
				groupBox = RectangleF.Union(groupBox, item.WorldBounds());
			}
			float gcx = (float)Math.Round((groupBox.X + groupBox.Width / 2f) / 10f) * 10f;
			float gcy = (float)Math.Round((groupBox.Y + groupBox.Height / 2f) / 10f) * 10f;
			foreach (CircuitElement item in _selection)
			{
				float nx = gcx - (item.Y - gcy);
				float ny = gcy + (item.X - gcx);
				item.X = nx;
				item.Y = ny;
				item.Rotation = checked(item.Rotation + 1) % 4;
			}
		}
		ResolveOverlaps(_selection);
		TidyTubes(_selection, force: true);
		Invalidate();
		CircuitModified?.Invoke(this, EventArgs.Empty);
	}

	public void CopySelection()
	{
		if (_selection.Count != 0)
		{
			_internalClipboard = _circuit.ExtractXml(_selection);
			try
			{
				Clipboard.SetText("PneuSimClipboard:" + _internalClipboard);
			}
			catch (Exception ex)
			{
				ProjectData.SetProjectError(ex);
				Exception ex2 = ex;
				ProjectData.ClearProjectError();
			}
			StatusMessage?.Invoke(this, $"Copied {_selection.Count} component(s).");
		}
	}

	public void CutSelection()
	{
		if (!Simulating)
		{
			CopySelection();
			DeleteSelection();
		}
	}

	public void Paste()
	{
		if (Simulating)
		{
			return;
		}
		string text = _internalClipboard;
		try
		{
			if (Clipboard.ContainsText() && Clipboard.GetText().StartsWith("PneuSimClipboard:"))
			{
				text = Clipboard.GetText().Substring("PneuSimClipboard:".Length);
			}
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			ProjectData.ClearProjectError();
		}
		if (!string.IsNullOrEmpty(text))
		{
			try
			{
				List<CircuitElement> list = _circuit.Merge(text, 40f, 40f);
				_internalClipboard = _circuit.ExtractXml(list);
				Naming.RenamePasted(list, ScopeElements());
				ResolveOverlaps(list);
				UpdateScrollSize();
				SetSelection(list);
				CircuitModified?.Invoke(this, EventArgs.Empty);
			}
			catch (Exception ex3)
			{
				ProjectData.SetProjectError(ex3);
				Exception ex4 = ex3;
				StatusMessage?.Invoke(this, "The clipboard does not contain PneuSim components.");
				ProjectData.ClearProjectError();
			}
		}
	}

	public void DuplicateSelection()
	{
		if (!Simulating && _selection.Count != 0)
		{
			List<CircuitElement> list = _circuit.Merge(_circuit.ExtractXml(_selection), 40f, 40f);
			Naming.RenamePasted(list, ScopeElements());
			SetSelection(list);
			CircuitModified?.Invoke(this, EventArgs.Empty);
		}
	}

	public void PlaceElement(LibraryPreset preset, PointF world)
	{
		CircuitElement circuitElement = preset.Factory();
		RectangleF localBounds = circuitElement.LocalBounds;
		_circuit.Add(circuitElement, Snap(Math.Max(0f - localBounds.Left, world.X - localBounds.Width / 2f - localBounds.Left)), Snap(Math.Max(0f - localBounds.Top, world.Y - localBounds.Height / 2f - localBounds.Top)));
		Naming.NameNewElement(circuitElement, ScopeElements());
		ResolveOverlaps(new CircuitElement[1] { circuitElement });
		UpdateScrollSize();
		SetSelection(new CircuitElement[1] { circuitElement });
		CircuitModified?.Invoke(this, EventArgs.Empty);
	}

	private bool _fitting;

	/// <summary>While true the view is re-fitted whenever the workspace changes size (for example when the bottom panel opens). Any zoom, scroll or pan by the user switches it off.</summary>
	public bool KeepFitted { get; set; }

	protected override void OnResize(EventArgs eventargs)
	{
		base.OnResize(eventargs);
		if (KeepFitted && !_fitting && base.ClientSize.Width >= 50 && base.ClientSize.Height >= 50)
		{
			ZoomToFit();
		}
	}

	/// <summary>Everything that is drawn: components, their labels, and every tube bend.</summary>
	private RectangleF DrawnBounds()
	{
		RectangleF result = _circuit.Bounds();
		if (result.IsEmpty)
		{
			return result;
		}
		foreach (CircuitElement element in _circuit.Elements)
		{
			PointF pointF = element.ToWorld(element.LabelAnchor);
			result = RectangleF.Union(result, new RectangleF(pointF.X - 8f, pointF.Y - 22f, 90f, 30f));
		}
		foreach (Tube tube in _circuit.Tubes)
		{
			try
			{
				PointF[] array = tube.Route();
				if (array != null)
				{
					foreach (PointF pointF2 in array)
					{
						result = RectangleF.Union(result, new RectangleF(pointF2.X - 2f, pointF2.Y - 2f, 4f, 4f));
					}
				}
			}
			catch (Exception)
			{
			}
		}
		return result;
	}

	/// <summary>Zooms and scrolls so that the whole circuit (components, labels and tubes) is visible, with a margin.</summary>
	public void ZoomToFit()
	{
		RectangleF rectangleF = DrawnBounds();
		if (rectangleF.IsEmpty || base.ClientSize.Width < 50 || base.ClientSize.Height < 50)
		{
			return;
		}
		_fitting = true;
		float num = 14f;
		float num2 = 16f;
		rectangleF.Inflate(num, num);
		float num3 = (float)(base.ClientSize.Width - SystemInformation.VerticalScrollBarWidth - 4);
		float num4 = (float)(base.ClientSize.Height - SystemInformation.HorizontalScrollBarHeight - 4) - num2;
		Zoom = Math.Max(0.1f, Math.Min(3f, Math.Min(num3 / rectangleF.Width, num4 / rectangleF.Height)));
		PerformLayout();
		float num5 = (rectangleF.Left + rectangleF.Width / 2f) * _zoom - num3 / 2f;
		float num6 = (rectangleF.Top + rectangleF.Height / 2f) * _zoom - num4 / 2f;
		base.AutoScrollPosition = new Point((int)Math.Round(Math.Max(0f, num5)), (int)Math.Round(Math.Max(0f, num6)));
		_fitting = false;
		KeepFitted = true;
		Invalidate();
	}

	public void ScrollToCircuit()
	{
		RectangleF rectangleF = _circuit.Bounds();
		if (!rectangleF.IsEmpty)
		{
			base.AutoScrollPosition = checked(new Point((int)Math.Round(Math.Max(0f, (rectangleF.Left - 40f) * _zoom)), (int)Math.Round(Math.Max(0f, (rectangleF.Top - 40f) * _zoom))));
		}
	}

	private static float Snap(float v)
	{
		if (!AppSettings.SnapToGrid)
		{
			return (float)Math.Round(v);
		}
		return (float)(Math.Round(v / 10f) * 10.0);
	}

	private PointF ToWorld(Point p)
	{
		return checked(new PointF((float)(p.X - base.AutoScrollPosition.X) / _zoom, (float)(p.Y - base.AutoScrollPosition.Y - 16) / _zoom));
	}

	protected override void OnPaint(PaintEventArgs e)
	{
		Graphics graphics = e.Graphics;
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		graphics.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;
		graphics.ResetClip();
		graphics.Clear(Scheme.Background);
		graphics.TranslateTransform(base.AutoScrollPosition.X, base.AutoScrollPosition.Y + 16);
		graphics.ScaleTransform(_zoom, _zoom);
		using (GdiSurface g = new GdiSurface(graphics, Scheme))
		{
			if (!Simulating && AppSettings.ShowGrid)
			{
				DrawGrid(g, base.ClientRectangle);
			}
			DrawContent(g, interactive: true);
		}
		graphics.ResetTransform();
		DrawColumnBand(graphics);
	}

	private void DrawColumnRuler(DrawSurface g)
	{
		RectangleF rectangleF = _circuit.Bounds();
		if (rectangleF.IsEmpty)
		{
			return;
		}
		float num = rectangleF.Top - 44f;
		checked
		{
			int num2 = Project.ColumnAt(rectangleF.Left) - 1;
			int num3 = Project.ColumnAt(rectangleF.Right) - 1;
			using Pen pen = new Pen(Color.FromArgb(150, 150, 150), 0.8f);
			using Font font = new Font("Segoe UI", 7.5f);
			using SolidBrush brush = new SolidBrush(Color.FromArgb(110, 110, 110));
			g.DrawLine(pen, (float)num2 * 100f, num + 14f, (float)(num3 + 1) * 100f, num + 14f);
			int num4 = num3 + 1;
			for (int i = num2; i <= num4; i++)
			{
				g.DrawLine(pen, (float)i * 100f, num + 8f, (float)i * 100f, num + 14f);
			}
			int num5 = num3;
			for (int j = num2; j <= num5; j++)
			{
				string text = (j + 1).ToString();
				g.DrawString(text, font, brush, (float)j * 100f + 50f - g.MeasureString(text, font).Width / 2f, num);
			}
		}
	}

	private void DrawColumnBand(Graphics g)
	{
		int num = 16;
		bool flag = _scheme == ColorScheme.Dark;
		checked
		{
			using SolidBrush brush = new SolidBrush(flag ? Color.FromArgb(44, 47, 56) : Color.FromArgb(240, 242, 246));
			using Pen pen = new Pen(flag ? Color.FromArgb(80, 85, 98) : Color.FromArgb(200, 204, 212));
			using SolidBrush brush2 = new SolidBrush(flag ? Color.FromArgb(170, 175, 185) : Color.FromArgb(110, 115, 125));
			using Font font = new Font("Segoe UI", 7.5f);
			g.FillRectangle(brush, 0, 0, base.ClientSize.Width, num);
			g.DrawLine(pen, 0, num, base.ClientSize.Width, num);
			float x = ToWorld(new Point(0, 0)).X;
			float x2 = ToWorld(new Point(base.ClientSize.Width, 0)).X;
			int num2 = Math.Max(0, (int)Math.Floor(x / 100f));
			int num3 = (int)Math.Floor(x2 / 100f);
			for (int i = num2; i <= num3; i++)
			{
				float num4 = (float)i * 100f * _zoom + (float)base.AutoScrollPosition.X;
				g.DrawLine(pen, num4, 0f, num4, num);
				string text = (i + 1).ToString();
				SizeF sizeF = g.MeasureString(text, font);
				g.DrawString(text, font, brush2, num4 + 100f * _zoom / 2f - sizeF.Width / 2f, 2f);
			}
		}
	}

	public void PaintTo(Graphics g)
	{
		g.SmoothingMode = SmoothingMode.AntiAlias;
		using GdiSurface g2 = new GdiSurface(g);
		DrawContent(g2, interactive: false);
	}

	public void PaintTo(DrawSurface s)
	{
		DrawContent(s, interactive: false);
	}

	private void DrawContent(DrawSurface g, bool interactive)
	{
		using RenderContext renderContext = new RenderContext
		{
			Simulating = Simulating,
			Interactive = interactive
		};
		if (!interactive && ShowColumns)
		{
			DrawColumnRuler(g);
		}
		DrawTubes(g, renderContext);
		using (Font font = new Font("Segoe UI", 8.5f, FontStyle.Bold))
		{
			foreach (CircuitElement element in _circuit.Elements)
			{
				object objectValue = RuntimeHelpers.GetObjectValue(g.Save());
				using (Matrix m = element.GetMatrix())
				{
					g.MultiplyTransform(m);
				}
				element.DrawSymbol(g, renderContext);
				g.Restore(RuntimeHelpers.GetObjectValue(objectValue));
				if (!string.IsNullOrEmpty(element.Label) && !(element is TextNote))
				{
					PointF pointF = element.ToWorld(element.LabelAnchor);
					g.DrawString(element.Label, font, Brushes.Black, pointF.X, pointF.Y - 15f);
				}
				if (element.Fault != 0 && (!element.FaultHidden || RevealFaults))
				{
					DrawElementFault(g, element);
				}
				if (Simulating && interactive && AppSettings.ShowValueLabels)
				{
					DrawValueLabel(g, element, font);
				}
			}
		}
		DrawPorts(g, interactive);
		if (!interactive)
		{
			return;
		}
		if (Simulating && InspectedObject != null)
		{
			DrawInspected(g);
		}
		DrawSelection(g);
		DrawAlarm(g);
		if (_connectFrom != null && _connectMoved)
		{
			using Pen pen = new Pen(Color.FromArgb(0, 140, 70), 1.5f)
			{
				DashStyle = DashStyle.Dash
			};
			PointF da = _connectFrom.WorldDir();
			g.DrawLines(pen, Tube.RouteBetween(_connectFrom.WorldPos(), da, _mouseWorld, new PointF(0f - da.X, 0f - da.Y)));
		}
		if (!_rubberBand.HasValue)
		{
			return;
		}
		RectangleF value = _rubberBand.Value;
		using SolidBrush brush = new SolidBrush(Color.FromArgb(30, 0, 120, 215));
		using Pen pen2 = new Pen(Color.FromArgb(0, 120, 215));
		g.FillRectangle(brush, value);
		g.DrawRectangle(pen2, value.X, value.Y, value.Width, value.Height);
	}

	private void DrawGrid(DrawSurface g, Rectangle clip)
	{
		PointF pointF = ToWorld(clip.Location);
		PointF pointF2 = ToWorld(new Point(clip.Right, clip.Bottom));
		float num = 10f * (float)((!(_zoom < 0.8f)) ? 1 : 2);
		using SolidBrush brush = new SolidBrush(ColorScheme.Light.GridDot);
		float num2 = (float)(Math.Floor(pointF.X / num) * (double)num);
		for (float num3 = (float)(Math.Floor(pointF.Y / num) * (double)num); num3 <= pointF2.Y; num3 += num)
		{
			for (float num4 = num2; num4 <= pointF2.X; num4 += num)
			{
				g.FillRectangle(brush, num4 - 0.6f, num3 - 0.6f, 1.2f, 1.2f);
			}
		}
	}

	private void DrawTubes(DrawSurface g, RenderContext ctx)
	{
		using Pen pen3 = new Pen(Simulating ? RenderContext.IdleTubeColor : Color.Black, 1.6f);
		using Pen pen2 = new Pen(Simulating ? Color.FromArgb(90, 90, 90) : Color.FromArgb(40, 40, 40), 1.2f);
		using Pen pen = new Pen(Color.FromArgb(120, 255, 170, 0), 7f);
		pen.LineJoin = LineJoin.Round;
		List<(Tube T, PointF[] R)> routes = _circuit.Tubes.Select((Tube t) => (t, t.Route())).ToList();
		foreach (var routeItem in routes)
		{
			Tube tube = routeItem.T;
			PointF[] array = WithHops(routeItem.R, routes.Where(((Tube T, PointF[] R) o) => o.T != tube && o.T.A != tube.A && o.T.A != tube.B && o.T.B != tube.A && o.T.B != tube.B).Select(((Tube T, PointF[] R) o) => o.R).ToList());
			if (tube == _selectedTube)
			{
				g.DrawLines(pen, array);
			}
			Pen pen4 = (tube.IsElectric ? ((Simulating && tube.IsPressurized) ? ctx.Energized : pen2) : ((tube.A.Kind == PortKind.Hydraulic) ? ((Simulating && tube.IsPressurized) ? ctx.HydraulicPressure : (Simulating ? pen2 : ctx.Line)) : ((!Simulating || !(Math.Min(tube.A.Pressure, tube.B.Pressure) < -0.1)) ? ((Simulating && tube.IsPressurized) ? ctx.Pressure : pen3) : ctx.Vacuum)));
			g.DrawLines(pen4, array);
			if (tube.Fault != 0 && (!tube.FaultHidden || RevealFaults))
			{
				DrawTubeFault(g, tube, array);
			}
		}
	}

	/// <summary>Where a horizontal line crosses a vertical line of another tube (not connected to it), the horizontal line hops over it with a small arch, as in electrical drawings.</summary>
	private static PointF[] WithHops(PointF[] r, List<PointF[]> others)
	{
		List<PointF> res = new List<PointF> { r[0] };
		for (int i = 0; i < r.Length - 1; i++)
		{
			PointF a = r[i];
			PointF b = r[i + 1];
			if (Math.Abs(a.Y - b.Y) < 0.01f && Math.Abs(a.X - b.X) > 14f)
			{
				float dir = (b.X > a.X) ? 1f : -1f;
				List<float> xs = new List<float>();
				foreach (PointF[] o in others)
				{
					for (int j = 0; j < o.Length - 1; j++)
					{
						PointF c = o[j];
						PointF d = o[j + 1];
						if (Math.Abs(c.X - d.X) < 0.01f && Math.Abs(c.Y - d.Y) > 0.5f && c.X > Math.Min(a.X, b.X) + 7f && c.X < Math.Max(a.X, b.X) - 7f && a.Y > Math.Min(c.Y, d.Y) + 3f && a.Y < Math.Max(c.Y, d.Y) - 3f)
						{
							xs.Add(c.X);
						}
					}
				}
				xs.Sort();
				if (dir < 0f)
				{
					xs.Reverse();
				}
				float last = float.NaN;
				foreach (float x in xs)
				{
					if (!float.IsNaN(last) && Math.Abs(x - last) < 12f)
					{
						continue;
					}
					last = x;
					for (int k = 0; k <= 6; k++)
					{
						double t = Math.PI * (double)k / 6.0;
						res.Add(new PointF(x - dir * 5f * (float)Math.Cos(t), a.Y - 5f * (float)Math.Sin(t)));
					}
				}
			}
			res.Add(b);
		}
		return res.ToArray();
	}

	/// <summary>Elements above their rated pressure; drawn with a red frame while simulating.</summary>
	public List<CircuitElement> AlarmElements { get; set; }

	private void DrawValueLabel(DrawSurface g, CircuitElement element, Font font)
	{
		string text = element.LiveValueText();
		if (text == null)
		{
			return;
		}
		string[] array = text.Split('\n');
		float num = 0f;
		string[] array2 = array;
		foreach (string text2 in array2)
		{
			num = Math.Max(num, g.MeasureString(text2, font).Width);
		}
		float num2 = (float)array.Length * (font.GetHeight() + 1f) + 4f;
		RectangleF rectangleF = element.WorldBounds();
		float num3 = rectangleF.Right + 6f;
		float num4 = rectangleF.Top + (rectangleF.Height - num2) / 2f;
		using SolidBrush brush = new SolidBrush(Color.FromArgb(235, 255, 250, 205));
		using Pen pen = new Pen(Color.FromArgb(150, 130, 90), 1f);
		g.FillRectangle(brush, num3, num4, num + 8f, num2);
		g.DrawRectangle(pen, num3, num4, num + 8f, num2);
		for (int j = 0; j < array.Length; j++)
		{
			g.DrawString(array[j], font, Brushes.Black, num3 + 4f, num4 + 2f + (float)j * (font.GetHeight() + 1f));
		}
	}

	private void DrawAlarm(DrawSurface g)
	{
		if (AlarmElements == null)
		{
			return;
		}
		using Pen pen = new Pen(Color.Red, 3f);
		foreach (CircuitElement alarmElement in AlarmElements)
		{
			if (_circuit.Elements.Contains(alarmElement))
			{
				RectangleF rectangleF = alarmElement.WorldBounds();
				rectangleF.Inflate(5f, 5f);
				g.DrawRectangle(pen, rectangleF.X, rectangleF.Y, rectangleF.Width, rectangleF.Height);
			}
		}
	}

	private void DrawInspected(DrawSurface g)
	{
		using Pen pen = new Pen(Color.FromArgb(0, 170, 190), 2f)
		{
			DashStyle = DashStyle.Dot
		};
		if (InspectedObject is CircuitElement circuitElement && _circuit.Elements.Contains(circuitElement))
		{
			RectangleF rectangleF = circuitElement.WorldBounds();
			rectangleF.Inflate(6f, 6f);
			g.DrawRectangle(pen, rectangleF.X, rectangleF.Y, rectangleF.Width, rectangleF.Height);
		}
		if (InspectedObject is Tube tube && _circuit.Tubes.Contains(tube))
		{
			pen.Width = 6f;
			pen.Color = Color.FromArgb(110, 0, 170, 190);
			g.DrawLines(pen, tube.Route());
		}
	}

	private void DrawTubeFault(DrawSurface g, Tube t, PointF[] pts)
	{
		int num = 0;
		float num2 = -1f;
		checked
		{
			int num3 = pts.Length - 2;
			for (int i = 0; i <= num3; i++)
			{
				float num4 = Math.Abs(pts[i + 1].X - pts[i].X) + Math.Abs(pts[i + 1].Y - pts[i].Y);
				if (num4 > num2)
				{
					num2 = num4;
					num = i;
				}
			}
			PointF pointF = Symbols.Lerp(pts[num], pts[num + 1], 0.5f);
			using (Pen pen = new Pen(RenderContext.FaultColor, 2.2f))
			{
				if (t.Fault == FaultKind.Leak)
				{
					int num5 = -1;
					do
					{
						g.DrawArc(pen, pointF.X - 4f + (float)(num5 * 5), pointF.Y - 12f, 6f, 8f, 200f, 140f);
						num5++;
					}
					while (num5 <= 1);
				}
				else
				{
					g.DrawLine(pen, pointF.X - 6f, pointF.Y - 6f, pointF.X + 6f, pointF.Y + 6f);
					g.DrawLine(pen, pointF.X + 6f, pointF.Y - 6f, pointF.X - 6f, pointF.Y + 6f);
				}
			}
			using Font font = new Font("Segoe UI", 7.5f, FontStyle.Bold);
			using SolidBrush brush = new SolidBrush(RenderContext.FaultColor);
			g.DrawString((t.Fault == FaultKind.Leak) ? "leak" : (t.IsElectric ? "broken" : "blocked"), font, brush, pointF.X + 7f, pointF.Y - 18f);
		}
	}

	private void DrawElementFault(DrawSurface g, CircuitElement el)
	{
		RectangleF rectangleF = el.WorldBounds();
		float num = rectangleF.Right - 4f;
		float num2 = rectangleF.Top - 4f;
		using (SolidBrush brush = new SolidBrush(RenderContext.FaultColor))
		{
			g.FillPolygon(brush, new PointF[3]
			{
				new PointF(num, num2 - 9f),
				new PointF(num + 9f, num2 + 7f),
				new PointF(num - 9f, num2 + 7f)
			});
		}
		using Font font = new Font("Segoe UI", 7.5f, FontStyle.Bold);
		using SolidBrush brush2 = new SolidBrush(RenderContext.FaultColor);
		g.DrawString("!", font, Brushes.White, num - 3f, num2 - 4f);
		g.DrawString(Faults.ShortName(el.Fault), font, brush2, num + 10f, num2 - 6f);
	}

	private void DrawPorts(DrawSurface g, bool interactive)
	{
		foreach (Port item in _circuit.AllPorts())
		{
			if (item.Owner is Junction)
			{
				continue;
			}
			PointF pointF = item.WorldPos();
			if (interactive && AppSettings.PortLabelMode != "Off" && item.Kind != PortKind.Electric)
			{
				PointF d = item.WorldDir();
				string pt = PortLabels.Text(item);
				using Font pf = new Font("Segoe UI", 7f, FontStyle.Bold);
				SizeF sz = g.MeasureString(pt, pf);
				float lx = pointF.X + d.X * 5f + ((d.X < 0f) ? (-sz.Width - 1f) : ((d.X > 0f) ? 1f : 4f));
				float ly = pointF.Y + d.Y * 5f + ((d.Y < 0f) ? (-sz.Height) : ((d.Y > 0f) ? 0f : (-sz.Height / 2f)));
				g.DrawString(pt, pf, Brushes.DarkBlue, lx, ly);
			}
			if (item.ConnectionCount == 0 && !Simulating && interactive)
			{
				g.FillEllipse(Brushes.White, pointF.X - 3f, pointF.Y - 3f, 6f, 6f);
				g.DrawEllipse((item.Kind == PortKind.Electric) ? Pens.Firebrick : Pens.Black, pointF.X - 3f, pointF.Y - 3f, 6f, 6f);
			}
			else if (item.ConnectionCount >= 2)
			{
				Color color = Color.Black;
				if (Simulating && item.IsPressurized)
				{
					color = ((item.Kind == PortKind.Electric) ? RenderContext.EnergizedColor : RenderContext.PressureColor);
				}
				using SolidBrush brush = new SolidBrush(color);
				g.FillEllipse(brush, pointF.X - 3f, pointF.Y - 3f, 6f, 6f);
			}
		}
		if (_hoverPort != null && !Simulating && interactive)
		{
			PointF pointF2 = _hoverPort.WorldPos();
			bool flag = _connectFrom == null || Circuit.CanConnect(_connectFrom, _hoverPort) || _hoverPort == _connectFrom;
			using (Pen pen = new Pen(flag ? Color.FromArgb(0, 160, 80) : Color.Red, 2f))
			{
				g.DrawEllipse(pen, pointF2.X - 6f, pointF2.Y - 6f, 12f, 12f);
			}
			using Font font = new Font("Segoe UI", 7.5f);
			g.DrawString(PortLabels.Letter(_hoverPort) + (PortLabels.Letter(_hoverPort) == _hoverPort.Name ? "" : " (" + _hoverPort.Name + ")"), font, flag ? Brushes.DarkGreen : Brushes.Red, pointF2.X + 6f, pointF2.Y - 16f);
		}
	}

	private void DrawSelection(DrawSurface g)
	{
		if (Simulating)
		{
			return;
		}
		using Pen pen = new Pen(Color.FromArgb(255, 140, 0), 1.2f)
		{
			DashStyle = DashStyle.Dash
		};
		foreach (CircuitElement item in _selection)
		{
			RectangleF rectangleF = item.WorldBounds();
			rectangleF.Inflate(5f, 5f);
			g.DrawRectangle(pen, rectangleF.X, rectangleF.Y, rectangleF.Width, rectangleF.Height);
		}
	}

	private bool _panning;

	private Point _panStartMouse;

	private Point _panStartScroll;

	private bool _spaceDown;

	private bool _panMode;

	/// <summary>Hand tool: dragging with the left mouse button moves the view. Middle mouse button, Space + drag and the touchpad always pan.</summary>
	public bool PanMode
	{
		get
		{
			return _panMode;
		}
		set
		{
			_panMode = value;
			Cursor = (value ? Cursors.Hand : Cursors.Default);
		}
	}

	private bool WantsPan(MouseEventArgs e)
	{
		return e.Button == MouseButtons.Middle || (e.Button == MouseButtons.Left && (_panMode || _spaceDown));
	}

	/// <summary>Moves the view by dx, dy pixels (positive = right / down).</summary>
	public void ScrollBy(int dx, int dy)
	{
		Point autoScrollPosition = base.AutoScrollPosition;
		base.AutoScrollPosition = new Point(Math.Max(0, -autoScrollPosition.X + dx), Math.Max(0, -autoScrollPosition.Y + dy));
		Invalidate();
	}

	protected override void WndProc(ref Message m)
	{
		if (m.Msg == 526)
		{
			int num = (short)(((long)m.WParam >> 16) & 0xFFFF);
			ScrollBy(num / 120 * 60, 0);
			m.Result = IntPtr.Zero;
		}
		else
		{
			base.WndProc(ref m);
		}
	}

	protected override void OnMouseDown(MouseEventArgs e)
	{
		if (WantsPan(e))
		{
			KeepFitted = false;
			Focus();
			_panning = true;
			_panStartMouse = e.Location;
			_panStartScroll = new Point(-base.AutoScrollPosition.X, -base.AutoScrollPosition.Y);
			Cursor = Cursors.SizeAll;
			base.Capture = true;
			return;
		}
		base.OnMouseDown(e);
		Focus();
		PointF pointF = (_mouseWorld = ToWorld(e.Location));
		if (PickingTarget && e.Button == MouseButtons.Left)
		{
			object objectValue = RuntimeHelpers.GetObjectValue(ObjectAt(pointF));
			if (objectValue != null)
			{
				PickingTarget = false;
				Cursor = Cursors.Default;
				TargetPicked?.Invoke(this, RuntimeHelpers.GetObjectValue(objectValue));
			}
			return;
		}
		if (Simulating)
		{
			if (e.Button == MouseButtons.Right)
			{
				object objectValue2 = RuntimeHelpers.GetObjectValue(ObjectAt(pointF));
				if (objectValue2 != null)
				{
					InspectedObject = RuntimeHelpers.GetObjectValue(objectValue2);
					InspectRequested?.Invoke(this, RuntimeHelpers.GetObjectValue(objectValue2));
					Invalidate();
				}
			}
			else if (e.Button == MouseButtons.Left)
			{
				CircuitElement circuitElement = _circuit.FindElementAt(pointF);
				if (circuitElement != null && circuitElement.IsManuallyOperated)
				{
					_pressedElement = circuitElement;
					circuitElement.OnSimMouseDown(circuitElement.ToLocal(pointF));
					ElementOperated?.Invoke(this, EventArgs.Empty);
				}
			}
			return;
		}
		if (e.Button == MouseButtons.Left && PlacingPreset != null)
		{
			PlaceElement(PlacingPreset, pointF);
			if ((Control.ModifierKeys & Keys.Shift) == 0)
			{
				PlacingPreset = null;
				StatusMessage?.Invoke(this, "Drag from a port (small circle) to another port to connect them.");
			}
			return;
		}
		bool flag = (Control.ModifierKeys & Keys.Control) != 0;
		Port port = _circuit.FindPortAt(pointF, 6f / _zoom + 2f);
		if (port != null && port.Owner is Junction && _selection.Contains(port.Owner))
		{
			port = null;
		}
		if (e.Button == MouseButtons.Left && port != null && !flag)
		{
			_connectFrom = port;
			_connectMoved = false;
			return;
		}
		CircuitElement circuitElement2 = _circuit.FindElementAt(pointF);
		if (circuitElement2 != null)
		{
			if (flag)
			{
				if (_selection.Contains(circuitElement2))
				{
					_selection.Remove(circuitElement2);
				}
				else
				{
					_selection.Add(circuitElement2);
				}
				_selectedTube = null;
				SelectionChanged?.Invoke(this, EventArgs.Empty);
				Invalidate();
				return;
			}
			if (!_selection.Contains(circuitElement2))
			{
				SetSelection(new CircuitElement[1] { circuitElement2 });
			}
			if (e.Button == MouseButtons.Left)
			{
				BeginDrag(pointF);
			}
			return;
		}
		Tube tube = _circuit.FindTubeAt(pointF, 5f / _zoom + 1f);
		if (tube != null)
		{
			_selection.Clear();
			_selectedTube = tube;
			SelectionChanged?.Invoke(this, EventArgs.Empty);
			if (e.Button == MouseButtons.Left && tube.MidAxis() != null)
			{
				_draggingTube = tube;
				_dragMoved = false;
			}
			Invalidate();
		}
		else
		{
			if (!flag)
			{
				ClearSelection();
			}
			if (e.Button == MouseButtons.Left)
			{
				_rubberStart = pointF;
				_rubberBand = new RectangleF(pointF, SizeF.Empty);
			}
		}
	}

	private void BeginDrag(PointF w)
	{
		_dragging = true;
		_dragMoved = false;
		_dragStart = w;
		_dragOrigins = _selection.ToDictionary([SpecialName] (CircuitElement el) => el, [SpecialName] (CircuitElement el) => new PointF(el.X, el.Y));
	}

	protected override void OnMouseMove(MouseEventArgs e)
	{
		if (_panning)
		{
			base.AutoScrollPosition = new Point(Math.Max(0, _panStartScroll.X - (e.X - _panStartMouse.X)), Math.Max(0, _panStartScroll.Y - (e.Y - _panStartMouse.Y)));
			Invalidate();
			return;
		}
		base.OnMouseMove(e);
		PointF pointF = (_mouseWorld = ToWorld(e.Location));
		TrackHover(pointF);
		if (Simulating)
		{
			CircuitElement circuitElement = _circuit.FindElementAt(pointF);
			Cursor = ((circuitElement != null && circuitElement.IsManuallyOperated) ? Cursors.Hand : Cursors.Default);
		}
		else if (_dragging)
		{
			float num = Snap(pointF.X - _dragStart.X);
			float num2 = Snap(pointF.Y - _dragStart.Y);
			float num3 = _dragOrigins.Keys.Min([SpecialName] (CircuitElement el) => el.WorldBounds().Left - el.X + _dragOrigins[el].X);
			float num4 = _dragOrigins.Keys.Min([SpecialName] (CircuitElement el) => el.WorldBounds().Top - el.Y + _dragOrigins[el].Y);
			if (num3 + num < 0f)
			{
				num = Snap(0f - num3 + 5f);
			}
			if (num4 + num2 < 0f)
			{
				num2 = Snap(0f - num4 + 5f);
			}
			foreach (KeyValuePair<CircuitElement, PointF> dragOrigin in _dragOrigins)
			{
				float num5 = dragOrigin.Value.X + num;
				float num6 = dragOrigin.Value.Y + num2;
				if (num5 != dragOrigin.Key.X || num6 != dragOrigin.Key.Y)
				{
					dragOrigin.Key.X = num5;
					dragOrigin.Key.Y = num6;
					_dragMoved = true;
				}
			}
			Invalidate();
		}
		else if (_draggingTube != null)
		{
			_draggingTube.Mid = Snap((Operators.CompareString(_draggingTube.MidAxis(), "X", TextCompare: false) == 0) ? pointF.X : pointF.Y);
			_dragMoved = true;
			Invalidate();
		}
		else if (_rubberBand.HasValue)
		{
			_rubberBand = RectangleF.FromLTRB(Math.Min(_rubberStart.X, pointF.X), Math.Min(_rubberStart.Y, pointF.Y), Math.Max(_rubberStart.X, pointF.X), Math.Max(_rubberStart.Y, pointF.Y));
			Invalidate();
		}
		else
		{
			Port port = _circuit.FindPortAt(pointF, 6f / _zoom + 2f);
			if (port != _hoverPort)
			{
				_hoverPort = port;
				Invalidate();
			}
			if (_connectFrom != null)
			{
				_connectMoved = true;
				Invalidate();
			}
			else if (PlacingPreset != null || port != null)
			{
				Cursor = Cursors.Cross;
			}
			else if (_circuit.FindElementAt(pointF) != null)
			{
				Cursor = Cursors.SizeAll;
			}
			else if (_circuit.FindTubeAt(pointF, 5f / _zoom + 1f) != null)
			{
				Tube tube = _circuit.FindTubeAt(pointF, 5f / _zoom + 1f);
				Cursor = ((Operators.CompareString(tube.MidAxis(), "X", TextCompare: false) == 0) ? Cursors.SizeWE : ((Operators.CompareString(tube.MidAxis(), "Y", TextCompare: false) == 0) ? Cursors.SizeNS : Cursors.Default));
			}
			else
			{
				Cursor = Cursors.Default;
			}
		}
	}

	protected override void OnMouseUp(MouseEventArgs e)
	{
		if (_panning)
		{
			_panning = false;
			base.Capture = false;
			Cursor = (_panMode ? Cursors.Hand : (_spaceDown ? Cursors.Hand : Cursors.Default));
			return;
		}
		base.OnMouseUp(e);
		PointF p = ToWorld(e.Location);
		if (Simulating)
		{
			if (_pressedElement != null)
			{
				_pressedElement.OnSimMouseUp();
				_pressedElement = null;
				ElementOperated?.Invoke(this, EventArgs.Empty);
			}
			return;
		}
		if (_connectFrom != null)
		{
			Port port = _circuit.FindPortAt(p, 8f / _zoom + 2f);
			if (port != null && port != _connectFrom)
			{
				if (port.Owner == _connectFrom.Owner)
				{
					StatusMessage?.Invoke(this, "A tube cannot connect two ports of the same component.");
				}
				else if (port.Kind != _connectFrom.Kind)
				{
					StatusMessage?.Invoke(this, "Electrical terminals can only be wired to electrical terminals, and tubes only join pneumatic ports.");
				}
				else if (_circuit.Connect(_connectFrom, port) != null)
				{
					StatusMessage?.Invoke(this, $"Connected {Describe(_connectFrom)} to {Describe(port)}.");
					CircuitModified?.Invoke(this, EventArgs.Empty);
				}
			}
			else if (!_connectMoved)
			{
				SetSelection(new CircuitElement[1] { _connectFrom.Owner });
			}
			else
			{
				StatusMessage?.Invoke(this, "");
			}
			_connectFrom = null;
			Invalidate();
		}
		if (_dragging)
		{
			_dragging = false;
			if (_dragMoved)
			{
				ResolveOverlaps(_selection);
				TidyTubes(_selection, force: false);
				UpdateScrollSize();
				CircuitModified?.Invoke(this, EventArgs.Empty);
			}
		}
		if (_draggingTube != null)
		{
			_draggingTube = null;
			if (_dragMoved)
			{
				CircuitModified?.Invoke(this, EventArgs.Empty);
			}
		}
		if (!_rubberBand.HasValue)
		{
			return;
		}
		RectangleF value = _rubberBand.Value;
		_rubberBand = null;
		if (value.Width > 2f || value.Height > 2f)
		{
			IEnumerable<CircuitElement> enumerable = _circuit.Elements.Where([SpecialName] (CircuitElement el) => value.IntersectsWith(el.WorldBounds()));
			bool flag = (Control.ModifierKeys & Keys.Control) != 0;
			SetSelection(flag ? _selection.Union(enumerable).ToList() : enumerable.ToList());
		}
		Invalidate();
	}

	private static string Describe(Port p)
	{
		string arg = (string.IsNullOrEmpty(p.Owner.Label) ? p.Owner.DisplayName : p.Owner.Label);
		return $"{arg} port {p.Name}";
	}

	protected override void OnMouseLeave(EventArgs e)
	{
		base.OnMouseLeave(e);
		if (_hoverElement != null)
		{
			_hoverElement = null;
			HoverElementChanged?.Invoke(this, null);
		}
	}

	protected override void OnMouseWheel(MouseEventArgs e)
	{
		if ((Control.ModifierKeys & Keys.Control) != 0)
		{
			PointF pointF = ToWorld(e.Location);
			Zoom *= ((e.Delta > 0) ? 1.1f : 0.9090909f);
			base.AutoScrollPosition = checked(new Point((int)Math.Round(Math.Max(0f, pointF.X * _zoom - (float)e.X)), (int)Math.Round(Math.Max(0f, pointF.Y * _zoom - (float)(e.Y - 16)))));
			Invalidate();
		}
		else if ((Control.ModifierKeys & Keys.Shift) != 0)
		{
			ScrollBy(-e.Delta / 2, 0);
		}
		else
		{
			base.OnMouseWheel(e);
			Invalidate();
		}
	}

	protected override void OnScroll(ScrollEventArgs se)
	{
		if (!_fitting)
		{
			KeepFitted = false;
		}
		base.OnScroll(se);
		Invalidate();
	}

	protected override bool IsInputKey(Keys keyData)
	{
		Keys keys = keyData & Keys.KeyCode;
		if (keys == Keys.Escape || (uint)(keys - 37) <= 3u || keys == Keys.Delete)
		{
			return true;
		}
		return base.IsInputKey(keyData);
	}

	protected override void OnKeyUp(KeyEventArgs e)
	{
		base.OnKeyUp(e);
		if (e.KeyCode == Keys.Space)
		{
			_spaceDown = false;
			Cursor = (_panMode ? Cursors.Hand : Cursors.Default);
		}
	}

	protected override void OnKeyDown(KeyEventArgs e)
	{
		if (e.KeyCode == Keys.Space && !e.Control)
		{
			_spaceDown = true;
			Cursor = Cursors.Hand;
			e.Handled = true;
			return;
		}
		base.OnKeyDown(e);
		if (e.Control)
		{
			switch (e.KeyCode)
			{
			case Keys.C:
				CopySelection();
				break;
			case Keys.X:
				CutSelection();
				break;
			case Keys.V:
				Paste();
				break;
			case Keys.D:
				DuplicateSelection();
				break;
			case Keys.A:
				SelectAll();
				break;
			}
			return;
		}
		switch (e.KeyCode)
		{
		case Keys.Back:
		case Keys.Delete:
			DeleteSelection();
			break;
		case Keys.R:
			if (e.Modifiers == Keys.None)
			{
				RotateSelection();
			}
			break;
		case Keys.Left:
		case Keys.Up:
		case Keys.Right:
		case Keys.Down:
			NudgeSelection(e.KeyCode);
			break;
		case Keys.Escape:
			PickingTarget = false;
			Cursor = Cursors.Default;
			PlacingPreset = null;
			_connectFrom = null;
			StatusMessage?.Invoke(this, "");
			ClearSelection();
			break;
		}
	}

	private void NudgeSelection(Keys key)
	{
		if (Simulating || _selection.Count == 0)
		{
			return;
		}
		float num = key switch
		{
			Keys.Right => 10f, 
			Keys.Left => -10f, 
			_ => 0f, 
		};
		float num2 = key switch
		{
			Keys.Down => 10f, 
			Keys.Up => -10f, 
			_ => 0f, 
		};
		if (_selection.Min([SpecialName] (CircuitElement el) => el.WorldBounds().Left) + num < 0f || _selection.Min([SpecialName] (CircuitElement el) => el.WorldBounds().Top) + num2 < 0f)
		{
			return;
		}
		foreach (CircuitElement item in _selection)
		{
			item.X += num;
			CircuitElement circuitElement = item;
			item.Y = circuitElement.Y + num2;
		}
		Invalidate();
		CircuitModified?.Invoke(this, EventArgs.Empty);
	}

	protected override void OnDragEnter(DragEventArgs e)
	{
		base.OnDragEnter(e);
		e.Effect = ((!Simulating && e.Data.GetDataPresent(typeof(LibraryPreset))) ? DragDropEffects.Copy : DragDropEffects.None);
	}

	protected override void OnDragDrop(DragEventArgs e)
	{
		base.OnDragDrop(e);
		if (e.Data.GetData(typeof(LibraryPreset)) is LibraryPreset preset && !Simulating)
		{
			PlaceElement(preset, ToWorld(PointToClient(new Point(e.X, e.Y))));
		}
	}
}
