using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>The File view that covers the window below the tab row: New, Open, Recent, Save, Save As, Close, Export, Information, Options, Exit.</summary>
public sealed class BackstageView : Panel
{
	private sealed class Entry : Control
	{
		private bool _hover;

		public string Title;

		public string Detail;

		public Bitmap Icon;

		public Action Click_;

		public bool Dark;

		public Entry()
		{
			SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.UserPaint | ControlStyles.ResizeRedraw, value: true);
			Cursor = Cursors.Hand;
			base.Size = new Size(560, 62);
			Margin = new Padding(0, 0, 0, 6);
		}

		protected override void OnMouseEnter(EventArgs e)
		{
			base.OnMouseEnter(e);
			_hover = true;
			Invalidate();
		}

		protected override void OnMouseLeave(EventArgs e)
		{
			base.OnMouseLeave(e);
			_hover = false;
			Invalidate();
		}

		protected override void OnClick(EventArgs e)
		{
			base.OnClick(e);
			Click_?.Invoke();
		}

		protected override void OnPaint(PaintEventArgs e)
		{
			Graphics graphics = e.Graphics;
			graphics.Clear(_hover ? (Dark ? Theme.Dk(Color.FromArgb(52, 56, 66)) : Color.FromArgb(226, 240, 243)) : BackColor);
			if (Icon != null)
			{
				graphics.DrawImage(Icon, 14, 14, 34, 34);
			}
			Color foreColor = (Dark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(30, 38, 46));
			Color foreColor2 = (Dark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(100, 110, 122));
			using Font font = new Font("Segoe UI Semibold", 10.5f);
			using Font font2 = new Font("Segoe UI", 9f);
			TextRenderer.DrawText(graphics, Title, font, new Rectangle(62, 8, Width - 70, 24), foreColor, TextFormatFlags.Left | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
			TextRenderer.DrawText(graphics, Detail ?? "", font2, new Rectangle(62, 32, Width - 70, 24), foreColor2, TextFormatFlags.Left | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
		}
	}

	private readonly Func<string, RCmd> _cmd;

	private readonly Func<List<(string Text, Action Run)>> _examples;

	private readonly Action<string> _openFile;

	private readonly Action _close;

	private readonly Panel _nav = new Panel { Dock = DockStyle.Left, Width = 220 };

	private readonly FlowLayoutPanel _content = new FlowLayoutPanel
	{
		Dock = DockStyle.Fill,
		AutoScroll = true,
		FlowDirection = FlowDirection.TopDown,
		WrapContents = false,
		Padding = new Padding(30, 24, 20, 20)
	};

	private List<string> _recent = new List<string>();

	private bool _dark;

	private string _selected = "Open";

	public BackstageView(Func<string, RCmd> cmd, Func<List<(string Text, Action Run)>> examples, Action<string> openFile, Action close)
	{
		_cmd = cmd;
		_examples = examples;
		_openFile = openFile;
		_close = close;
		base.Controls.Add(_content);
		base.Controls.Add(_nav);
		base.Visible = false;
	}

	private Color NavBack => _dark ? Theme.Dk(Color.FromArgb(0, 84, 96)) : Color.FromArgb(0, 118, 132);

	private Color ContentBack => _dark ? Theme.Dk(Color.FromArgb(44, 48, 57)) : Color.FromArgb(250, 251, 252);

	public void Rebuild(IEnumerable<string> recent, bool dark)
	{
		_recent = recent.Where(File.Exists).Take(12).ToList();
		_dark = dark;
		BuildNav();
		ShowSection(_selected);
	}

	private void BuildNav()
	{
		_nav.Controls.Clear();
		_nav.BackColor = NavBack;
		string[] array = new string[11] { "<", "New", "Open", "Recent", "Save", "SaveAs", "Close", "Export", "Info", "Options", "Exit" };
		string[] array2 = new string[11] { "Back", "New", "Open", "Recent", "Save", "Save As", "Close", "Export", "Information", "Options", "Exit" };
		int num = 14;
		for (int i = 0; i < array.Length; i++)
		{
			string key = array[i];
			bool flag = key == "<";
			Label label = new Label
			{
				Text = (flag ? "←   Back" : array2[i]),
				ForeColor = Color.White,
				Font = new Font("Segoe UI", flag ? 11f : 11.5f),
				Location = new Point(0, num),
				Size = new Size(_nav.Width, 40),
				TextAlign = ContentAlignment.MiddleLeft,
				Padding = new Padding(26, 0, 0, 0),
				Cursor = Cursors.Hand,
				BackColor = ((key == _selected) ? Color.FromArgb(40, 255, 255, 255) : Color.Transparent)
			};
			label.MouseEnter += delegate
			{
				label.BackColor = Color.FromArgb(50, 255, 255, 255);
			};
			label.MouseLeave += delegate
			{
				label.BackColor = ((key == _selected) ? Color.FromArgb(40, 255, 255, 255) : Color.Transparent);
			};
			label.Click += delegate
			{
				OnNav(key);
			};
			_nav.Controls.Add(label);
			num += (flag ? 52 : 42);
		}
	}

	private void Run(string cmdKey, bool close = true)
	{
		RCmd rCmd = _cmd(cmdKey);
		if (close)
		{
			_close();
		}
		if (rCmd != null && (rCmd.Enabled == null || rCmd.Enabled()))
		{
			rCmd.Run();
		}
	}

	private void OnNav(string key)
	{
		switch (key)
		{
		case "<":
			_close();
			break;
		case "Save":
		case "SaveAs":
		case "Close":
		case "Info":
		case "Exit":
			Run(key);
			break;
		case "Options":
			Run("Settings");
			break;
		default:
			_selected = key;
			BuildNav();
			ShowSection(key);
			break;
		}
	}

	private Entry Add(string title, string detail, string icon, Action click)
	{
		Entry entry = new Entry
		{
			Title = title,
			Detail = detail,
			Icon = RibbonGlyphs.Make(icon, _dark ? ColorScheme.Dark : ColorScheme.Light, 34),
			Click_ = click,
			Dark = _dark,
			BackColor = ContentBack
		};
		_content.Controls.Add(entry);
		return entry;
	}

	private void Heading(string text)
	{
		_content.Controls.Add(new Label
		{
			Text = text,
			Font = new Font("Segoe UI Semibold", 15f),
			ForeColor = (_dark ? Color.FromArgb(226, 229, 236) : Color.FromArgb(30, 38, 46)),
			AutoSize = true,
			Margin = new Padding(0, 0, 0, 14)
		});
	}

	private void SubHeading(string text)
	{
		_content.Controls.Add(new Label
		{
			Text = text,
			Font = new Font("Segoe UI Semibold", 10.5f),
			ForeColor = (_dark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(100, 110, 122)),
			AutoSize = true,
			Margin = new Padding(0, 14, 0, 6)
		});
	}

	private void RecentList()
	{
		if (_recent.Count == 0)
		{
			_content.Controls.Add(new Label
			{
				Text = "No recent circuits yet.",
				AutoSize = true,
				ForeColor = (_dark ? Color.FromArgb(150, 158, 172) : Color.FromArgb(100, 110, 122))
			});
		}
		foreach (string path in _recent)
		{
			string p = path;
			Add(Path.GetFileNameWithoutExtension(p), Path.GetDirectoryName(p), "Open", delegate
			{
				_openFile(p);
			});
		}
	}

	private void ShowSection(string key)
	{
		_content.SuspendLayout();
		_content.Controls.Clear();
		_content.BackColor = ContentBack;
		switch (key)
		{
		case "New":
			Heading("New");
			Add("Blank circuit", "Start with an empty workspace in a new tab", "New", delegate
			{
				Run("New");
			});
			Add("Circuit generator", "Make a circuit from a movement sequence such as A+ B+ B- A-, or complete the parts you placed", "Generator", delegate
			{
				Run("Generator");
			});
			SubHeading("Open an example in a new tab");
			foreach (var example in _examples())
			{
				Action run = example.Run;
				Add(example.Text, "", "Examples", delegate
				{
					_close();
					run();
				});
			}
			break;
		case "Open":
			Heading("Open");
			Add("Browse for a circuit…", "Open a .pneu file from your computer in a new tab", "Open", delegate
			{
				Run("Open");
			});
			SubHeading("Recent circuits");
			RecentList();
			break;
		case "Recent":
			Heading("Recent circuits");
			RecentList();
			break;
		case "Export":
			Heading("Export");
			Add("PDF report: print preview, save and print", "All sheets with the title block, the parts list and the explanation", "Pdf", delegate
			{
				Run("Pdf");
			});
			Add("SVG drawing (this page)", "A vector picture of the current page", "Svg", delegate
			{
				Run("Svg");
			});
			Add("DXF drawing for CAD (this page)", "Open the page in a CAD program", "Dxf", delegate
			{
				Run("Dxf");
			});
			Add("PNG image (this page)", "A picture of the current page", "Png", delegate
			{
				Run("Png");
			});
			Add("Parts list (CSV)", "The parts and their costs for a spreadsheet", "Csv", delegate
			{
				Run("Csv");
			});
			break;
		}
		_content.ResumeLayout();
	}
}
