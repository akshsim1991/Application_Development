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

	/// <summary>Called once after start-up: checks about once a day if the user has not switched it off.</summary>
	public void StartDailyUpdateCheck()
	{
		if (AppSettings.CheckForUpdates && DateTime.UtcNow.Ticks - AppSettings.LastUpdateCheckUtcTicks > TimeSpan.FromHours(23.0).Ticks)
		{
			RunUpdateCheck(manual: false);
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
		if (_running && !manual)
		{
			_statusMessage.Text = "A new version of PneuSim is available (" + manifest.Version + "). Use Help > Update to new version.";
			return;
		}
		using UpdateDialog updateDialog = new UpdateDialog(manifest);
		Theme.Apply(updateDialog);
		if (updateDialog.ShowDialog(this) == DialogResult.OK && updateDialog.InstallerPath != null)
		{
			InstallUpdate(updateDialog.InstallerPath);
		}
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
