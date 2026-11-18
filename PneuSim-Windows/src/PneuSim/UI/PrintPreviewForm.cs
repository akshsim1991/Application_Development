using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Printing;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Print preview of the PDF report: look at every sheet, then save it as a PDF or print it.</summary>
public sealed class PrintPreviewForm : Form
{
	private readonly PdfDocument _doc;

	private readonly Panel _view = new Panel { Dock = DockStyle.Fill, AutoScroll = true, BackColor = Color.FromArgb(120, 124, 132) };

	private readonly Label _pageLabel = new Label { AutoSize = true, Margin = new Padding(8, 8, 8, 0) };

	private readonly ComboBox _zoom = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 110 };

	private readonly Dictionary<int, Bitmap> _cache = new Dictionary<int, Bitmap>();

	private int _page;

	private int _printPage;

	/// <summary>Raised when the user chooses Save as PDF.</summary>
	public event Action SaveRequested;

	public PrintPreviewForm(PdfDocument doc, string title)
	{
		_doc = doc;
		Text = "Print preview - " + title;
		Font = new Font("Segoe UI", 9f);
		base.StartPosition = FormStartPosition.CenterParent;
		base.Size = new Size(1100, 780);
		base.ShowInTaskbar = false;
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Top,
			Height = 44,
			Padding = new Padding(8, 6, 8, 4),
			WrapContents = false
		};
		Button prev = Btn("◀ Previous", 100);
		Button next = Btn("Next ▶", 90);
		_zoom.Items.AddRange(new object[4] { "Fit page", "Fit width", "100 %", "150 %" });
		_zoom.SelectedIndex = 0;
		_zoom.Margin = new Padding(14, 4, 3, 3);
		Button save = Btn("Save as PDF…", 120);
		Button print = Btn("Print…", 90);
		Button close = Btn("Close", 80);
		save.Margin = new Padding(24, 3, 3, 3);
		flowLayoutPanel.Controls.AddRange(new Control[8] { prev, next, _pageLabel, _zoom, save, print, close, new Label { AutoSize = true } });
		base.Controls.Add(_view);
		base.Controls.Add(flowLayoutPanel);
		_view.Paint += PaintPage;
		_view.Resize += delegate
		{
			_view.Invalidate();
		};
		prev.Click += delegate
		{
			Go(_page - 1);
		};
		next.Click += delegate
		{
			Go(_page + 1);
		};
		_zoom.SelectedIndexChanged += delegate
		{
			Layout_();
		};
		save.Click += delegate
		{
			this.SaveRequested?.Invoke();
		};
		print.Click += delegate
		{
			PrintAll();
		};
		close.Click += delegate
		{
			Close();
		};
		base.KeyPreview = true;
		base.KeyDown += delegate(object s, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.Escape)
			{
				Close();
			}
			else if (e.KeyCode == Keys.PageDown || e.KeyCode == Keys.Right)
			{
				Go(_page + 1);
			}
			else if (e.KeyCode == Keys.PageUp || e.KeyCode == Keys.Left)
			{
				Go(_page - 1);
			}
			else if (e.Control && e.KeyCode == Keys.P)
			{
				PrintAll();
			}
		};
		Go(0);
	}

	private static Button Btn(string text, int width)
	{
		return new Button
		{
			Text = text,
			Width = width,
			Height = 30
		};
	}

	private void Go(int page)
	{
		_page = Math.Max(0, Math.Min(_doc.Pages.Count - 1, page));
		_pageLabel.Text = "Sheet " + (_page + 1) + " of " + _doc.Pages.Count;
		Layout_();
	}

	private float Scale()
	{
		PdfPage pdfPage = _doc.Pages[_page];
		switch (_zoom.SelectedIndex)
		{
		case 1:
			return (float)(_view.ClientSize.Width - 40) / pdfPage.Width;
		case 2:
			return 1.333f;
		case 3:
			return 2f;
		default:
			return Math.Min((float)(_view.ClientSize.Width - 40) / pdfPage.Width, (float)(_view.ClientSize.Height - 40) / pdfPage.Height);
		}
	}

	private void Layout_()
	{
		PdfPage pdfPage = _doc.Pages[_page];
		float num = Scale();
		_view.AutoScrollMinSize = new Size((int)(pdfPage.Width * num) + 40, (int)(pdfPage.Height * num) + 40);
		_view.Invalidate();
	}

	private static void Replay(PdfPage page, Graphics g, float scale)
	{
		g.SmoothingMode = SmoothingMode.AntiAlias;
		g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAlias;
		foreach (Action<Graphics, float> op in page.Ops)
		{
			op(g, scale);
		}
	}

	private void PaintPage(object sender, PaintEventArgs e)
	{
		PdfPage pdfPage = _doc.Pages[_page];
		float num = Scale();
		int num2 = (int)(pdfPage.Width * num);
		int num3 = (int)(pdfPage.Height * num);
		int num4 = Math.Max(20, (_view.ClientSize.Width - num2) / 2) + _view.AutoScrollPosition.X;
		int num5 = Math.Max(20, (_view.ClientSize.Height - num3) / 2) + _view.AutoScrollPosition.Y;
		e.Graphics.FillRectangle(Brushes.Black, num4 + 4, num5 + 4, num2, num3);
		e.Graphics.FillRectangle(Brushes.White, num4, num5, num2, num3);
		GraphicsState gstate = e.Graphics.Save();
		e.Graphics.SetClip(new Rectangle(num4, num5, num2, num3));
		e.Graphics.TranslateTransform(num4, num5);
		Replay(pdfPage, e.Graphics, num);
		e.Graphics.Restore(gstate);
	}

	private void PrintAll()
	{
		using PrintDocument printDocument = new PrintDocument();
		printDocument.DocumentName = Text;
		printDocument.DefaultPageSettings.Landscape = true;
		using PrintDialog printDialog = new PrintDialog
		{
			Document = printDocument,
			AllowSomePages = true,
			UseEXDialog = true
		};
		printDialog.PrinterSettings.MinimumPage = 1;
		printDialog.PrinterSettings.MaximumPage = _doc.Pages.Count;
		printDialog.PrinterSettings.FromPage = 1;
		printDialog.PrinterSettings.ToPage = _doc.Pages.Count;
		if (printDialog.ShowDialog(this) != DialogResult.OK)
		{
			return;
		}
		int first = ((printDialog.PrinterSettings.PrintRange == PrintRange.SomePages) ? printDialog.PrinterSettings.FromPage : 1);
		int last = ((printDialog.PrinterSettings.PrintRange == PrintRange.SomePages) ? printDialog.PrinterSettings.ToPage : _doc.Pages.Count);
		_printPage = first - 1;
		printDocument.PrintPage += delegate(object s, PrintPageEventArgs pe)
		{
			PdfPage pdfPage = _doc.Pages[_printPage];
			RectangleF rectangleF = pe.PageBounds;
			float num = Math.Min(rectangleF.Width / pdfPage.Width, rectangleF.Height / pdfPage.Height);
			GraphicsState gstate = pe.Graphics.Save();
			pe.Graphics.TranslateTransform((rectangleF.Width - pdfPage.Width * num) / 2f, (rectangleF.Height - pdfPage.Height * num) / 2f);
			Replay(pdfPage, pe.Graphics, num);
			pe.Graphics.Restore(gstate);
			_printPage++;
			pe.HasMorePages = _printPage < last;
		};
		try
		{
			printDocument.Print();
		}
		catch (Exception ex)
		{
			MessageBox.Show(this, "Printing failed: " + ex.Message, AppInfo.Name, MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
		}
	}
}
