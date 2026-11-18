using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>The screen shown when PneuSim starts: new, open, drop a file, examples and recent files.</summary>
public sealed class StartScreen : Panel
{
	private readonly Panel _drop = new Panel { AllowDrop = true, Size = new Size(330, 120) };

	private readonly Label _dropText = new Label { Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleCenter, Text = "Drop a PneuSim file here\nto open it", AllowDrop = true };

	public event Action NewRequested;

	public event Action OpenRequested;

	public event Action<string> FileRequested;

	public event Action<string> ExampleRequested;

	public StartScreen(IEnumerable<string> exampleNames, IEnumerable<string> recentFiles, bool dark)
	{
		Dock = DockStyle.Fill;
		AllowDrop = true;
		Color color = (dark ? Theme.Dk(Color.FromArgb(34, 37, 43)) : Color.FromArgb(246, 248, 250));
		Color color2 = (dark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(30, 38, 46));
		Color color3 = (dark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(100, 110, 122));
		Color color4 = (dark ? Color.FromArgb(70, 200, 210) : Color.FromArgb(0, 118, 132));
		BackColor = color;
		ForeColor = color2;
		Font = new Font("Segoe UI", 10f);
		TableLayoutPanel table = new TableLayoutPanel
		{
			ColumnCount = 2,
			RowCount = 1,
			AutoSize = true,
			Anchor = AnchorStyles.None,
			BackColor = color
		};
		table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 380f));
		table.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 360f));
		FlowLayoutPanel left = Column(color);
		left.Controls.Add(new Label
		{
			Text = AppInfo.Name,
			Font = new Font("Segoe UI Light", 34f),
			ForeColor = color4,
			AutoSize = true
		});
		left.Controls.Add(new Label
		{
			Text = "Design, simulate and check pneumatic, electro-pneumatic and hydraulic circuits.",
			ForeColor = color3,
			Width = 340,
			Height = 44
		});
		left.Controls.Add(MakeButton("New circuit", color4, delegate
		{
			this.NewRequested?.Invoke();
		}));
		left.Controls.Add(MakeButton("Open circuit…", color4, delegate
		{
			this.OpenRequested?.Invoke();
		}));
		_drop.BackColor = (dark ? Theme.Dk(Color.FromArgb(44, 48, 57)) : Color.White);
		_drop.Margin = new Padding(0, 18, 0, 0);
		_drop.Paint += delegate(object s, PaintEventArgs e)
		{
			using Pen pen = new Pen(color4, 1.5f)
			{
				DashStyle = System.Drawing.Drawing2D.DashStyle.Dash
			};
			e.Graphics.DrawRectangle(pen, 4, 4, _drop.Width - 9, _drop.Height - 9);
		};
		_dropText.ForeColor = color3;
		_drop.Controls.Add(_dropText);
		left.Controls.Add(_drop);
		foreach (Control item in new Control[3] { this, _drop, _dropText })
		{
			item.DragEnter += OnDragEnter;
			item.DragDrop += OnDragDrop;
		}
		FlowLayoutPanel right = Column(color);
		right.Controls.Add(Heading("Recent circuits", color3));
		List<string> list = recentFiles.Where(File.Exists).Take(6).ToList();
		if (list.Count == 0)
		{
			right.Controls.Add(new Label
			{
				Text = "None yet",
				ForeColor = color3,
				AutoSize = true
			});
		}
		foreach (string path in list)
		{
			right.Controls.Add(Link(Path.GetFileNameWithoutExtension(path), path, color4, delegate
			{
				this.FileRequested?.Invoke(path);
			}));
		}
		right.Controls.Add(Heading("Examples", color3));
		foreach (string name in exampleNames.Take(8))
		{
			right.Controls.Add(Link(name, null, color4, delegate
			{
				this.ExampleRequested?.Invoke(name);
			}));
		}
		table.Controls.Add(left, 0, 0);
		table.Controls.Add(right, 1, 0);
		base.Controls.Add(table);
		base.Resize += delegate
		{
			table.Location = new Point(Math.Max(0, (base.Width - table.Width) / 2), Math.Max(10, (base.Height - table.Height) / 2));
		};
		base.VisibleChanged += delegate
		{
			table.Location = new Point(Math.Max(0, (base.Width - table.Width) / 2), Math.Max(10, (base.Height - table.Height) / 2));
		};
	}

	private static FlowLayoutPanel Column(Color back)
	{
		return new FlowLayoutPanel
		{
			FlowDirection = FlowDirection.TopDown,
			WrapContents = false,
			AutoSize = true,
			BackColor = back,
			Margin = new Padding(10)
		};
	}

	private static Label Heading(string text, Color color)
	{
		return new Label
		{
			Text = text,
			ForeColor = color,
			Font = new Font("Segoe UI Semibold", 10f),
			AutoSize = true,
			Margin = new Padding(0, 14, 0, 4)
		};
	}

	private static Button MakeButton(string text, Color accent, Action click)
	{
		Button button = new Button
		{
			Text = text,
			Width = 330,
			Height = 40,
			FlatStyle = FlatStyle.Flat,
			ForeColor = accent,
			TextAlign = ContentAlignment.MiddleLeft,
			Padding = new Padding(10, 0, 0, 0),
			Margin = new Padding(0, 6, 0, 0),
			Cursor = Cursors.Hand
		};
		button.FlatAppearance.BorderColor = accent;
		button.Click += delegate
		{
			click();
		};
		return button;
	}

	private static LinkLabel Link(string text, string tip, Color color, Action click)
	{
		LinkLabel linkLabel = new LinkLabel
		{
			Text = text,
			AutoSize = true,
			LinkColor = color,
			ActiveLinkColor = color,
			LinkBehavior = LinkBehavior.HoverUnderline,
			Margin = new Padding(0, 2, 0, 2)
		};
		linkLabel.LinkClicked += delegate
		{
			click();
		};
		if (tip != null)
		{
			new ToolTip().SetToolTip(linkLabel, tip);
		}
		return linkLabel;
	}

	private static void OnDragEnter(object sender, DragEventArgs e)
	{
		e.Effect = (e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None);
	}

	private void OnDragDrop(object sender, DragEventArgs e)
	{
		if (e.Data.GetData(DataFormats.FileDrop) is string[] source)
		{
			string text = source.FirstOrDefault((string f) => f.EndsWith(".pneu", StringComparison.OrdinalIgnoreCase));
			if (text != null)
			{
				this.FileRequested?.Invoke(text);
			}
		}
	}
}
