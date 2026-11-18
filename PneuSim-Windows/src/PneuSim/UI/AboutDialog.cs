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
		ClientSize = new Size(520, 470);
		RichTextBox richTextBox = new RichTextBox
		{
			ReadOnly = true,
			BorderStyle = BorderStyle.FixedSingle,
			Location = new Point(14, 14),
			Size = new Size(492, 370),
			BackColor = SystemColors.Window,
			DetectUrls = false,
			TabStop = false
		};
		AddTitle(richTextBox, AppInfo.Name, 20f);
		AddLine(richTextBox, "Version " + AppInfo.Version + " \"" + AppInfo.CodeName + "\", built " + AppInfo.BuildDate.ToString("d MMMM yyyy", CultureInfo.InvariantCulture), bold: true);
		AddLine(richTextBox, "");
		AddLine(richTextBox, "Pneumatic, electro-pneumatic and hydraulic circuit design and simulation. Draw a circuit, run it, check it for mistakes and see how it works before you build it.");
		AddLine(richTextBox, "");
		AddLine(richTextBox, "Created by: " + AppInfo.Developer, bold: true);
		AddLine(richTextBox, "");
		AddHeading(richTextBox, "DEVELOPMENT HISTORY");
		AddLine(richTextBox, "PneuSim began as a Visual Basic .NET application. From version 3.2 its source was recovered from the released program and development continued in C# on .NET Framework 4.7.2, with the same simulation behaviour verified automatically against version 3.2.0.");
		AddLine(richTextBox, "");
		AddHeading(richTextBox, "DEVELOPMENT AND AI CONTRIBUTIONS");
		AddLine(richTextBox, "PneuSim is designed and directed by " + AppInfo.Developer + ": the features, the engineering rules, the look and the decisions are the developer's. AI coding assistance (Claude, by Anthropic) was used to write and test program code under the developer's direction and review.");
		AddLine(richTextBox, "");
		AddHeading(richTextBox, "SYMBOLS");
		AddLine(richTextBox, "Circuit symbols are drawn by PneuSim and are based on ISO 1219 and IEC 60617 conventions. PneuSim is not endorsed by, or affiliated with, any standards body or component manufacturer.");
		AddLine(richTextBox, "");
		AddLine(richTextBox, "© 2026 " + AppInfo.Developer + ". All rights reserved.");
		richTextBox.Select(0, 0);
		_mail.Location = new Point(14, 394);
		Button button = new Button
		{
			Text = "Close",
			DialogResult = DialogResult.OK,
			Location = new Point(344, 428),
			Size = new Size(80, 28)
		};
		Button button2 = new Button
		{
			Text = "Cancel",
			DialogResult = DialogResult.Cancel,
			Location = new Point(430, 428),
			Size = new Size(76, 28)
		};
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
