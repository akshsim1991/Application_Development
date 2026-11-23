using FeatherView.Core.Settings;
using FeatherView.Core.Updates;

namespace FeatherView.Services;

public enum UpdateChoice { UpdateNow, Later, SkipVersion }

/// <summary>Everything the view-models need from the UI, behind an interface so logic stays out of windows.</summary>
public interface IDialogService
{
    bool Confirm(string title, string message, string confirmText = "OK");
    void Info(string title, string message);
    void Error(string title, string message);
    string? PickFile();
    string? PickFolder();
    AppSettings? ShowSettings(AppSettings current, Func<Task> checkUpdatesNow);
    UpdateChoice AskUpdate(UpdateManifest manifest, string currentVersion);
    IProgress<double> ShowDownloadProgress(string title, Action cancel);
    void CloseDownloadProgress();
}
