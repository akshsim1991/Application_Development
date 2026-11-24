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
        if (s.LastUpdateCheckUtc is DateTime last && DateTime.UtcNow - last < TimeSpan.FromHours(6)) return;
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
            try { m = await CheckWithRetryAsync(svc); }
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

            // Already downloaded earlier (background download): install straight away, no second download
            if (HavePending(m.Version) is string ready)
            {
                if (userInitiated || !_settings().AutoInstallUpdates)
                {
                    if (_dialogs.AskUpdate(m, _currentVersion, asset?.Size) == UpdateChoice.UpdateNow) LaunchInstaller(ready, restart: true);
                }
                return;
            }

            // Background mode: download quietly, then install on exit (or ask)
            if (!userInitiated && _settings().AutoDownloadUpdates)
            {
                await BackgroundDownloadAsync(svc, m, asset);
                return;
            }

            switch (_dialogs.AskUpdate(m, _currentVersion, asset?.Size))
            {
                case UpdateChoice.SkipVersion: { var s = _settings(); s.SkippedVersion = m.Version; _settingsService.Save(s); return; }
                case UpdateChoice.Later: return;
            }
            await DownloadAndInstallAsync(svc, m, asset);
        }
        finally { _busy = false; }
    }

    private async Task<UpdateManifest?> CheckWithRetryAsync(UpdateService svc)
    {
        // A release that is being published can be unreachable for a few seconds: try again before reporting an error
        for (int attempt = 0; ; attempt++)
        {
            try { return await svc.CheckAsync(_currentVersion); }
            catch (UpdateException) when (attempt < 3) { await Task.Delay(TimeSpan.FromSeconds(2 * (attempt + 1))); }
        }
    }

    private string? HavePending(string version)
    {
        var s = _settings();
        if (!string.Equals(s.PendingUpdateVersion, version, StringComparison.OrdinalIgnoreCase) || string.IsNullOrEmpty(s.PendingUpdatePath) || !File.Exists(s.PendingUpdatePath)) return null;
        return s.PendingUpdatePath;
    }

    private async Task BackgroundDownloadAsync(UpdateService svc, UpdateManifest m, UpdateAsset? asset)
    {
        try
        {
            var file = await svc.DownloadAsync(m, Path.Combine(Path.GetTempPath(), "FeatherViewUpdate"), null, CancellationToken.None, asset);
            var s = _settings();
            s.PendingUpdateVersion = m.Version; s.PendingUpdatePath = file; s.PendingUpdateSha256 = asset?.Sha256;
            _settingsService.Save(s);
            _log.Info($"Update {m.Version} downloaded and verified.");
            if (!s.AutoInstallUpdates)
            {
                var choice = _dialogs.AskUpdate(m, _currentVersion, asset?.Size);
                if (choice == UpdateChoice.UpdateNow) LaunchInstaller(file, restart: true);
                else if (choice == UpdateChoice.SkipVersion) { s.SkippedVersion = m.Version; _settingsService.Save(s); }
            }
        }
        catch (Exception ex) { _log.Warn($"Background update download failed ({ex.GetType().Name})."); }
    }

    /// <summary>Called when Feather View closes: installs a verified, already downloaded update silently (no restart, the app is closed).</summary>
    public void InstallPendingOnExit()
    {
        try
        {
            var s = _settings();
            if (!s.AutoInstallUpdates || string.IsNullOrEmpty(s.PendingUpdatePath) || string.IsNullOrEmpty(s.PendingUpdateVersion) || !File.Exists(s.PendingUpdatePath)) return;
            if (!VersionUtil.IsNewer(s.PendingUpdateVersion, _currentVersion)) { ClearPending(); return; }
            if (!string.IsNullOrEmpty(s.PendingUpdateSha256))
            {
                using var fs = File.OpenRead(s.PendingUpdatePath);
                if (!Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(fs)).Equals(s.PendingUpdateSha256, StringComparison.OrdinalIgnoreCase)) { ClearPending(); return; }
            }
            var path = s.PendingUpdatePath; ClearPending();
            Process.Start(new ProcessStartInfo(path, "/SILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS") { UseShellExecute = true });
        }
        catch (Exception ex) { _log.Warn($"Installing the pending update failed ({ex.GetType().Name})."); }
    }

    private void ClearPending() { var s = _settings(); s.PendingUpdateVersion = null; s.PendingUpdatePath = null; s.PendingUpdateSha256 = null; _settingsService.Save(s); }

    private void LaunchInstaller(string file, bool restart)
    {
        try
        {
            ClearPending();
            Process.Start(new ProcessStartInfo(file, "/SILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS" + (restart ? " /RESTARTAPPLICATIONS" : " /NORESTART")) { UseShellExecute = true });
            System.Windows.Application.Current.Shutdown();
        }
        catch (Exception ex) { _log.Warn($"Update launch failed ({ex.GetType().Name})."); _dialogs.Error("Update", "The installer could not be started."); }
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
            ClearPending();
            Process.Start(new ProcessStartInfo(file, "/SILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS") { UseShellExecute = true });
            System.Windows.Application.Current.Shutdown();
        }
        catch (Exception ex) { _log.Warn($"Update launch failed ({ex.GetType().Name})."); _dialogs.Error("Update", "The installer could not be started."); }
    }
}
