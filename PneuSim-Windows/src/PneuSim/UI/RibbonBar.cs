using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>
/// PneuSim's own ribbon: a quick access toolbar, a File tab (backstage), tabs with groups of large and small buttons, toggles and
/// drop-downs, a search box and a collapse button. Everything is custom-painted (no third-party library).
/// Component symbols can be clicked or dragged into the workspace.
/// </summary>
public sealed class RibbonBar : Control
{
	public sealed class Item
	{
		public string Label;

		public string Tip;

		public LibraryPreset Preset;

		public Action Command;

		public Bitmap Icon;

		public bool IsSmall;

		public Func<bool> Enabled;

		public Func<bool> Checked;

		/// <summary>If set, clicking shows these entries in a drop-down menu instead of running Command.</summary>
		public Func<List<(string Text, Action Run)>> Menu;

		public string Key;
	}

	public sealed class Group
	{
		public string Title;

		public List<Item> Items = new List<Item>();

		public int Shown = int.MaxValue;
	}

	public sealed class Tab
	{
		public string Name;

		public bool IsFile;

		public List<Group> Groups = new List<Group>();
	}

	private sealed class Hit
	{
		public Rectangle Bounds;

		public Item Item;

		public Group MoreOf;
	}

	public const int HeaderHeight = 32;

	private const int ContentHeight = 112;

	private const int LargeWidth = 84;

	private const int SmallWidth = 124;

	private const int SmallRow = 27;

	private const int MaxPerGroup = 5;

	private readonly List<Tab> _tabs = new List<Tab>();

	private readonly List<Hit> _hits = new List<Hit>();

	private readonly List<Rectangle> _tabRects = new List<Rectangle>();

	private readonly TextBox _search = new TextBox();

	private readonly ToolTip _tip = new ToolTip { InitialDelay = 500 };

	private readonly ListBox _results = new ListBox { IntegralHeight = true, BorderStyle = BorderStyle.FixedSingle, Visible = false };

	private readonly List<Item> _allItems = new List<Item>();

	private ContextMenuStrip _moreMenu;

	private Rectangle _chevron;

	private int _active = 1;

	private int _scroll;

	private int _contentWidth;

	private Hit _hover;

	private Hit _down;

	private Point _downPoint;

	private bool _dark;

	private bool _collapsed;

	private bool _tempOpen;

	/// <summary>Quick access toolbar items (small icons at the left of the tab row).</summary>
	public List<Item> QuickAccess { get; } = new List<Item>();

	public event Action<LibraryPreset> PresetChosen;

	/// <summary>Raised when the File tab is clicked (the owner shows the backstage view).</summary>
	public event Action FileRequested;

	/// <summary>Raised when the user clicks anywhere on the ribbon except the File tab (used to close the File view).</summary>
	public event Action Interacted;

	public event Action CollapsedChanged;

	public IEnumerable<string> TabNames => _tabs.Select((Tab t) => t.Name);

	public string ActiveTabName => (_active >= 0 && _active < _tabs.Count) ? _tabs[_active].Name : null;

	/// <summary>True when only the tab row is shown (more room for the workspace).</summary>
	public bool Collapsed
	{
		get
		{
			return _collapsed;
		}
		set
		{
			_collapsed = value;
			_tempOpen = false;
			ApplyHeight();
			CollapsedChanged?.Invoke();
		}
	}

	private void ApplyHeight()
	{
		base.Height = HeaderHeight + ((_collapsed && !_tempOpen) ? 2 : (ContentHeight + 2));
		Invalidate();
	}

