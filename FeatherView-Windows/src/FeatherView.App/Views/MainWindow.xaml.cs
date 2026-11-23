using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using FeatherView.Services;
using FeatherView.ViewModels;

namespace FeatherView.Views;

/// <summary>View-only code: window chrome, drag-and-drop, wiring the zoom surface and gallery scrolling to the view-model.</summary>
public partial class MainWindow : Window
{
    private readonly MainViewModel _vm;
    private readonly ThemeService _theme;
    private WindowState _stateBeforeFullscreen;

    public MainWindow(MainViewModel vm, ThemeService theme)
    {
        _vm = vm; _theme = theme;
        InitializeComponent();
        DataContext = vm;
        RestoreWindowBounds();

        vm.Viewport = Viewer;
        Viewer.ZoomChanged += vm.ReportZoom;
        vm.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(MainViewModel.IsFullscreen)) ApplyFullscreen(vm.IsFullscreen); };
        vm.Gallery.ScrollToRow += row =>
        {
            if (row >= 0 && row < vm.Gallery.Rows.Count) GalleryList.ScrollIntoView(vm.Gallery.Rows[row]);
        };
        GalleryList.SizeChanged += (_, e) => vm.Gallery.SetViewportWidth(e.NewSize.Width - SystemParameters.VerticalScrollBarWidth - 20);
        SourceInitialized += (_, _) => theme.ApplyToWindow(this);
        Closing += (_, _) => SaveBounds();
        Closed += (_, _) => vm.Shutdown();
        Drop += OnDrop;
        DragOver += (_, e) => { e.Effects = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None; e.Handled = true; };
    }

    private void OnDrop(object sender, DragEventArgs e)
    {
        if (e.Data.GetData(DataFormats.FileDrop) is string[] paths && paths.Length > 0) _vm.OpenPath(paths[0]);
    }

    private void RecentButton_Click(object sender, RoutedEventArgs e)
    {
        _vm.RefreshRecent();
        if (RecentButton.ContextMenu is { } m) { m.PlacementTarget = RecentButton; m.Placement = System.Windows.Controls.Primitives.PlacementMode.Bottom; m.IsOpen = true; }
    }

    private void Tile_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: MediaItemViewModel item })
        {
            if (e.ClickCount >= 2) _vm.OpenItemCommand.Execute(item); else _vm.SelectItemCommand.Execute(item);
            e.Handled = true;
        }
    }

    private void ApplyFullscreen(bool on)
    {
        if (on)
        {
            _stateBeforeFullscreen = WindowState;
            WindowStyle = WindowStyle.None; ResizeMode = ResizeMode.NoResize;
            WindowState = WindowState.Normal; WindowState = WindowState.Maximized; Topmost = true;
        }
        else
        {
            Topmost = false; WindowStyle = WindowStyle.SingleBorderWindow; ResizeMode = ResizeMode.CanResize;
            WindowState = _stateBeforeFullscreen;
        }
    }

    private void RestoreWindowBounds()
    {
        var s = _vm.Settings;
        if (!s.RememberWindow) return;
        Width = s.WindowWidth; Height = s.WindowHeight;
        bool onScreen = !double.IsNaN(s.WindowLeft) && !double.IsNaN(s.WindowTop)
            && s.WindowLeft < SystemParameters.VirtualScreenLeft + SystemParameters.VirtualScreenWidth - 80
            && s.WindowTop < SystemParameters.VirtualScreenTop + SystemParameters.VirtualScreenHeight - 80
            && s.WindowLeft + s.WindowWidth > SystemParameters.VirtualScreenLeft + 80 && s.WindowTop >= SystemParameters.VirtualScreenTop - 10;
        if (onScreen) { WindowStartupLocation = WindowStartupLocation.Manual; Left = s.WindowLeft; Top = s.WindowTop; }
        else WindowStartupLocation = WindowStartupLocation.CenterScreen;
        if (s.WindowMaximized) WindowState = WindowState.Maximized;
    }

    private void SaveBounds()
    {
        var s = _vm.Settings;
        if (!s.RememberWindow) { _vm.SaveSettings(); return; }
        var b = WindowState == WindowState.Normal ? new Rect(Left, Top, Width, Height) : base.RestoreBounds;
        s.WindowLeft = b.Left; s.WindowTop = b.Top; s.WindowWidth = b.Width; s.WindowHeight = b.Height;
        s.WindowMaximized = WindowState == WindowState.Maximized && !_vm.IsFullscreen;
        _vm.SaveSettings();
    }
}
