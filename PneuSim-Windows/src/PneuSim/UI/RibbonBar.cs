using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>
/// PneuSim's own ribbon: tabs, groups of large symbol buttons, a "More" drop-down for long groups and a
/// search box. Everything is custom-painted (no third-party library). Click or drag a symbol onto the drawing.
/// </summary>
public sealed class RibbonBar : Control
{
	public sealed class Item
	{
		public string Label;

		public LibraryPreset Preset;

		public Action Command;

		public Bitmap Icon;
	}

	public sealed class Group
	{
		public string Title;

		public List<Item> Items = new List<Item>();
	}

	public sealed class Tab
	{
		public string Name;

		public List<Group> Groups = new List<Group>();
	}

	private sealed class Hit
	{
		public Rectangle Bounds;

		public Item Item;

		public Group MoreOf;
	}

	private const int TabHeight = 30;

	private const int ContentHeight = 112;

	private const int ButtonWidth = 76;

	private const int MaxPerGroup = 5;

	private readonly List<Tab> _tabs = new List<Tab>();

	private readonly List<Hit> _hits = new List<Hit>();

	private readonly List<Rectangle> _tabRects = new List<Rectangle>();

	private readonly TextBox _search = new TextBox();

	private readonly ToolTip _tip = new ToolTip { InitialDelay = 500 };

	private readonly ListBox _results = new ListBox { IntegralHeight = true, BorderStyle = BorderStyle.FixedSingle, Visible = false };

	private readonly List<Item> _allItems = new List<Item>();

	private int _active;

	private int _scroll;

	private int _contentWidth;

	private Hit _hover;

	private Hit _down;

	private Point _downPoint;

	private bool _dark;

	public event Action<LibraryPreset> PresetChosen;

	/// <summary>Tab names, in order.</summary>
	public IEnumerable<string> TabNames => _tabs.Select((Tab t) => t.Name);

