using System.Windows;

namespace FeatherView.Views;

public partial class MessageWindow : Window
{
    public MessageWindow(string title, string message, string okText, bool showCancel)
    {
        InitializeComponent();
        Title = title; TitleText.Text = title; BodyText.Text = message; OkButton.Content = okText;
        CancelButton.Visibility = showCancel ? Visibility.Visible : Visibility.Collapsed;
    }
    private void Ok_Click(object sender, RoutedEventArgs e) => DialogResult = true;
    private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;
}
