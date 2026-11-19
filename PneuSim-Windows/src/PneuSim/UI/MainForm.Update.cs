using System;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PneuSim;

public partial class MainForm
{
	private bool _updateBusy;

	/// <summary>Help > Update to new version: asks the update server and offers the update.</summary>
	private void OnUpdateCheck(object sender, EventArgs e)
	{
		RunUpdateCheck(manual: true);
	}

	private ToolStripStatusLabel _statusUpdate;

	private UpdateManifest _knownUpdate;

	private string _pendingInstaller;

	/// <summary>Called once after start-up: asks the update server every time PneuSim starts (when a network is connected). Failures are silent.</summary>
	public void StartDailyUpdateCheck()
	{
		if (!string.IsNullOrEmpty(AppSettings.PendingUpdateVersion))
		{
			if (UpdateService.IsNewer(AppSettings.PendingUpdateVersion, AppInfo.Version))
			{
				UpdateService.Log("The automatic update to " + AppSettings.PendingUpdateVersion + " did not complete; still running " + AppInfo.Version + ". It will be offered again.");
			}
			else
			{
				UpdateService.Log("Automatic update to " + AppSettings.PendingUpdateVersion + " completed; running " + AppInfo.Version + ".");
			}
			AppSettings.PendingUpdateVersion = "";
			AppSettings.Save();
		}
		if (AppSettings.CheckForUpdates || AppSettings.AutoUpdate)
		{
			RunUpdateCheck(manual: false);
		}
	}

	private void ShowUpdateLink(string text)
	{
		if (_statusUpdate == null)
		{
			_statusUpdate = new ToolStripStatusLabel
			{
				IsLink = true,
				LinkBehavior = LinkBehavior.HoverUnderline
			};
			_statusUpdate.Click += delegate
			{
				if (_knownUpdate != null && _pendingInstaller == null)
				{
					OfferUpdate(_knownUpdate);
				}
			};
			_status.Items.Insert(2, _statusUpdate);
		}
		_statusUpdate.Text = text;
		_statusUpdate.Visible = true;
	}

	private void OfferUpdate(UpdateManifest manifest)
	{
		using UpdateDialog updateDialog = new UpdateDialog(manifest);
		Theme.Apply(updateDialog);
		if (updateDialog.ShowDialog(this) == DialogResult.OK && updateDialog.InstallerPath != null)
		{
			InstallUpdate(updateDialog.InstallerPath);
		}
	}

	/// <summary>Automatic updates: downloads the installer quietly, checks its SHA-256, and keeps it until PneuSim is closed.</summary>
	private void StartBackgroundUpdate(UpdateManifest manifest)
	{
		try
		{
			System.Net.WebClient client = UpdateService.CreateClient();
			string path = UpdateService.StartDownload(manifest, client);
			ShowUpdateLink("Downloading update " + manifest.Version + "…");
			client.DownloadFileCompleted += delegate(object s, System.ComponentModel.AsyncCompletedEventArgs e)
			{
				try
				{
					client.Dispose();
					if (e.Error == null && !e.Cancelled && UpdateService.Verify(path, manifest))
					{
						_pendingInstaller = path;
						UpdateService.Log("Downloaded and verified update " + manifest.Version + "; it will be installed when PneuSim is closed.");
						AppSettings.PendingUpdateVersion = manifest.Version;
						AppSettings.Save();
						ShowUpdateLink("Update " + manifest.Version + " ready: installs when you close PneuSim");
					}
					else
					{
						UpdateService.Log("Download of update " + manifest.Version + " failed or did not match its checksum" + ((e.Error != null) ? (": " + e.Error.Message) : "") + ". The file was discarded.");
						_statusUpdate.Visible = false;
						try
						{
							System.IO.File.Delete(path);
						}
						catch (Exception)
						{
						}
					}
				}
				catch (Exception)
				{
				}
			};
		}
		catch (Exception)
		{
		}
	}

	/// <summary>Called when PneuSim has closed: starts a downloaded automatic update silently, after PneuSim has fully exited.</summary>
	private void RunPendingUpdate()
	{
		if (_pendingInstaller == null || !AppSettings.AutoUpdate || !System.IO.File.Exists(_pendingInstaller))
		{
			return;
		}
		try
		{
			string args = "/c timeout /t 3 /nobreak >nul & \"" + _pendingInstaller + "\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /NORESTARTAPPLICATIONS";
			UpdateService.Log("Starting the silent installer " + _pendingInstaller);
			Process.Start(new ProcessStartInfo("cmd.exe", args)
			{
				CreateNoWindow = true,
				UseShellExecute = false,
				WindowStyle = ProcessWindowStyle.Hidden
			});
		}
		catch (Exception)
		{
		}
	}

	private void RunUpdateCheck(bool manual)
	{
		if (_updateBusy)
		{
			return;
		}
		_updateBusy = true;
		if (manual)
		{
			_statusMessage.Text = "Checking for a new version…";
		}
		Task.Run(delegate
		{
			UpdateManifest manifest = null;
			Exception error = null;
			try
			{
				manifest = UpdateService.CheckForNewer();
			}
			catch (Exception ex)
			{
				error = ex;
			}
			try
			{
				BeginInvoke(new Action(delegate
				{
					_updateBusy = false;
					if (error == null)
					{
						AppSettings.LastUpdateCheckUtcTicks = DateTime.UtcNow.Ticks;
						AppSettings.Save();
					}
					OnUpdateResult(manifest, error, manual);
				}));
			}
			catch (Exception)
			{
				_updateBusy = false;
			}
		});
	}

	private void OnUpdateResult(UpdateManifest manifest, Exception error, bool manual)
	{
		if (error != null)
		{
			if (manual)
			{
				_statusMessage.Text = "Could not check for updates.";
				MessageBox.Show(this, "PneuSim could not reach the update server. Check your internet connection and try again.\r\n\r\n" + error.Message, AppInfo.Name, MessageBoxButtons.OK, MessageBoxIcon.Information);
			}
			return;
		}
		if (manifest == null)
		{
			if (manual)
			{
				_statusMessage.Text = "PneuSim is up to date.";
				MessageBox.Show(this, "You have the latest version (" + AppInfo.Version + ").", AppInfo.Name, MessageBoxButtons.OK, MessageBoxIcon.Information);
			}
			return;
		}
		_knownUpdate = manifest;
		if (!manual && AppSettings.AutoUpdate)
		{
			if (_pendingInstaller == null)
			{
				StartBackgroundUpdate(manifest);
			}
			return;
		}
		ShowUpdateLink("Update available: " + manifest.Version + " (click to install)");
		if (_running && !manual)
		{
			_statusMessage.Text = "A new version of PneuSim is available (" + manifest.Version + "). Click the update link at the bottom right.";
			return;
		}
		OfferUpdate(manifest);
	}

	/// <summary>Saves work (asks first if needed), starts the silent installer and closes PneuSim.</summary>
	private void InstallUpdate(string installerPath)
	{
		if (!ConfirmDiscard())
		{
			_statusMessage.Text = "Update cancelled. The installer is ready: " + installerPath;
			return;
		}
		_dirty = false;
		try
		{
			Process.Start(new ProcessStartInfo(installerPath, UpdateService.InstallerArguments)
			{
				UseShellExecute = true
			});
		}
		catch (Exception ex)
		{
			MessageBox.Show(this, "Could not start the installer: " + ex.Message + "\r\n\r\nYou can run it yourself: " + installerPath, AppInfo.Name, MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
			return;
		}
		Close();
	}
}
