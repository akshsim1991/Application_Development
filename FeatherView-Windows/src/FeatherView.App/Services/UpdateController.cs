using System.Diagnostics;
using System.IO;
using System.Net.Http;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Settings;
using FeatherView.Core.Updates;

namespace FeatherView.Services;

/// <summary>
/// Automatic updates in the same style as FeatherPDF/FileGlade: at most one check a day (when enabled), the user is asked
/// before anything is downloaded, the installer is verified (size + SHA-256) and only then started.
/// </summary>
public sealed class UpdateController
{
    private readonly IDialogService _dialogs;
    private readonly SettingsService _settingsService;
    private readonly ILog _log;
    private readonly HttpClient _http;
    private readonly Func<AppSettings> _settings;
    private readonly string _currentVersion;
    private bool _busy;

    public UpdateController(IDialogService dialogs, SettingsService svc, Func<AppSettings> settings, string currentVersion, ILog log)
    {
        _dialogs = dialogs; _settingsService = svc; _settings = settings; _currentVersion = currentVersion; _log = log;
        _http = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
        _http.DefaultRequestHeaders.UserAgent.ParseAdd($"FeatherView/{currentVersion}");
    }

    /// <summary>Runs the daily background check when the user has not turned it off.</summary>
    public async Task CheckAutomaticallyAsync()
    {
        var s = _settings();
        if (!s.AutoUpdateCheck) return;
        if (s.LastUpdateCheckUtc is DateTime last && DateTime.UtcNow - last < TimeSpan.FromHours(24)) return;
        await CheckAsync(userInitiated: false);
    }

    public async Task CheckAsync(bool userInitiated)
    {
        if (_busy) return;
        _busy = true;
        try
        {
            var svc = new UpdateService(_http, log: _log);
            UpdateManifest? m;
            try { m = await svc.CheckAsync(_currentVersion); }
            catch (UpdateException ex)
            {
                if (userInitiated) _dialogs.Error("Check for updates", ex.Message + " Check your internet connection and try again.");
                return;
            }
            finally { var s = _settings(); s.LastUpdateCheckUtc = DateTime.UtcNow; _settingsService.Save(s); }

            if (m == null)
            {
                if (userInitiated) _dialogs.Info("Check for updates", $"Feather View {_currentVersion} is the latest version.");
                return;
            }
            if (!userInitiated && string.Equals(_settings().SkippedVersion, m.Version, StringComparison.OrdinalIgnoreCase)) return;

            var asset = m.PickInstaller(IsFrameworkDependentInstall());
            switch (_dialogs.AskUpdate(m, _currentVersion, asset?.Size))
            {
                case UpdateChoice.SkipVersion: { var s = _settings(); s.SkippedVersion = m.Version; _settingsService.Save(s); return; }
                case UpdateChoice.Later: return;
            }
            await DownloadAndInstallAsync(svc, m, asset);
        }
        finally { _busy = false; }
    }

    private static bool IsFrameworkDependentInstall()
    {
        try { return InstallFlavor.IsFrameworkDependent(File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "FeatherView.runtimeconfig.json"))); }
        catch { return false; }
    }

    private async Task DownloadAndInstallAsync(UpdateService svc, UpdateManifest m, UpdateAsset? asset)
    {
        var cts = new CancellationTokenSource();
        var progress = _dialogs.ShowDownloadProgress($"Downloading Feather View {m.Version}", () => cts.Cancel());
        string? file = null;
        try
        {
            file = await svc.DownloadAsync(m, Path.Combine(Path.GetTempPath(), "FeatherViewUpdate"), progress, cts.Token, asset);
        }
        catch (OperationCanceledException) { return; }
        catch (UpdateException ex) { _dialogs.CloseDownloadProgress(); _dialogs.Error("Update", ex.Message); return; }
        catch (Exception ex) { _log.Warn($"Update download failed ({ex.GetType().Name})."); _dialogs.CloseDownloadProgress(); _dialogs.Error("Update", "The update could not be downloaded."); return; }
        _dialogs.CloseDownloadProgress();

        try
        {
            // Inno Setup silent upgrade: closes this app, installs over it and starts it again.
            Process.Start(new ProcessStartInfo(file, "/SILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS") { UseShellExecute = true });
            System.Windows.Application.Current.Shutdown();
        }
        catch (Exception ex) { _log.Warn($"Update launch failed ({ex.GetType().Name})."); _dialogs.Error("Update", "The installer could not be started."); }
    }
}
