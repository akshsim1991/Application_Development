using System.Windows;
using FeatherView.Core.Updates;
using FeatherView.Services;

namespace FeatherView.Views;

public partial class UpdateWindow : Window
{
    public UpdateChoice Choice { get; private set; } = UpdateChoice.Later;

    public UpdateWindow(UpdateManifest m, string current, long? downloadBytes)
    {
        InitializeComponent();
        Headline.Text = $"Feather View {m.Version} is available";
        var size = downloadBytes is long b ? (b >= 1 << 20 ? $"{b / 1048576.0:0.#} MB" : $"{Math.Max(1, b / 1024)} KB") : null;
        Detail.Text = $"You have version {current}." + (size != null ? $" Download size: {size}." : "") + " Updating takes a few seconds and the app restarts itself.";
        Notes.Text = string.IsNullOrWhiteSpace(m.Notes) ? "" : m.Notes;
    }
    private void Now_Click(object s, RoutedEventArgs e) { Choice = UpdateChoice.UpdateNow; DialogResult = true; }
    private void Later_Click(object s, RoutedEventArgs e) { Choice = UpdateChoice.Later; DialogResult = false; }
    private void Skip_Click(object s, RoutedEventArgs e) { Choice = UpdateChoice.SkipVersion; DialogResult = false; }
}
