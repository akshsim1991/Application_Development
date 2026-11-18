using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Row of document (file) tabs with a close button on each tab and a "+" button for a new file. Own custom-painted control.</summary>
public sealed class DocumentTabs : Control
{
	private readonly List<string> _titles = new List<string>();

	private readonly List<Rectangle> _tabRects = new List<Rectangle>();

	private readonly List<Rectangle> _closeRects = new List<Rectangle>();

	private Rectangle _plus;

	private int _selected;

	private int _hover = -1;

	private int _hoverClose = -1;

	public event Action<int> TabSelected;

	public event Action<int> TabClosed;

	public event Action NewRequested;

	public DocumentTabs()
	{
		SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw | ControlStyles.UserPaint, value: true);
		Dock = DockStyle.Top;
		Height = 32;
		Font = new Font("Segoe UI", 9f);
	}

	public void SetTabs(IEnumerable<string> titles, int selected)
	{
		_titles.Clear();
		_titles.AddRange(titles);
		_selected = selected;
		Invalidate();
	}

	private static bool Dark => AppSettings.DarkMode;

	private Color Back => Dark ? Theme.Dk(Color.FromArgb(30, 33, 39)) : Color.FromArgb(228, 232, 238);

	private Color TabBack => Dark ? Theme.Dk(Color.FromArgb(52, 56, 66)) : Color.White;

	private Color Text_ => Dark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(30, 38, 46);

	private Color Faint => Dark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(100, 110, 122);

	private Color Accent => Dark ? Color.FromArgb(70, 200, 210) : Color.FromArgb(0, 118, 132);

	protected override void OnPaint(PaintEventArgs e)
	{
		Graphics graphics = e.Graphics;
		graphics.SmoothingMode = SmoothingMode.AntiAlias;
		graphics.Clear(Back);
		_tabRects.Clear();
		_closeRects.Clear();
		int num = 8;
		int num2 = Math.Max(90, Math.Min(220, (base.Width - 60) / Math.Max(1, _titles.Count)));
		for (int i = 0; i < _titles.Count; i++)
		{
			Rectangle rectangle = new Rectangle(num, 4, num2, base.Height - 4);
			_tabRects.Add(rectangle);
			Rectangle rectangle2 = new Rectangle(rectangle.Right - 24, rectangle.Y + (rectangle.Height - 16) / 2, 16, 16);
			_closeRects.Add(rectangle2);
			bool flag = i == _selected;
			using (SolidBrush brush = new SolidBrush(flag ? TabBack : ((i == _hover) ? Color.FromArgb(40, Accent) : Color.Transparent)))
			{
				graphics.FillRectangle(brush, rectangle);
			}
			if (flag)
			{
				using SolidBrush brush2 = new SolidBrush(Accent);
				graphics.FillRectangle(brush2, rectangle.X, rectangle.Y, rectangle.Width, 3);
			}
			Rectangle bounds = new Rectangle(rectangle.X + 10, rectangle.Y + 3, rectangle.Width - 38, rectangle.Height - 3);
			TextRenderer.DrawText(graphics, _titles[i], Font, bounds, flag ? Text_ : Faint, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
			if (i == _hoverClose)
			{
				using SolidBrush brush3 = new SolidBrush(Color.FromArgb(60, 200, 60, 60));
				graphics.FillEllipse(brush3, rectangle2);
			}
			using Pen pen = new Pen((i == _hoverClose) ? Color.FromArgb(200, 40, 40) : Faint, 1.6f);
			graphics.DrawLine(pen, rectangle2.X + 4, rectangle2.Y + 4, rectangle2.Right - 4, rectangle2.Bottom - 4);
			graphics.DrawLine(pen, rectangle2.Right - 4, rectangle2.Y + 4, rectangle2.X + 4, rectangle2.Bottom - 4);
			num += num2 + 2;
		}
		_plus = new Rectangle(num + 2, 8, 24, 22);
		using Pen pen2 = new Pen(Accent, 2f);
		graphics.DrawLine(pen2, _plus.X + 6, _plus.Y + 11, _plus.Right - 6, _plus.Y + 11);
		graphics.DrawLine(pen2, _plus.X + 12, _plus.Y + 5, _plus.X + 12, _plus.Bottom - 5);
	}

	protected override void OnMouseMove(MouseEventArgs e)
	{
		base.OnMouseMove(e);
		int num = _tabRects.FindIndex((Rectangle r) => r.Contains(e.Location));
		int num2 = _closeRects.FindIndex((Rectangle r) => r.Contains(e.Location));
		if (num != _hover || num2 != _hoverClose)
		{
			_hover = num;
			_hoverClose = num2;
			Invalidate();
		}
	}

	protected override void OnMouseLeave(EventArgs e)
	{
		base.OnMouseLeave(e);
		_hover = -1;
		_hoverClose = -1;
		Invalidate();
	}

	protected override void OnMouseUp(MouseEventArgs e)
	{
		base.OnMouseUp(e);
		int num = _tabRects.FindIndex((Rectangle r) => r.Contains(e.Location));
		if (e.Button == MouseButtons.Middle && num >= 0)
		{
			this.TabClosed?.Invoke(num);
		}
		else
		{
			if (e.Button != MouseButtons.Left)
			{
				return;
			}
			int num2 = _closeRects.FindIndex((Rectangle r) => r.Contains(e.Location));
			if (num2 >= 0)
			{
				this.TabClosed?.Invoke(num2);
			}
			else if (num >= 0)
			{
				this.TabSelected?.Invoke(num);
			}
			else if (_plus.Contains(e.Location))
			{
				this.NewRequested?.Invoke();
			}
		}
	}
}
