using System;
using System.Drawing;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Friendly error window with a "Copy error report" button. The report stays on this computer unless the user pastes it somewhere.</summary>
public sealed class ErrorDialog : Form
{
	public ErrorDialog(string report, string friendlyMessage)
	{
		Text = AppInfo.Name;
		FormBorderStyle = FormBorderStyle.FixedDialog;
		StartPosition = FormStartPosition.CenterScreen;
		MaximizeBox = false;
		MinimizeBox = false;
		Font = new Font("Segoe UI", 9f);
		ClientSize = new Size(560, 372);
		Label value = new Label
		{
			Text = friendlyMessage,
			Location = new Point(14, 12),
			Size = new Size(532, 96)
		};
		TextBox textBox = new TextBox
		{
			Multiline = true,
			ReadOnly = true,
			ScrollBars = ScrollBars.Both,
			WordWrap = false,
			Text = report,
			Location = new Point(14, 114),
			Size = new Size(532, 190),
			Font = new Font("Consolas", 8.5f)
		};
		Button button = new Button
		{
			Text = "Copy error report",
			Location = new Point(14, 322),
			Size = new Size(140, 30)
		};
		Button button2 = new Button
		{
			Text = "OK",
			DialogResult = DialogResult.OK,
			Location = new Point(466, 322),
			Size = new Size(80, 30)
		};
		button.Click += delegate
		{
			try
			{
				Clipboard.SetText(report);
				button.Text = "Copied";
			}
			catch (Exception)
			{
				button.Text = "Could not copy";
			}
		};
		base.Controls.Add(value);
		base.Controls.Add(textBox);
		base.Controls.Add(button);
		base.Controls.Add(button2);
		base.AcceptButton = button2;
	}
}
