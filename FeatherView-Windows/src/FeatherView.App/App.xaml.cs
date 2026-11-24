using System.IO;
using System.Windows;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Settings;
using FeatherView.Core.Tabs;
using FeatherView.Core.Video;
using FeatherView.Services;
using FeatherView.ViewModels;
using FeatherView.Views;

namespace FeatherView;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        var appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        var log = new FileLog(Path.Combine(appData, "FeatherView", "featherview.log"));
        var version = typeof(App).Assembly.GetName().Version?.ToString(3) ?? "0.0.0";
        log.Info($"Feather View {version} starting");

        DispatcherUnhandledException += (_, a) =>
        {
            log.Error("Unhandled UI error", a.Exception);
            MessageBox.Show("Something unexpected happened, but Feather View kept running. Details were written to the log file.", "Feather View", MessageBoxButton.OK, MessageBoxImage.Warning);
            a.Handled = true;
        };
        AppDomain.CurrentDomain.UnhandledException += (_, a) => log.Error("Fatal error", a.ExceptionObject as Exception);
        TaskScheduler.UnobservedTaskException += (_, a) => { log.Error("Background task error", a.Exception); a.SetObserved(); };

        var settingsService = new SettingsService(SettingsService.DefaultPath(appData), log);
        var settings = settingsService.Load();
        var theme = new ThemeService();
        theme.Apply(settings.Theme);
        var thumbs = new ThumbnailService(settings.ThumbnailSize, settings.ThumbnailCacheItems, settings.ThumbnailParallelism, log);
        var dialogs = new WpfDialogService(theme);
        var history = new PlaybackHistory(Path.Combine(appData, "FeatherView", "playback.json"), log);
        var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var vm = new MainViewModel(settings, settingsService, thumbs, dialogs, theme, log,
            main => new VideoPlayerViewModel(() => main.Settings, dialogs, history, localAppData, log));
        vm.Updates = new UpdateController(dialogs, settingsService, () => vm.Settings, version, log);
        vm.RefreshRecent();

        var window = new MainWindow(vm, theme);
        MainWindow = window;
        window.Show();

        vm.SessionPath = Path.Combine(appData, "FeatherView", "session.json");
        bool restored = false;
        if (settings.RestoreSession && new SessionStore(vm.SessionPath, log).Load() is { Tabs.Count: > 0 } session)
        {
            vm.Tabs.LoadSession(session); restored = true;
        }
        var arg = e.Args.FirstOrDefault(a => !a.StartsWith('-'));
        if (arg != null) { if (restored) vm.Tabs.OpenInNewTab(arg); else vm.OpenPath(arg); }

        Exit += (_, _) => vm.Updates.InstallPendingOnExit();   // a downloaded update is installed when the app closes

        // Quiet daily update check a few seconds after start-up; the user is asked before anything is downloaded.
        _ = Dispatcher.InvokeAsync(async () =>
        {
            await Task.Delay(TimeSpan.FromSeconds(5));
            try { await vm.Updates.CheckAutomaticallyAsync(); } catch (Exception ex) { log.Warn($"Update check skipped ({ex.GetType().Name})."); }
        });
    }
}
