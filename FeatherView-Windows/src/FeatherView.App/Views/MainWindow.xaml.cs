using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Shell;
using System.Runtime.InteropServices;
using FeatherView.Core.Settings;
using FeatherView.Services;
using FeatherView.ViewModels;

namespace FeatherView.Views;

/// <summary>View-only code: window chrome, drag-and-drop, drop-down menus, filmstrip scrolling, and wiring the zoom surface to the view-model.</summary>
public partial class MainWindow : Window
{
    private readonly MainViewModel _vm;
    private readonly ThemeService _theme;
    private WindowState _stateBeforeFullscreen;
    private List<InputBinding>? _savedBindings;
    private Point _lastMouse;

    public MainWindow(MainViewModel vm, ThemeService theme)
    {
        _vm = vm; _theme = theme;
        InitializeComponent();
        DataContext = vm;
        RestoreWindowBounds();

        vm.Viewport = Viewer;
        Viewer.ZoomChanged += vm.ReportZoom;
        Viewer.ViewChanged += (z, p, fit) => Viewer2.ApplyView(z, p, fit);     // compare mode: both panes zoom and pan together
        Viewer2.ViewChanged += (z, p, fit) => Viewer.ApplyView(z, p, fit);
        vm.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(MainViewModel.IsFullscreen)) ApplyFullscreen(vm.IsFullscreen); };
        vm.Gallery.ScrollToRow += row =>
        {
            if (row >= 0 && row < vm.Gallery.Rows.Count) GalleryList.ScrollIntoView(vm.Gallery.Rows[row]);
        };
        vm.Gallery.PropertyChanged += (_, e) =>
        {
            if (e.PropertyName == nameof(GalleryViewModel.SelectedIndex) && vm.Gallery.Selected is { } sel) FilmList.ScrollIntoView(sel);
        };
        vm.FocusSearchRequested += () => Dispatcher.BeginInvoke(new Action(() => { SearchBox.Focus(); SearchBox.SelectAll(); }), System.Windows.Threading.DispatcherPriority.Input);
        GalleryList.SizeChanged += (_, e) => vm.Gallery.SetViewportWidth(e.NewSize.Width - SystemParameters.VerticalScrollBarWidth - 20);

        // While typing in the search box, single-letter shortcuts must not fire.
        SearchBox.GotKeyboardFocus += (_, _) =>
        {
            _savedBindings = InputBindings.Cast<InputBinding>().ToList();
            InputBindings.Clear();
            InputBindings.Add(new KeyBinding(vm.ClearSearchCommand, Key.Escape, ModifierKeys.None));
        };
        SearchBox.LostKeyboardFocus += (_, _) =>
        {
            if (_savedBindings == null) return;
            InputBindings.Clear();
            foreach (var b in _savedBindings) InputBindings.Add(b);
            _savedBindings = null;
        };

        PreviewKeyDown += (_, _) => vm.Video.RegisterActivity();
        // Esc and F11 must work even when keyboard focus sits inside a native video window, which WPF key bindings never see.
        ComponentDispatcher.ThreadPreprocessMessage += (ref MSG msg, ref bool handled) =>
        {
            if (handled || (msg.message != 0x0100 /* WM_KEYDOWN */ && msg.message != 0x0104 /* WM_SYSKEYDOWN */) || !IsActive) return;
            int key = (int)msg.wParam;
            if (key == 0x79 /* F10 */) { MoreButton_Click(MoreButton, new RoutedEventArgs()); handled = true; return; }   // F10 opens the ... menu (also for keyboard-only use)
            if (key == 0x1B && _vm.IsFullscreen) { _vm.EscapeCommand.Execute(null); handled = true; }       // Esc leaves full screen (or stops a slideshow first)
            else if (key == 0x7A) { _vm.ToggleFullscreenCommand.Execute(null); handled = true; }              // F11
            else if (key == 0x20 && _vm.IsVideo && Keyboard.FocusedElement is not TextBox) { _vm.Video.TogglePlay(); handled = true; }   // Space = play / pause
        };
        PreviewMouseMove += (_, e) =>
        {
            var p = e.GetPosition(this);
            if (Math.Abs(p.X - _lastMouse.X) < 2 && Math.Abs(p.Y - _lastMouse.Y) < 2) return;   // ignore moves caused by layout changes
            _lastMouse = p; vm.Video.RegisterActivity();
        };
        SourceInitialized += (_, _) => theme.ApplyToWindow(this);
        StateChanged += (_, _) => UpdateMaximizeState();
        Closing += (_, _) => SaveBounds();
        Closed += (_, _) => vm.Shutdown();
        Drop += OnDrop;
        DragOver += (_, e) => { e.Effects = e.Data.GetDataPresent(DataFormats.FileDrop) ? DragDropEffects.Copy : DragDropEffects.None; e.Handled = true; };
    }

    // ---- tabs ----
    private void TitleBar_SizeChanged(object sender, SizeChangedEventArgs e) => _vm.Tabs.SetAvailableWidth(e.NewSize.Width - 52 - 150 - 36);

    private static TabItemViewModel? TabOf(object sender) => (sender as FrameworkElement)?.DataContext as TabItemViewModel;

    private void Tab_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (TabOf(sender) is { } tab) { _vm.Tabs.Activate(tab); e.Handled = true; }
    }

    private void Tab_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton == MouseButton.Middle && TabOf(sender) is { } tab) { _vm.Tabs.CloseTabCommand.Execute(tab); e.Handled = true; }
    }

    private void TabClose_Click(object sender, RoutedEventArgs e)
    {
        if (TabOf(sender) is { } tab) _vm.Tabs.CloseTabCommand.Execute(tab);
        e.Handled = true;
    }

    private void Tab_MouseRightButtonUp(object sender, MouseButtonEventArgs e)
    {
        if (TabOf(sender) is not { } tab) return;
        var menu = new ContextMenu();
        menu.Items.Add(Item("Rename tab...", () => _vm.Tabs.RenameTabCommand.Execute(tab)));
        menu.Items.Add(Item("Duplicate tab", () => _vm.Tabs.DuplicateTabCommand.Execute(tab)));
        menu.Items.Add(Item("Close tab", () => _vm.Tabs.CloseTabCommand.Execute(tab), gesture: "Ctrl+W"));
        menu.Items.Add(Item("Close other tabs", () => _vm.Tabs.CloseOtherTabsCommand.Execute(tab), enabled: _vm.Tabs.Count > 1));
        menu.Items.Add(new Separator());
        menu.Items.Add(Item("Reopen closed tab", () => _vm.Tabs.ReopenTabCommand.Execute(null), enabled: _vm.Tabs.CanReopen, gesture: "Ctrl+Shift+T"));
        menu.PlacementTarget = (UIElement)sender; menu.IsOpen = true;
        e.Handled = true;
    }

    // ---- custom title bar ----
    private void Minimize_Click(object sender, RoutedEventArgs e) => SystemCommands.MinimizeWindow(this);
    private void Close_Click(object sender, RoutedEventArgs e) => SystemCommands.CloseWindow(this);
    private void Maximize_Click(object sender, RoutedEventArgs e)
    {
        if (WindowState == WindowState.Maximized) SystemCommands.RestoreWindow(this); else SystemCommands.MaximizeWindow(this);
    }

    private void UpdateMaximizeState()
    {
        bool max = WindowState == WindowState.Maximized;
        // A maximized window with a custom frame overhangs the screen by the resize border: pull the content back in.
        RootGrid.Margin = max && !_vm.IsFullscreen ? SystemParameters.WindowResizeBorderThickness : new Thickness(0);
        MaximizeButton.Tag = (Geometry)FindResource(max ? "CaptionRestore" : "CaptionMaximize");
        MaximizeButton.ToolTip = max ? "Restore" : "Maximize";
    }

    private void MediaHost_SizeChanged(object sender, SizeChangedEventArgs e)
    {
        UpdateMediaClip();
    }

    private void UpdateMediaClip()
    {
        if (_vm.IsFullscreen) { MediaHost.Clip = null; return; }
        var size = MediaHost.RenderSize;
        MediaHost.Clip = new RectangleGeometry(new Rect(0, 0, size.Width, size.Height), 12, 12);   // rounded corners for the picture
    }

    private void OnDrop(object sender, DragEventArgs e)
    {
        if (e.Data.GetData(DataFormats.FileDrop) is string[] paths && paths.Length > 0) _vm.OpenPath(paths[0]);
    }

    private void Tile_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: MediaItemViewModel item })
        {
            if (e.ClickCount >= 2) _vm.OpenItemCommand.Execute(item); else _vm.SelectItemCommand.Execute(item);
            e.Handled = true;
        }
    }

    // ---- filmstrip ----
    private void FilmItem_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: MediaItemViewModel item }) { _vm.OpenItemCommand.Execute(item); e.Handled = true; }
    }

    private void FilmLeft_Click(object sender, RoutedEventArgs e) => ScrollFilm(-1);
    private void FilmRight_Click(object sender, RoutedEventArgs e) => ScrollFilm(1);

    private void ScrollFilm(int direction)
    {
        var sv = FindDescendant<ScrollViewer>(FilmList);
        if (sv == null) return;
        for (int i = 0; i < 5; i++) { if (direction < 0) sv.LineLeft(); else sv.LineRight(); }
    }

    private static T? FindDescendant<T>(DependencyObject root) where T : DependencyObject
    {
        for (int i = 0; i < VisualTreeHelper.GetChildrenCount(root); i++)
        {
            var c = VisualTreeHelper.GetChild(root, i);
            if (c is T t) return t;
            if (FindDescendant<T>(c) is { } d) return d;
        }
        return null;
    }

    // ---- menus (built when opened so they always reflect the current state) ----
    private static MenuItem Item(string header, Action action, bool check = false, bool enabled = true, string? gesture = null)
    {
        var m = new MenuItem { Header = header, IsChecked = check, IsEnabled = enabled, InputGestureText = gesture ?? "" };
        m.Click += (_, _) => action();
        return m;
    }

    private static void Show(Button anchor, ContextMenu menu)
    {
        menu.PlacementTarget = anchor; menu.Placement = PlacementMode.Bottom; menu.IsOpen = true;
    }

    private void ZoomButton_Click(object sender, RoutedEventArgs e)
    {
        var menu = new ContextMenu();
        menu.Items.Add(Item("Fit to window", () => _vm.ZoomPresetCommand.Execute("fit"), gesture: "0"));
        foreach (var (label, factor) in new[] { ("25%", "0.25"), ("50%", "0.5"), ("100% (actual size)", "1"), ("200%", "2"), ("400%", "4") })
        { var f = factor; menu.Items.Add(Item(label, () => _vm.ZoomPresetCommand.Execute(f))); }
        Show(ZoomButton, menu);
    }

    private void MoreButton_Click(object sender, RoutedEventArgs e)
    {
        _vm.RefreshRecent();
        var menu = new ContextMenu();
        menu.Items.Add(Item("Open file...", () => _vm.OpenFileCommand.Execute(null), gesture: "Ctrl+O"));
        menu.Items.Add(Item("Open folder...", () => _vm.OpenFolderCommand.Execute(null), gesture: "Ctrl+Shift+O"));
        menu.Items.Add(Item("Open folder in a new tab...", () => _vm.OpenFolderInNewTabCommand.Execute(null)));
        var recent = new MenuItem { Header = "Recent files", IsEnabled = _vm.RecentFiles.Count > 0 };
        foreach (var p in _vm.RecentFiles) { var path = p; recent.Items.Add(Item(System.IO.Path.GetFileName(path), () => _vm.OpenRecentCommand.Execute(path))); }
        menu.Items.Add(recent);
        menu.Items.Add(new Separator());
        menu.Items.Add(Item("Rotate left", () => _vm.RotateLeftCommand.Execute(null), gesture: "L"));
        menu.Items.Add(Item("Flip horizontally", () => _vm.FlipCommand.Execute(null)));
        menu.Items.Add(Item("Actual size (100%)", () => _vm.ActualSizeCommand.Execute(null), gesture: "1"));
        menu.Items.Add(Item("Copy image", () => _vm.CopyCommand.Execute(null), gesture: "Ctrl+C"));
        menu.Items.Add(Item("Delete (Recycle Bin)...", () => _vm.DeleteCommand.Execute(null), gesture: "Del"));
        menu.Items.Add(Item("Reload folder", () => _vm.ReloadCommand.Execute(null), gesture: "F5"));
        menu.Items.Add(new Separator());
        var sort = new MenuItem { Header = "Sort by" };
        foreach (var mode in Enum.GetValues<SortMode>()) { var m = mode; sort.Items.Add(Item(m.ToString(), () => _vm.SetSort(m), check: _vm.Settings.Sort == m)); }
        sort.Items.Add(new Separator());
        sort.Items.Add(Item("Ascending", () => _vm.SetSort(_vm.Settings.Sort, false), check: !_vm.Settings.SortDescending));
        sort.Items.Add(Item("Descending", () => _vm.SetSort(_vm.Settings.Sort, true), check: _vm.Settings.SortDescending));
        menu.Items.Add(sort);
        var group = new MenuItem { Header = "Group by" };
        foreach (var (mode, label) in new[] { (GroupMode.None, "None"), (GroupMode.Folder, "Folder"), (GroupMode.Type, "File type"), (GroupMode.Year, "Date: year"), (GroupMode.Month, "Date: month"), (GroupMode.Day, "Date: day"), (GroupMode.Size, "File size") })
        { var m = mode; group.Items.Add(Item(label, () => _vm.SetGroup(m), check: _vm.Settings.GroupBy == m)); }
        menu.Items.Add(group);
        menu.Items.Add(Item("Include subfolders", () => _vm.ToggleSubfoldersCommand.Execute(null), check: _vm.IncludeSubfolders));
        menu.Items.Add(Item("Sidebar", () => _vm.ToggleSidebarCommand.Execute(null), check: _vm.ShowSidebar, gesture: "Ctrl+B"));
        menu.Items.Add(Item("Thumbnail strip", () => _vm.ToggleFilmstripCommand.Execute(null), check: _vm.Settings.ShowFilmstrip, gesture: "T"));
        menu.Items.Add(Item("Details panel", () => _vm.ToggleInfoCommand.Execute(null), check: _vm.ShowInfo, gesture: "I"));
        menu.Items.Add(new Separator());
        menu.Items.Add(Item("Settings...", () => _vm.SettingsCommand.Execute(null), gesture: "Ctrl+,"));
        menu.Items.Add(Item("Check for updates...", () => _vm.CheckUpdatesCommand.Execute(null)));
        menu.Items.Add(Item("About Feather View", () => _vm.AboutCommand.Execute(null)));
        Show(MoreButton, menu);
    }

    // ---- full screen and remembered bounds ----
    [DllImport("user32.dll")] private static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint flags);
    [DllImport("user32.dll")] private static extern bool GetMonitorInfo(IntPtr monitor, ref MONITORINFO info);
    [DllImport("user32.dll")] private static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct MONITORINFO { public int Size; public RECT Monitor, Work; public uint Flags; }

    private WindowChrome? _savedChrome;
    private Rect _savedBounds;

    /// <summary>
    /// True full screen: no frame, no custom chrome, no margins, black background, and exactly the monitor's pixels
    /// (a window that fills the monitor without a frame also makes Windows hide the taskbar).
    /// </summary>
    private void ApplyFullscreen(bool on)
    {
        var hwnd = new WindowInteropHelper(this).Handle;
        if (on)
        {
            _stateBeforeFullscreen = WindowState;
            if (WindowState != WindowState.Normal) WindowState = WindowState.Normal;
            _savedBounds = new Rect(Left, Top, Width, Height);
            _savedChrome = WindowChrome.GetWindowChrome(this);
            WindowChrome.SetWindowChrome(this, null);
            WindowStyle = WindowStyle.None; ResizeMode = ResizeMode.NoResize;
            Background = Brushes.Black;
            RootGrid.Margin = new Thickness(0); BodyGrid.Margin = new Thickness(0);
            MediaHost.CornerRadius = new CornerRadius(0); MediaHost.Background = Brushes.Black; MediaHost.Clip = null;
            Topmost = true;
            var mi = new MONITORINFO { Size = Marshal.SizeOf<MONITORINFO>() };
            if (GetMonitorInfo(MonitorFromWindow(hwnd, 2 /* nearest */), ref mi))
                SetWindowPos(hwnd, new IntPtr(-1) /* topmost */, mi.Monitor.Left, mi.Monitor.Top, mi.Monitor.Right - mi.Monitor.Left, mi.Monitor.Bottom - mi.Monitor.Top, 0x0020 | 0x0040);
            else WindowState = WindowState.Maximized;
        }
        else
        {
            Topmost = false;
            WindowChrome.SetWindowChrome(this, _savedChrome);
            WindowStyle = WindowStyle.SingleBorderWindow; ResizeMode = ResizeMode.CanResize;
            SetResourceReference(BackgroundProperty, "BgBrush");
            BodyGrid.Margin = new Thickness(10, 0, 10, 0);
            MediaHost.CornerRadius = new CornerRadius(12); MediaHost.SetResourceReference(Border.BackgroundProperty, "SurfaceBrush");
            Left = _savedBounds.Left; Top = _savedBounds.Top; Width = _savedBounds.Width; Height = _savedBounds.Height;
            WindowState = _stateBeforeFullscreen;
            UpdateMaximizeState();
            UpdateMediaClip();
        }
    }

    private void RestoreWindowBounds()
    {
        var s = _vm.Settings;
        if (!s.RememberWindow) return;
        Width = Math.Max(s.WindowWidth, MinWidth); Height = Math.Max(s.WindowHeight, MinHeight);
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
