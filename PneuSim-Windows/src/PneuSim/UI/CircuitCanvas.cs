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
			_zoom = Math.Max(0.3f, Math.Min(3f, value));
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
		base.AutoScrollMinSize = checked(new Size((int)Math.Round(num * _zoom), (int)Math.Round(num2 * _zoom)));
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

	public void RotateSelection()
	{
		if (Simulating || _selection.Count == 0)
		{
			return;
		}
		foreach (CircuitElement item in _selection)
		{
			RectangleF rectangleF = item.WorldBounds();
			item.Rotation = checked(item.Rotation + 1) % 4;
			RectangleF rectangleF2 = item.WorldBounds();
			item.X = Snap(item.X + (rectangleF.X + rectangleF.Width / 2f) - (rectangleF2.X + rectangleF2.Width / 2f));
			item.Y = Snap(item.Y + (rectangleF.Y + rectangleF.Height / 2f) - (rectangleF2.Y + rectangleF2.Height / 2f));
		}
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
		UpdateScrollSize();
		SetSelection(new CircuitElement[1] { circuitElement });
		CircuitModified?.Invoke(this, EventArgs.Empty);
	}

	public void ZoomToFit()
	{
		RectangleF rectangleF = _circuit.Bounds();
		if (!rectangleF.IsEmpty && base.ClientSize.Width >= 50 && base.ClientSize.Height >= 50)
		{
			rectangleF.Inflate(30f, 30f);
			Zoom = Math.Min(1f, Math.Min((float)base.ClientSize.Width / rectangleF.Width, (float)base.ClientSize.Height / rectangleF.Height));
			ScrollToCircuit();
		}
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
		return (float)(Math.Round(v / 10f) * 10.0);
	}

	private PointF ToWorld(Point p)
	{
		return checked(new PointF((float)(p.X - base.AutoScrollPosition.X) / _zoom, (float)(p.Y - base.AutoScrollPosition.Y) / _zoom));
	}

	protected override void OnPaint(PaintEventArgs e)
	{
		Graphics graphics = e.Graphics;
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		graphics.TextRenderingHint = TextRenderingHint.ClearTypeGridFit;
		graphics.ResetClip();
		graphics.Clear(Scheme.Background);
		graphics.TranslateTransform(base.AutoScrollPosition.X, base.AutoScrollPosition.Y);
		graphics.ScaleTransform(_zoom, _zoom);
		using (GdiSurface g = new GdiSurface(graphics, Scheme))
		{
			if (!Simulating)
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
		foreach (Tube tube in _circuit.Tubes)
		{
			PointF[] array = tube.Route();
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

	/// <summary>Elements above their rated pressure; drawn with a red frame while simulating.</summary>
	public List<CircuitElement> AlarmElements { get; set; }

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
			g.DrawString(_hoverPort.Name, font, flag ? Brushes.DarkGreen : Brushes.Red, pointF2.X + 6f, pointF2.Y - 16f);
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

	protected override void OnMouseDown(MouseEventArgs e)
	{
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
			base.AutoScrollPosition = checked(new Point((int)Math.Round(Math.Max(0f, pointF.X * _zoom - (float)e.X)), (int)Math.Round(Math.Max(0f, pointF.Y * _zoom - (float)e.Y))));
			Invalidate();
		}
		else
		{
			base.OnMouseWheel(e);
			Invalidate();
		}
	}

	protected override void OnScroll(ScrollEventArgs se)
	{
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

	protected override void OnKeyDown(KeyEventArgs e)
	{
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
