using System.Windows;
using FeatherView.Core.Media;
using FeatherView.Core.Settings;
using FeatherView.Core.Updates;
using FeatherView.Views;
using Microsoft.Win32;

namespace FeatherView.Services;

public sealed class WpfDialogService : IDialogService
{
    private readonly ThemeService _theme;
    private ProgressWindow? _progress;

    public WpfDialogService(ThemeService theme) { _theme = theme; }

    private static Window? Owner => Application.Current?.MainWindow is { IsVisible: true } w ? w : null;

    private T Prepare<T>(T w) where T : Window { w.Owner = Owner; _theme.ApplyToWindow(w); return w; }

    public bool Confirm(string title, string message, string confirmText = "OK") =>
        Prepare(new MessageWindow(title, message, confirmText, showCancel: true)).ShowDialog() == true;

    public void Info(string title, string message) => Prepare(new MessageWindow(title, message, "OK", showCancel: false)).ShowDialog();
    public void Error(string title, string message) => Prepare(new MessageWindow(title, message, "OK", showCancel: false)).ShowDialog();

    public string? PickFile()
    {
        var exts = string.Join(";", MediaTypes.ImageExtensions.Concat(MediaTypes.VideoExtensions).Select(e => "*" + e));
        var dlg = new OpenFileDialog { Title = "Open photo or video", Filter = $"Photos and videos|{exts}|All files|*.*" };
        return dlg.ShowDialog(Owner) == true ? dlg.FileName : null;
    }

    public string? PickFile(string title, string filter)
    {
        var dlg = new OpenFileDialog { Title = title, Filter = filter };
        return dlg.ShowDialog(Owner) == true ? dlg.FileName : null;
    }

    public string? Prompt(string title, string message, string defaultValue)
    {
        var w = Prepare(new PromptWindow(title, message, defaultValue));
        return w.ShowDialog() == true ? w.Value : null;
    }

    public string? PickFolder()
    {
        var dlg = new OpenFolderDialog { Title = "Open folder" };
        return dlg.ShowDialog(Owner) == true ? dlg.FolderName : null;
    }

    public AppSettings? ShowSettings(AppSettings current, Func<Task> checkUpdatesNow, string engineStatus, Func<Task> installEngine)
    {
        var w = Prepare(new SettingsWindow(current, checkUpdatesNow, engineStatus, installEngine));
        return w.ShowDialog() == true ? w.Edited : null;
    }

    public UpdateChoice AskUpdate(UpdateManifest manifest, string currentVersion)
    {
        var w = Prepare(new UpdateWindow(manifest, currentVersion));
        w.ShowDialog();
        return w.Choice;
    }

    public IProgress<double> ShowDownloadProgress(string title, Action cancel)
    {
        _progress = Prepare(new ProgressWindow(title, cancel));
        _progress.Show();
        var p = _progress;
        return new Progress<double>(v => p.SetProgress(v));
    }

    public void CloseDownloadProgress() { _progress?.Close(); _progress = null; }
}