	public RibbonBar()
	{
		SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.UserPaint, value: true);
		Dock = DockStyle.Top;
		Height = TabHeight + ContentHeight + 2;
		Font = new Font("Segoe UI", 9f);
		_search.Width = 210;
		_search.BorderStyle = BorderStyle.FixedSingle;
		base.Controls.Add(_search);
		_search.TextChanged += delegate
		{
			UpdateResults();
		};
		_search.KeyDown += OnSearchKey;
		_search.LostFocus += delegate
		{
			if (!_results.Focused)
			{
				_results.Visible = false;
			}
		};
		_results.Click += delegate
		{
			PickResult();
		};
		_results.KeyDown += delegate(object s, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.Return)
			{
				PickResult();
			}
			else if (e.KeyCode == Keys.Escape)
			{
				_results.Visible = false;
			}
		};
		_results.LostFocus += delegate
		{
			if (!_search.Focused)
			{
				_results.Visible = false;
			}
		};
		base.HandleCreated += delegate
		{
			SendMessage(_search.Handle, 5377, (IntPtr)1, "Search components…");
		};
	}

	[DllImport("user32.dll", CharSet = CharSet.Unicode)]
	private static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, string lParam);

	public Tab AddTab(string name)
	{
		Tab tab = new Tab { Name = name };
		_tabs.Add(tab);
		return tab;
	}

	/// <summary>Registers every item for the search box (called by the builder once).</summary>
	public void IndexForSearch()
	{
		_allItems.Clear();
		foreach (Tab tab in _tabs)
		{
			foreach (Group group in tab.Groups)
			{
				foreach (Item item in group.Items)
				{
					if (!_allItems.Any((Item i) => i.Label == item.Label))
					{
						_allItems.Add(item);
					}
				}
			}
		}
	}

	public string ActiveTabName => (_active >= 0 && _active < _tabs.Count) ? _tabs[_active].Name : null;

	/// <summary>Removes all tabs (used when the colour scheme changes and the ribbon is rebuilt).</summary>
	public void Reset()
	{
		_tabs.Clear();
		_allItems.Clear();
		_hits.Clear();
		_hover = null;
		_down = null;
		_active = 0;
		_scroll = 0;
	}

	public void SelectTab(string name)
	{
		int num = _tabs.FindIndex((Tab t) => t.Name == name);
		if (num >= 0)
		{
			_active = num;
			_scroll = 0;
			Invalidate();
		}
	}

	/// <summary>Switches colours; call when the theme changes (icons are redrawn by the owner).</summary>
	public void SetDark(bool dark)
	{
		_dark = dark;
		Invalidate();
	}

	private Color Back => _dark ? Color.FromArgb(34, 37, 43) : Color.FromArgb(240, 243, 246);

	private Color Panel_ => _dark ? Color.FromArgb(44, 48, 57) : Color.FromArgb(252, 253, 254);

	private Color Accent => _dark ? Color.FromArgb(70, 200, 210) : Color.FromArgb(0, 118, 132);

	private Color Text_ => _dark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(30, 38, 46);

	private Color Faint => _dark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(110, 120, 132);

	private Color Border_ => _dark ? Color.FromArgb(72, 78, 90) : Color.FromArgb(206, 212, 220);

	private Color HoverBack => _dark ? Color.FromArgb(62, 72, 86) : Color.FromArgb(222, 238, 241);

	protected override void OnResize(EventArgs e)
	{
		base.OnResize(e);
		_search.Location = new Point(Math.Max(8, base.Width - _search.Width - 12), 4);
	}

	protected override void OnPaint(PaintEventArgs e)
	{
		Graphics graphics = e.Graphics;
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		graphics.Clear(Back);
		_hits.Clear();
		_tabRects.Clear();
		int num = 10;
		using (Font font = new Font(Font.FontFamily, 9.5f, FontStyle.Regular))
		{
			for (int i = 0; i < _tabs.Count; i++)
			{
				Size size = TextRenderer.MeasureText(_tabs[i].Name, font);
				Rectangle rectangle = new Rectangle(num, 4, size.Width + 22, TabHeight - 4);
				_tabRects.Add(rectangle);
				if (i == _active)
				{
					using SolidBrush brush = new SolidBrush(Panel_);
					graphics.FillRectangle(brush, rectangle);
					using SolidBrush brush2 = new SolidBrush(Accent);
					graphics.FillRectangle(brush2, rectangle.X, rectangle.Y, rectangle.Width, 3);
				}
				TextRenderer.DrawText(graphics, _tabs[i].Name, font, rectangle, (i == _active) ? Text_ : Faint, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix);
				num += rectangle.Width + 2;
			}
		}
		Rectangle rect = new Rectangle(0, TabHeight, base.Width, ContentHeight + 2);
		using (SolidBrush brush3 = new SolidBrush(Panel_))
		{
			graphics.FillRectangle(brush3, rect);
		}
		using (Pen pen = new Pen(Border_))
		{
			graphics.DrawLine(pen, 0, TabHeight, base.Width, TabHeight);
			graphics.DrawLine(pen, 0, rect.Bottom - 1, base.Width, rect.Bottom - 1);
		}
		if (_active >= 0 && _active < _tabs.Count)
		{
			DrawContent(graphics, _tabs[_active]);
		}
	}

	private void DrawContent(Graphics g, Tab tab)
	{
		int num = 8 - _scroll;
		int num2 = TabHeight + 4;
		int num3 = ContentHeight - 22;
		using Font font = new Font(Font.FontFamily, 8.25f);
		using Font font2 = new Font(Font.FontFamily, 8f);
		g.SetClip(new Rectangle(0, TabHeight + 1, base.Width, ContentHeight));
		foreach (Group group in tab.Groups)
		{
			List<Item> list = group.Items.Take((group.Items.Count > MaxPerGroup) ? (MaxPerGroup - 1) : group.Items.Count).ToList();
			int num4 = list.Count + ((group.Items.Count > MaxPerGroup) ? 1 : 0);
			int num5 = num;
			foreach (Item item in list)
			{
				Rectangle bounds = new Rectangle(num, num2, ButtonWidth, num3);
				Hit hit = new Hit { Bounds = bounds, Item = item };
				_hits.Add(hit);
				DrawButton(g, hit, item.Label, item.Icon, font);
				num += ButtonWidth;
			}
			if (group.Items.Count > MaxPerGroup)
			{
				Rectangle bounds2 = new Rectangle(num, num2, ButtonWidth, num3);
				Hit hit2 = new Hit { Bounds = bounds2, MoreOf = group };
				_hits.Add(hit2);
				DrawButton(g, hit2, $"More ({group.Items.Count - list.Count})  ▾", null, font);
				num += ButtonWidth;
			}
			Rectangle layoutRectangle = new Rectangle(num5, num2 + num3 + 1, Math.Max(10, num4 * ButtonWidth), 16);
			TextRenderer.DrawText(g, group.Title, font2, layoutRectangle, Faint, TextFormatFlags.HorizontalCenter | TextFormatFlags.SingleLine | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
			num += 6;
			using (Pen pen = new Pen(Border_))
			{
				g.DrawLine(pen, num - 3, num2 + 4, num - 3, num2 + num3 + 12);
			}
		}
		_contentWidth = num + _scroll + 8;
		g.ResetClip();
	}

	private void DrawButton(Graphics g, Hit hit, string label, Bitmap icon, Font font)
	{
		Rectangle bounds = hit.Bounds;
		if (Same(hit, _hover) || Same(hit, _down))
		{
			using GraphicsPath path = RoundRect(Rectangle.Inflate(bounds, -2, -1), 5);
			using SolidBrush brush = new SolidBrush(Same(hit, _down) ? Color.FromArgb(60, Accent) : HoverBack);
			g.FillPath(brush, path);
			using Pen pen = new Pen(Accent);
			g.DrawPath(pen, path);
		}
		if (icon != null)
		{
			g.DrawImage(icon, bounds.X + (bounds.Width - icon.Width) / 2, bounds.Y + 3, icon.Width, icon.Height);
		}
		Rectangle rectangle = new Rectangle(bounds.X + 2, bounds.Y + 46, bounds.Width - 4, bounds.Height - 46);
		TextRenderer.DrawText(g, label, font, rectangle, Text_, TextFormatFlags.HorizontalCenter | TextFormatFlags.Top | TextFormatFlags.WordBreak | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
	}

	private static GraphicsPath RoundRect(Rectangle r, int radius)
	{
		GraphicsPath graphicsPath = new GraphicsPath();
		int num = radius * 2;
		graphicsPath.AddArc(r.X, r.Y, num, num, 180f, 90f);
		graphicsPath.AddArc(r.Right - num, r.Y, num, num, 270f, 90f);
		graphicsPath.AddArc(r.Right - num, r.Bottom - num, num, num, 0f, 90f);
		graphicsPath.AddArc(r.X, r.Bottom - num, num, num, 90f, 90f);
		graphicsPath.CloseFigure();
		return graphicsPath;
	}

	private Hit HitAt(Point p)
	{
		return _hits.FirstOrDefault((Hit h) => h.Bounds.Contains(p));
	}

	/// <summary>Hits are rebuilt on every repaint, so compare what they stand for, not the objects.</summary>
	private static bool Same(Hit a, Hit b)
	{
		return a != null && b != null && a.Item == b.Item && a.MoreOf == b.MoreOf;
	}

	protected override void OnMouseMove(MouseEventArgs e)
	{
		base.OnMouseMove(e);
		if (_down != null && _down.Item != null && _down.Item.Preset != null && e.Button == MouseButtons.Left && (Math.Abs(e.X - _downPoint.X) > 6 || Math.Abs(e.Y - _downPoint.Y) > 6))
		{
			LibraryPreset preset = _down.Item.Preset;
			_down = null;
			Invalidate();
			DoDragDrop(preset, DragDropEffects.Copy);
			return;
		}
		Hit hit = HitAt(e.Location);
		if (!Same(hit, _hover) && !(hit == null && _hover == null))
		{
			_hover = hit;
			Cursor = ((hit != null) ? Cursors.Hand : Cursors.Default);
			_tip.SetToolTip(this, (hit == null) ? "" : ((hit.Item != null) ? hit.Item.Label : "Show the other items in this group"));
			Invalidate();
		}
	}

	protected override void OnMouseLeave(EventArgs e)
	{
		base.OnMouseLeave(e);
		_hover = null;
		Invalidate();
	}

	protected override void OnMouseDown(MouseEventArgs e)
	{
		base.OnMouseDown(e);
		if (e.Button != MouseButtons.Left)
		{
			return;
		}
		for (int i = 0; i < _tabRects.Count; i++)
		{
			if (_tabRects[i].Contains(e.Location))
			{
				SelectTab(_tabs[i].Name);
				return;
			}
		}
		_down = HitAt(e.Location);
		_downPoint = e.Location;
		Invalidate();
	}

	protected override void OnMouseUp(MouseEventArgs e)
	{
		base.OnMouseUp(e);
		Hit down = _down;
		_down = null;
		Invalidate();
		if (down == null || e.Button != MouseButtons.Left || !Same(HitAt(e.Location), down))
		{
			return;
		}
		if (down.MoreOf != null)
		{
			ShowMore(down);
		}
		else if (down.Item != null)
		{
			Activate(down.Item);
		}
	}

	protected override void OnMouseWheel(MouseEventArgs e)
	{
		base.OnMouseWheel(e);
		int num = Math.Max(0, _contentWidth - base.Width);
		_scroll = Math.Max(0, Math.Min(num, _scroll - e.Delta));
		Invalidate();
	}

	private void Activate(Item item)
	{
		if (item.Preset != null)
		{
			this.PresetChosen?.Invoke(item.Preset);
		}
		else
		{
			item.Command?.Invoke();
		}
	}

	private void ShowMore(Hit hit)
	{
		ContextMenuStrip contextMenuStrip = new ContextMenuStrip
		{
			ImageScalingSize = new Size(56, 40),
			ShowImageMargin = true
		};
		foreach (Item item in hit.MoreOf.Items)
		{
			Item captured = item;
			ToolStripMenuItem toolStripMenuItem = new ToolStripMenuItem(item.Label, item.Icon);
			toolStripMenuItem.Click += delegate
			{
				Activate(captured);
			};
			contextMenuStrip.Items.Add(toolStripMenuItem);
		}
		contextMenuStrip.Closed += delegate
		{
			contextMenuStrip.Dispose();
		};
		contextMenuStrip.Show(this, new Point(hit.Bounds.X, hit.Bounds.Bottom));
	}

	private void UpdateResults()
	{
		string text = _search.Text.Trim();
		if (text.Length == 0)
		{
			_results.Visible = false;
			return;
		}
		string[] terms = text.ToLowerInvariant().Split(new char[1] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
		List<Item> list = _allItems.Where((Item i) => terms.All((string t) => i.Label.ToLowerInvariant().Contains(t))).Take(14).ToList();
		_results.Items.Clear();
		foreach (Item item in list)
		{
			_results.Items.Add(item);
		}
		_results.DisplayMember = "Label";
		Form form = FindForm();
		if (list.Count == 0 || form == null)
		{
			_results.Visible = false;
			return;
		}
		if (_results.Parent != form)
		{
			form.Controls.Add(_results);
		}
		Point point = form.PointToClient(_search.PointToScreen(new Point(0, _search.Height)));
		_results.Font = Font;
		_results.BackColor = Panel_;
		_results.ForeColor = Text_;
		_results.Width = 320;
		_results.Height = Math.Min(14, list.Count) * _results.ItemHeight + 4;
		_results.Location = new Point(Math.Max(0, Math.Min(point.X, form.ClientSize.Width - _results.Width - 4)), point.Y);
		_results.SelectedIndex = 0;
		_results.BringToFront();
		_results.Visible = true;
	}

	private void OnSearchKey(object sender, KeyEventArgs e)
	{
		if (e.KeyCode == Keys.Down && _results.Visible)
		{
			_results.Focus();
			e.Handled = true;
		}
		else if (e.KeyCode == Keys.Return && _results.Visible)
		{
			PickResult();
			e.Handled = true;
			e.SuppressKeyPress = true;
		}
		else if (e.KeyCode == Keys.Escape)
		{
			_search.Text = "";
			_results.Visible = false;
		}
	}

	private void PickResult()
	{
		if (_results.SelectedItem is Item item)
		{
			_results.Visible = false;
			_search.Text = "";
			Activate(item);
		}
	}

	protected override void Dispose(bool disposing)
	{
		if (disposing)
		{
			_results.Dispose();
		}
		base.Dispose(disposing);
	}
}
