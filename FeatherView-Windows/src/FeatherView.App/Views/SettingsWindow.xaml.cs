using System.Diagnostics;
using System.Windows;
using FeatherView.Core.Settings;

namespace FeatherView.Views;

public partial class SettingsWindow : Window
{
    private readonly Func<Task> _checkNow;
    public AppSettings Edited { get; }

    public SettingsWindow(AppSettings settings, Func<Task> checkNow)
    {
        InitializeComponent();
        Edited = settings; _checkNow = checkNow; DataContext = settings;
    }

    private void Save_Click(object sender, RoutedEventArgs e) => DialogResult = true;
    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;
    private async void CheckNow_Click(object sender, RoutedEventArgs e)
    {
        var b = (System.Windows.Controls.Button)sender; b.IsEnabled = false;
        try { await _checkNow(); } finally { b.IsEnabled = true; }
    }
    private void DefaultApps_Click(object sender, RoutedEventArgs e)
    {
        try { Process.Start(new ProcessStartInfo("ms-settings:defaultapps") { UseShellExecute = true }); } catch { }
    }
}