	public RibbonBar()
	{
		SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.UserPaint, value: true);
		Dock = DockStyle.Top;
		Font = new Font("Segoe UI", 9f);
		base.Height = HeaderHeight + ContentHeight + 2;
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
			SendMessage(_search.Handle, 5377, (IntPtr)1, "Search commands and components…");
		};
	}

	[DllImport("user32.dll", CharSet = CharSet.Unicode)]
	private static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, string lParam);

	public Tab AddTab(string name, bool isFile = false)
	{
		Tab tab = new Tab { Name = name, IsFile = isFile };
		_tabs.Add(tab);
		return tab;
	}

	public void IndexForSearch()
	{
		_allItems.Clear();
		foreach (Tab tab in _tabs)
		{
			foreach (Group group in tab.Groups)
			{
				foreach (Item item in group.Items)
				{
					if (!string.IsNullOrEmpty(item.Label) && !_allItems.Any((Item i) => i.Label == item.Label))
					{
						_allItems.Add(item);
					}
				}
			}
		}
	}

	public void Reset()
	{
		_tabs.Clear();
		_allItems.Clear();
		_hits.Clear();
		QuickAccess.Clear();
		_hover = null;
		_down = null;
		_active = 1;
		_scroll = 0;
	}

	public void SelectTab(string name)
	{
		int num = _tabs.FindIndex((Tab t) => t.Name == name && !t.IsFile);
		if (num >= 0)
		{
			_active = num;
			_scroll = 0;
			Invalidate();
		}
	}

	public void SetDark(bool dark)
	{
		_dark = dark;
		Invalidate();
	}

	private Color Back => _dark ? Theme.Dk(Color.FromArgb(34, 37, 43)) : Color.FromArgb(240, 243, 246);

	private Color Panel_ => _dark ? Theme.Dk(Color.FromArgb(44, 48, 57)) : Color.FromArgb(252, 253, 254);

	private Color Accent => _dark ? Color.FromArgb(70, 200, 210) : Color.FromArgb(0, 118, 132);

	private Color Text_ => _dark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(30, 38, 46);

	private Color Faint => _dark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(110, 120, 132);

	private Color Border_ => _dark ? Color.FromArgb(72, 78, 90) : Color.FromArgb(206, 212, 220);

	private Color HoverBack => _dark ? Color.FromArgb(62, 72, 86) : Color.FromArgb(222, 238, 241);

	protected override void OnResize(EventArgs e)
	{
		base.OnResize(e);
		_search.Location = new Point(Math.Max(8, base.Width - _search.Width - 44), 5);
	}

	private static bool IsEnabled(Item i)
	{
		return i == null || i.Enabled == null || i.Enabled();
	}

	private static bool IsChecked(Item i)
	{
		return i != null && i.Checked != null && i.Checked();
	}

	protected override void OnPaint(PaintEventArgs e)
	{
		Graphics graphics = e.Graphics;
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		graphics.Clear(Back);
		_hits.Clear();
		_tabRects.Clear();
		int num = 6;
		foreach (Item item in QuickAccess)
		{
			Rectangle rectangle = new Rectangle(num, 4, 26, 24);
			Hit hit = new Hit { Bounds = rectangle, Item = item };
			_hits.Add(hit);
			bool flag = IsEnabled(item);
			if (flag && Same(hit, _hover))
			{
				using SolidBrush brush = new SolidBrush(HoverBack);
				graphics.FillRectangle(brush, rectangle);
			}
			if (item.Icon != null)
			{
				DrawIcon(graphics, item.Icon, new Rectangle(rectangle.X + 5, rectangle.Y + 4, 16, 16), flag);
			}
			num += 28;
		}
		num += 6;
		using (Font font = new Font(Font.FontFamily, 9.5f))
		{
			for (int i = 0; i < _tabs.Count; i++)
			{
				Size size = TextRenderer.MeasureText(_tabs[i].Name, font);
				Rectangle rectangle2 = new Rectangle(num, 4, size.Width + (_tabs[i].IsFile ? 30 : 22), HeaderHeight - 4);
				_tabRects.Add(rectangle2);
				bool flag2 = i == _active && !_tabs[i].IsFile;
				if (_tabs[i].IsFile)
				{
					using SolidBrush brush2 = new SolidBrush(Accent);
					graphics.FillRectangle(brush2, rectangle2);
					TextRenderer.DrawText(graphics, _tabs[i].Name, font, rectangle2, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix);
				}
				else
				{
					if (flag2 && !(_collapsed && !_tempOpen))
					{
						using SolidBrush brush3 = new SolidBrush(Panel_);
						graphics.FillRectangle(brush3, rectangle2);
						using SolidBrush brush4 = new SolidBrush(Accent);
						graphics.FillRectangle(brush4, rectangle2.X + 6, rectangle2.Bottom - 3, rectangle2.Width - 12, 3);
					}
					TextRenderer.DrawText(graphics, _tabs[i].Name, font, rectangle2, flag2 ? Text_ : Faint, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix);
				}
				num += rectangle2.Width + 2;
			}
		}
		_chevron = new Rectangle(base.Width - 34, 6, 22, 22);
		using (Pen pen = new Pen(Faint, 1.8f))
		{
			int num2 = _chevron.X + 6;
			int num3 = _chevron.Y + 9;
			if (_collapsed)
			{
				graphics.DrawLines(pen, new Point[3]
				{
					new Point(num2, num3),
					new Point(num2 + 5, num3 + 5),
					new Point(num2 + 10, num3)
				});
			}
			else
			{
				graphics.DrawLines(pen, new Point[3]
				{
					new Point(num2, num3 + 5),
					new Point(num2 + 5, num3),
					new Point(num2 + 10, num3 + 5)
				});
			}
		}
		if (_collapsed && !_tempOpen)
		{
			using Pen pen2 = new Pen(Border_);
			graphics.DrawLine(pen2, 0, base.Height - 1, base.Width, base.Height - 1);
			return;
		}
		Rectangle rect = new Rectangle(0, HeaderHeight, base.Width, ContentHeight + 2);
		using (SolidBrush brush5 = new SolidBrush(Panel_))
		{
			graphics.FillRectangle(brush5, rect);
		}
		using (Pen pen3 = new Pen(Border_))
		{
			graphics.DrawLine(pen3, 0, HeaderHeight, base.Width, HeaderHeight);
			graphics.DrawLine(pen3, 0, rect.Bottom - 1, base.Width, rect.Bottom - 1);
		}
		if (_active >= 0 && _active < _tabs.Count && !_tabs[_active].IsFile)
		{
			DrawContent(graphics, _tabs[_active]);
		}
	}

	private static void DrawIcon(Graphics g, Image icon, Rectangle target, bool enabled)
	{
		if (enabled)
		{
			g.DrawImage(icon, target);
			return;
		}
		using ImageAttributes imageAttributes = new ImageAttributes();
		ColorMatrix newColorMatrix = new ColorMatrix(new float[5][]
		{
			new float[5] { 0.3f, 0.3f, 0.3f, 0f, 0f },
			new float[5] { 0.3f, 0.3f, 0.3f, 0f, 0f },
			new float[5] { 0.3f, 0.3f, 0.3f, 0f, 0f },
			new float[5] { 0f, 0f, 0f, 0.4f, 0f },
			new float[5] { 0.25f, 0.25f, 0.25f, 0f, 1f }
		});
		imageAttributes.SetColorMatrix(newColorMatrix);
		g.DrawImage(icon, target, 0, 0, icon.Width, icon.Height, GraphicsUnit.Pixel, imageAttributes);
	}

	private void DrawContent(Graphics g, Tab tab)
	{
		int num = 8 - _scroll;
		int num2 = HeaderHeight + 4;
		int num3 = ContentHeight - 22;
		using Font font = new Font(Font.FontFamily, 8.75f);
		using Font font2 = new Font(Font.FontFamily, 8f);
		g.SetClip(new Rectangle(0, HeaderHeight + 1, base.Width, ContentHeight));
		foreach (Group group in tab.Groups)
		{
			int num4 = num;
			bool allPresets = group.Items.Count > 0 && group.Items.All((Item i) => i.Preset != null);
			int fit = Math.Max(1, (base.Width - num - 16 - LargeWidth) / LargeWidth);
			bool flag = allPresets && (group.Items.Count > MaxPerGroup || group.Items.Count > fit);
			int take = Math.Min(MaxPerGroup - 1, fit);
			group.Shown = flag ? take : int.MaxValue;
			List<Item> list = (flag ? group.Items.Take(take).ToList() : group.Items);
			List<Item> list2 = new List<Item>();
			foreach (Item item in list)
			{
				if (item.IsSmall)
				{
					list2.Add(item);
					if (list2.Count == 3)
					{
						num = FlushSmall(g, list2, num, num2, font);
					}
					continue;
				}
				num = FlushSmall(g, list2, num, num2, font);
				Rectangle bounds = new Rectangle(num, num2, LargeWidth, num3);
				Hit hit = new Hit { Bounds = bounds, Item = item };
				_hits.Add(hit);
				DrawLarge(g, hit, item, font);
				num += LargeWidth;
			}
			num = FlushSmall(g, list2, num, num2, font);
			if (flag)
			{
				Rectangle bounds2 = new Rectangle(num, num2, LargeWidth, num3);
				Hit hit2 = new Hit { Bounds = bounds2, MoreOf = group };
				_hits.Add(hit2);
				DrawLarge(g, hit2, new Item { Label = $"More ({group.Items.Count - list.Count})  ▾" }, font);
				num += LargeWidth;
			}
			Rectangle layoutRectangle = new Rectangle(num4, num2 + num3 + 1, Math.Max(10, num - num4), 16);
			TextRenderer.DrawText(g, group.Title, font2, layoutRectangle, Faint, TextFormatFlags.HorizontalCenter | TextFormatFlags.SingleLine | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
			num += 6;
			using Pen pen = new Pen(Border_);
			g.DrawLine(pen, num - 3, num2 + 4, num - 3, num2 + num3 + 12);
		}
		_contentWidth = num + _scroll + 8;
		g.ResetClip();
	}

	private int FlushSmall(Graphics g, List<Item> stack, int x, int y, Font font)
	{
		if (stack.Count == 0)
		{
			return x;
		}
		for (int i = 0; i < stack.Count; i++)
		{
			Item item = stack[i];
			Rectangle bounds = new Rectangle(x, y + i * SmallRow, SmallWidth, SmallRow);
			Hit hit = new Hit { Bounds = bounds, Item = item };
			_hits.Add(hit);
			DrawSmall(g, hit, item, font);
		}
		stack.Clear();
		return x + SmallWidth;
	}

	private void DrawButtonBack(Graphics g, Hit hit, Item item, Rectangle bounds)
	{
		bool flag = IsEnabled(item);
		bool flag2 = IsChecked(item);
		bool flag3 = flag && (Same(hit, _hover) || Same(hit, _down));
		if (!(flag3 || flag2))
		{
			return;
		}
		using GraphicsPath path = RoundRect(Rectangle.Inflate(bounds, -2, -1), 5);
		using SolidBrush brush = new SolidBrush(Same(hit, _down) ? Color.FromArgb(70, Accent) : (flag2 ? Color.FromArgb(48, Accent) : HoverBack));
		g.FillPath(brush, path);
		using Pen pen = new Pen(flag2 ? Accent : Color.FromArgb(150, Accent));
		g.DrawPath(pen, path);
	}

	private void DrawLarge(Graphics g, Hit hit, Item item, Font font)
	{
		Rectangle bounds = hit.Bounds;
		DrawButtonBack(g, hit, item, bounds);
		bool flag = IsEnabled(item);
		if (item.Icon != null)
		{
			DrawIcon(g, item.Icon, new Rectangle(bounds.X + (bounds.Width - item.Icon.Width) / 2, bounds.Y + 3, item.Icon.Width, item.Icon.Height), flag);
		}
		Rectangle rectangle = new Rectangle(bounds.X + 2, bounds.Y + 46, bounds.Width - 4, bounds.Height - 46);
		string text = ((item.Menu != null) ? (item.Label + " ▾") : item.Label);
		TextRenderer.DrawText(g, text, font, rectangle, flag ? Text_ : Faint, TextFormatFlags.HorizontalCenter | TextFormatFlags.Top | TextFormatFlags.WordBreak | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
	}

	private void DrawSmall(Graphics g, Hit hit, Item item, Font font)
	{
		Rectangle bounds = hit.Bounds;
		DrawButtonBack(g, hit, item, bounds);
		bool flag = IsEnabled(item);
		if (item.Icon != null)
		{
			DrawIcon(g, item.Icon, new Rectangle(bounds.X + 6, bounds.Y + (bounds.Height - 16) / 2, 16, 16), flag);
		}
		Rectangle rectangle = new Rectangle(bounds.X + 26, bounds.Y, bounds.Width - 28, bounds.Height);
		string text = ((item.Menu != null) ? (item.Label + " ▾") : item.Label);
		TextRenderer.DrawText(g, text, font, rectangle, flag ? Text_ : Faint, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
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
			Cursor = ((hit != null && IsEnabled(hit.Item)) ? Cursors.Hand : Cursors.Default);
			string text = ((hit == null) ? "" : ((hit.Item != null) ? (string.IsNullOrEmpty(hit.Item.Tip) ? hit.Item.Label : hit.Item.Tip) : "Show the other items in this group"));
			_tip.SetToolTip(this, text);
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
		bool onFileTab = false;
		for (int t = 0; t < _tabRects.Count; t++)
		{
			if (_tabRects[t].Contains(e.Location) && _tabs[t].IsFile)
			{
				onFileTab = true;
			}
		}
		if (!onFileTab)
		{
			Interacted?.Invoke();
		}
		if (_chevron.Contains(e.Location))
		{
			Collapsed = !_collapsed;
			return;
		}
		for (int i = 0; i < _tabRects.Count; i++)
		{
			if (!_tabRects[i].Contains(e.Location))
			{
				continue;
			}
			if (_tabs[i].IsFile)
			{
				FileRequested?.Invoke();
				return;
			}
			if (_collapsed)
			{
				_active = i;
				_tempOpen = true;
				ApplyHeight();
			}
			else
			{
				_active = i;
				_scroll = 0;
				Invalidate();
			}
			return;
		}
		_down = HitAt(e.Location);
		_downPoint = e.Location;
		Invalidate();
	}

	protected override void OnMouseDoubleClick(MouseEventArgs e)
	{
		base.OnMouseDoubleClick(e);
		if (e.Button == MouseButtons.Left && e.Y < HeaderHeight && _tabRects.Any((Rectangle r) => r.Contains(e.Location)))
		{
			Collapsed = !_collapsed;
		}
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
		else if (down.Item != null && IsEnabled(down.Item))
		{
			Activate(down.Item, down.Bounds);
			if (_collapsed && _tempOpen && down.Item.Preset == null)
			{
				_tempOpen = false;
				ApplyHeight();
			}
		}
	}

	protected override void WndProc(ref Message m)
	{
		if (m.Msg == 526)
		{
			int num = (short)(((long)m.WParam >> 16) & 0xFFFF);
			int num2 = Math.Max(0, _contentWidth - base.Width);
			_scroll = Math.Max(0, Math.Min(num2, _scroll + num / 120 * 80));
			Invalidate();
			m.Result = IntPtr.Zero;
		}
		else
		{
			base.WndProc(ref m);
		}
	}

	protected override void OnMouseWheel(MouseEventArgs e)
	{
		base.OnMouseWheel(e);
		int num = Math.Max(0, _contentWidth - base.Width);
		_scroll = Math.Max(0, Math.Min(num, _scroll - e.Delta));
		Invalidate();
	}

	private void Activate(Item item, Rectangle bounds)
	{
		if (item.Menu != null)
		{
			ShowMenu(item.Menu(), new Point(bounds.X, bounds.Bottom));
		}
		else if (item.Preset != null)
		{
			PresetChosen?.Invoke(item.Preset);
		}
		else
		{
			item.Command?.Invoke();
		}
	}

	private void ShowMenu(List<(string Text, Action Run)> entries, Point where)
	{
		ContextMenuStrip contextMenuStrip = new ContextMenuStrip();
		foreach (var entry in entries)
		{
			if (entry.Text == "-")
			{
				contextMenuStrip.Items.Add(new ToolStripSeparator());
				continue;
			}
			Action run = entry.Run;
			ToolStripMenuItem toolStripMenuItem = new ToolStripMenuItem(entry.Text);
			toolStripMenuItem.Click += delegate
			{
				run?.Invoke();
			};
			contextMenuStrip.Items.Add(toolStripMenuItem);
		}
		_moreMenu?.Dispose();
		_moreMenu = contextMenuStrip;
		contextMenuStrip.Show(this, where);
	}

	private void ShowMore(Hit hit)
	{
		ContextMenuStrip contextMenuStrip = new ContextMenuStrip
		{
			ImageScalingSize = new Size(56, 40),
			ShowImageMargin = true
		};
		foreach (Item item in hit.MoreOf.Items.Skip(Math.Min(hit.MoreOf.Shown, hit.MoreOf.Items.Count)))
		{
			Item captured = item;
			ToolStripMenuItem toolStripMenuItem = new ToolStripMenuItem(item.Label, item.Icon);
			toolStripMenuItem.Click += delegate
			{
				Activate(captured, Rectangle.Empty);
			};
			contextMenuStrip.Items.Add(toolStripMenuItem);
		}
		_moreMenu?.Dispose();
		_moreMenu = contextMenuStrip;
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
		List<Item> list = _allItems.Where((Item i) => i.Menu == null && terms.All((string t) => i.Label.ToLowerInvariant().Contains(t))).Take(14).ToList();
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
		_results.Width = 340;
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
			if (IsEnabled(item))
			{
				Activate(item, Rectangle.Empty);
			}
		}
	}

	protected override void Dispose(bool disposing)
	{
		if (disposing)
		{
			_moreMenu?.Dispose();
			_results.Dispose();
		}
		base.Dispose(disposing);
	}
}
