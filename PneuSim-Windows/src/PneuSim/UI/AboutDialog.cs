using System;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>About box: scrolling text, optional "write to the developer" e-mail.</summary>
public sealed class AboutDialog : Form
{
	private readonly CheckBox _mail = new CheckBox
	{
		Text = "Write to the developer now (opens your e-mail program)",
		AutoSize = true
	};

	public AboutDialog()
	{
		Text = "About " + AppInfo.Name;
		FormBorderStyle = FormBorderStyle.FixedDialog;
		StartPosition = FormStartPosition.CenterParent;
		MaximizeBox = false;
		MinimizeBox = false;
		ShowInTaskbar = false;
		Font = new Font("Segoe UI", 9f);
		ClientSize = new Size(520, 580);
		RichTextBox richTextBox = new RichTextBox
		{
			ReadOnly = true,
			BorderStyle = BorderStyle.FixedSingle,
			Location = new Point(14, 150),
			Size = new Size(492, 330),
			BackColor = SystemColors.Window,
			DetectUrls = false,
			TabStop = false
		};
		AddLine(richTextBox, "Pneumatic, electro-pneumatic and hydraulic circuit design and simulation. Draw a circuit, run it, check it for mistakes and see how it works before you build it.");
		AddLine(richTextBox, "");
		AddLine(richTextBox, "Created by: " + AppInfo.Developer, bold: true);
		AddLine(richTextBox, "");
		AddHeading(richTextBox, "DEVELOPMENT HISTORY");
		AddLine(richTextBox, "PneuSim began as a Visual Basic .NET application. From version 3.2 its source was recovered from the released program and development continued in C# on .NET Framework 4.7.2, with the same simulation behaviour verified automatically against version 3.2.0.");
		AddLine(richTextBox, "");
		AddHeading(richTextBox, "DEVELOPMENT");
		AddLine(richTextBox, "PneuSim is designed, developed and tested by " + AppInfo.Developer + ". The features, the engineering rules, the look and every design decision are the developer's own.");
		AddLine(richTextBox, "");
		AddHeading(richTextBox, "SYMBOLS");
		AddLine(richTextBox, "Circuit symbols are drawn by PneuSim and are based on ISO 1219 and IEC 60617 conventions. PneuSim is not endorsed by, or affiliated with, any standards body or component manufacturer.");
		AddLine(richTextBox, "");
		AddLine(richTextBox, "© 2026 " + AppInfo.Developer + ". All rights reserved.");
		richTextBox.Select(0, 0);
		_mail.Location = new Point(14, 492);
		Button button = new Button
		{
			Text = "Close",
			DialogResult = DialogResult.OK,
			Location = new Point(344, 534),
			Size = new Size(80, 28)
		};
		Button button2 = new Button
		{
			Text = "Cancel",
			DialogResult = DialogResult.Cancel,
			Location = new Point(430, 534),
			Size = new Size(76, 28)
		};
		PictureBox pictureBox = new PictureBox
		{
			Location = new Point(24, 16),
			Size = new Size(120, 120),
			SizeMode = PictureBoxSizeMode.Zoom,
			Image = LoadLogo(120)
		};
		Label label = new Label
		{
			Text = AppInfo.Name,
			Font = new Font("Segoe UI Semibold", 30f),
			ForeColor = Color.FromArgb(0, 118, 132),
			AutoSize = true,
			Location = new Point(158, 22)
		};
		Label label2 = new Label
		{
			Text = "Version " + AppInfo.Version + " \"" + AppInfo.CodeName + "\"\r\nBuilt " + AppInfo.BuildDate.ToString("d MMMM yyyy", CultureInfo.InvariantCulture),
			Font = new Font("Segoe UI", 10.5f),
			AutoSize = true,
			Location = new Point(162, 84)
		};
		base.Controls.Add(pictureBox);
		base.Controls.Add(label);
		base.Controls.Add(label2);
		base.Controls.Add(richTextBox);
		base.Controls.Add(_mail);
		base.Controls.Add(button);
		base.Controls.Add(button2);
		base.AcceptButton = button;
		base.CancelButton = button2;
		button.Click += delegate
		{
			if (_mail.Checked)
			{
				OpenMail();
			}
		};
	}

	/// <summary>The program icon at high quality (the 256 pixel image inside app.ico), scaled smoothly.</summary>
	private static Image LoadLogo(int size)
	{
		try
		{
			using System.IO.Stream stream = System.Reflection.Assembly.GetExecutingAssembly().GetManifestResourceStream("app.ico");
			using Icon icon = new Icon(stream, 256, 256);
			using Bitmap bitmap = icon.ToBitmap();
			Bitmap bitmap2 = new Bitmap(size, size);
			using Graphics graphics = Graphics.FromImage(bitmap2);
			graphics.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
			graphics.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;
			graphics.SmoothingMode = System.Drawing.Drawing2D.SmoothingMode.HighQuality;
			graphics.DrawImage(bitmap, 0, 0, size, size);
			return bitmap2;
		}
		catch (Exception)
		{
			return null;
		}
	}

	private static void AddTitle(RichTextBox box, string text, float size)
	{
		box.SelectionFont = new Font("Segoe UI Semibold", size);
		box.AppendText(text + "\n");
	}

	private static void AddHeading(RichTextBox box, string text)
	{
		box.SelectionFont = new Font("Segoe UI", 9f, FontStyle.Bold);
		box.AppendText(text + "\n");
	}

	private static void AddLine(RichTextBox box, string text, bool bold = false)
	{
		box.SelectionFont = new Font("Segoe UI", 9f, bold ? FontStyle.Bold : FontStyle.Regular);
		box.AppendText(text + "\n");
	}

	private static void OpenMail()
	{
		try
		{
			string fileName = "mailto:" + AppInfo.DeveloperEmail + "?subject=" + Uri.EscapeDataString(AppInfo.Full + " feedback");
			Process.Start(new ProcessStartInfo(fileName)
			{
				UseShellExecute = true
			});
		}
		catch (Exception)
		{
			MessageBox.Show("Could not open your e-mail program. You can write to " + AppInfo.DeveloperEmail + ".", AppInfo.Name, MessageBoxButtons.OK, MessageBoxIcon.Information);
		}
	}
}
