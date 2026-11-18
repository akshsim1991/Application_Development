using System;
using System.ComponentModel;
using System.Drawing;
using System.Net;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Shows a new version, downloads it with progress and checks it. "Update now" / "Later".</summary>
public sealed class UpdateDialog : Form
{
	private readonly UpdateManifest _manifest;

	private readonly Button _now = new Button { Text = "Update now" };

	private readonly Button _later = new Button { Text = "Later", DialogResult = DialogResult.Cancel };

	private readonly ProgressBar _bar = new ProgressBar { Visible = false };

	private readonly Label _status = new Label();

	private WebClient _client;

	private string _path;

	/// <summary>Path of the verified installer once "Update now" has finished.</summary>
	public string InstallerPath { get; private set; }

	public UpdateDialog(UpdateManifest manifest)
	{
		_manifest = manifest;
		Text = "PneuSim update";
		FormBorderStyle = FormBorderStyle.FixedDialog;
		StartPosition = FormStartPosition.CenterParent;
		MaximizeBox = false;
		MinimizeBox = false;
		ShowInTaskbar = false;
		Font = new Font("Segoe UI", 9f);
		ClientSize = new Size(470, 300);
		Label value = new Label
		{
			Text = "PneuSim " + manifest.Version + ((!string.IsNullOrEmpty(manifest.CodeName)) ? (" \"" + manifest.CodeName + "\"") : "") + " is available",
			Font = new Font("Segoe UI Semibold", 12f),
			Location = new Point(14, 12),
			Size = new Size(440, 28)
		};
		Label value2 = new Label
		{
			Text = "You have " + AppInfo.Version + ((!string.IsNullOrEmpty(manifest.Released)) ? ("    Released " + manifest.Released) : ""),
			Location = new Point(14, 42),
			Size = new Size(440, 20)
		};
		TextBox value3 = new TextBox
		{
			Multiline = true,
			ReadOnly = true,
			ScrollBars = ScrollBars.Vertical,
			Text = (manifest.Notes ?? "").Replace("\\n", "\r\n"),
			Location = new Point(14, 70),
			Size = new Size(442, 140)
		};
		_status.Location = new Point(14, 218);
		_status.Size = new Size(442, 20);
		_status.Text = "Your work is saved first. PneuSim closes, updates and starts again.";
		_bar.Location = new Point(14, 242);
		_bar.Size = new Size(442, 14);
		_now.Location = new Point(280, 262);
		_now.Size = new Size(100, 30);
		_later.Location = new Point(386, 262);
		_later.Size = new Size(70, 30);
		base.Controls.AddRange(new Control[7] { value, value2, value3, _status, _bar, _now, _later });
		base.CancelButton = _later;
		_now.Click += delegate
		{
			StartDownload();
		};
	}

	private void StartDownload()
	{
		_now.Enabled = false;
		_bar.Visible = true;
		_status.Text = "Downloading…";
		_client = UpdateService.CreateClient();
		_client.DownloadProgressChanged += delegate(object s, DownloadProgressChangedEventArgs e)
		{
			_bar.Value = Math.Max(0, Math.Min(100, e.ProgressPercentage));
		};
		_client.DownloadFileCompleted += delegate(object s, AsyncCompletedEventArgs e)
		{
			if (e.Cancelled)
			{
				return;
			}
			if (e.Error != null)
			{
				_status.Text = "Download failed: " + e.Error.Message;
				_now.Enabled = true;
				_now.Text = "Try again";
				return;
			}
			_status.Text = "Checking the download…";
			if (!UpdateService.Verify(_path, _manifest))
			{
				try
				{
					System.IO.File.Delete(_path);
				}
				catch (Exception)
				{
				}
				_status.Text = "The download did not match its checksum and was discarded. Please try again later.";
				_now.Enabled = true;
				_now.Text = "Try again";
			}
			else
			{
				InstallerPath = _path;
				base.DialogResult = DialogResult.OK;
				Close();
			}
		};
		try
		{
			_path = UpdateService.StartDownload(_manifest, _client);
		}
		catch (Exception ex)
		{
			_status.Text = "Could not start the download: " + ex.Message;
			_now.Enabled = true;
		}
	}

	protected override void OnFormClosing(FormClosingEventArgs e)
	{
		if (base.DialogResult != DialogResult.OK && _client != null && _client.IsBusy)
		{
			_client.CancelAsync();
		}
		base.OnFormClosing(e);
	}
}
