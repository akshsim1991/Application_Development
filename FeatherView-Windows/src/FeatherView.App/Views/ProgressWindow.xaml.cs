using System.Windows;

namespace FeatherView.Views;

public partial class ProgressWindow : Window
{
    private readonly Action _cancel;
    public ProgressWindow(string title, Action cancel)
    {
        InitializeComponent();
        Title = title; TitleText.Text = title; _cancel = cancel;
    }
    public void SetProgress(double v) => Bar.Value = Math.Clamp(v, 0, 1);
    private void Cancel_Click(object sender, RoutedEventArgs e) { _cancel(); Close(); }
}
