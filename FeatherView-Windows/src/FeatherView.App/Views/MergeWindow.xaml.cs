using System.Windows;
using FeatherView.Services;
using FeatherView.ViewModels;

namespace FeatherView.Views;

public partial class MergeWindow : Window
{
    private readonly MergeViewModel _vm;
    public MergeWindow(MergeViewModel vm)
    {
        InitializeComponent();
        DataContext = _vm = vm;
        Closing += (_, e) => { if (_vm.Running && !ConfirmStop()) e.Cancel = true; else _vm.CancelCommand.Execute(null); };
    }

    private bool ConfirmStop() => MessageBox.Show(this, "A merge is running. Stop it?", "Merge videos", MessageBoxButton.YesNo, MessageBoxImage.Question) == MessageBoxResult.Yes;
    private MergeItem? Selected => List.SelectedItem as MergeItem;
    private void Up_Click(object sender, RoutedEventArgs e) { var s = Selected; _vm.MoveUpCommand.Execute(s); List.SelectedItem = s; }
    private void Down_Click(object sender, RoutedEventArgs e) { var s = Selected; _vm.MoveDownCommand.Execute(s); List.SelectedItem = s; }
    private void Remove_Click(object sender, RoutedEventArgs e) => _vm.RemoveCommand.Execute(Selected);
    private void ModeAuto_Checked(object sender, RoutedEventArgs e) { if (_vm != null) _vm.ModeIndex = 0; }
    private void ModeRe_Checked(object sender, RoutedEventArgs e) { if (_vm != null) _vm.ModeIndex = 1; }
    private void Show_Click(object sender, RoutedEventArgs e) { if (_vm.OutputPath != null) FileOperations.ShowInExplorer(_vm.OutputPath); }
    private void Play_Click(object sender, RoutedEventArgs e)
    {
        if (_vm.OutputPath == null) return;
        try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(_vm.OutputPath) { UseShellExecute = true }); } catch { }
    }
    private void Close_Click(object sender, RoutedEventArgs e) => Close();
}
