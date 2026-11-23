using System.Windows;

namespace FeatherView.Views;

public partial class PromptWindow : Window
{
    public string Value => Input.Text.Trim();
    public PromptWindow(string title, string message, string defaultValue)
    {
        InitializeComponent();
        Title = title; TitleText.Text = title; BodyText.Text = message; Input.Text = defaultValue;
        Loaded += (_, _) => { Input.Focus(); Input.SelectAll(); };
    }
    private void Ok_Click(object sender, RoutedEventArgs e) => DialogResult = true;
    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;
}
