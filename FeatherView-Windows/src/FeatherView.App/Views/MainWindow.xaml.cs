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

        // Rubber-band selection: press on the gallery and drag a rectangle over thumbnails (hold Ctrl to add to the current selection)
        GalleryList.PreviewMouseLeftButtonDown += Gallery_MarqueeDown;
        GalleryList.PreviewMouseMove += Gallery_MarqueeMove;
        GalleryList.PreviewMouseLeftButtonUp += (_, e) =>
        {
            if (EndMarquee()) e.Handled = true;
            if (_deferredClick is { } d) { _deferredClick = null; _vm.ClickItem(d, ModifierKeys.None); }   // a plain click (no drag) on part of a multi-selection selects just that one
            _dragItem = null;
        };
        GalleryList.LostMouseCapture += (_, _) => EndMarquee();

        // The folder path is editable: Enter goes there, Esc puts the current path back. Shortcuts are paused while typing, like in the search box.
        PathBox.GotKeyboardFocus += (_, _) =>
        {
            _savedBindings = InputBindings.Cast<InputBinding>().ToList();
            InputBindings.Clear();
            PathBox.Foreground = (Brush)FindResource("TextBrush"); PathBox.BorderBrush = (Brush)FindResource("BorderBrushSoft"); PathBox.SelectAll();
        };
        PathBox.LostKeyboardFocus += (_, _) =>
        {
            if (_savedBindings != null) { InputBindings.Clear(); foreach (var b in _savedBindings) InputBindings.Add(b); _savedBindings = null; }
            PathBox.Foreground = (Brush)FindResource("SubtleTextBrush"); PathBox.BorderBrush = Brushes.Transparent;
            PathBox.Text = vm.FolderDisplay;
        };
        PathBox.PreviewKeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter) { var text = PathBox.Text; Keyboard.ClearFocus(); FocusManager.SetFocusedElement(this, this); _vm.GoToTypedPath(text); e.Handled = true; }
            else if (e.Key == Key.Escape) { PathBox.Text = vm.FolderDisplay; Keyboard.ClearFocus(); FocusManager.SetFocusedElement(this, this); e.Handled = true; }
        };

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
        // On narrow windows the Details panel would push the right-hand toolbar buttons (delete, favourite, menu) out of view: hide it, bring it back when the window is wide again
        SizeChanged += (_, e) =>
        {
            if (e.NewSize.Width < 1250 && vm.ShowInfo) { _autoHidInfo = true; vm.ShowInfo = false; }
            else if (e.NewSize.Width >= 1250 && _autoHidInfo) { _autoHidInfo = false; vm.ShowInfo = true; }
        };
        vm.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(MainViewModel.ShowInfo) && vm.ShowInfo && ActualWidth < 1250) _autoHidInfo = false; };   // the user opened it on purpose
        vm.Video.ContextMenuRequested += () => Dispatcher.BeginInvoke(new Action(ShowFileMenu));   // right click on a video (either engine)
        FilmList.PreviewMouseLeftButtonUp += (_, _) => _filmDragItem = null;
        FilmList.PreviewMouseWheel += (_, e) =>
        {
            if (!Keyboard.Modifiers.HasFlag(ModifierKeys.Control)) return;
            vm.FilmstripHeight += e.Delta > 0 ? 10 : -10; vm.SaveSettings(); e.Handled = true;   // Ctrl + wheel resizes the strip
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

    private bool _autoHidInfo;

    private void OnDrop(object sender, DragEventArgs e)
    {
        if (e.Data.GetData(DataFormats.FileDrop) is string[] paths && paths.Length > 0) _vm.OpenPath(paths[0]);
    }

    // ---- rubber-band (drag rectangle) selection in the gallery ----
    private MediaItemViewModel? _dragItem, _deferredClick;
    private Point _mqStart; private bool _mqDown, _mqActive; private HashSet<MediaItemViewModel>? _mqBase;

    private static T? Ancestor<T>(DependencyObject? d) where T : DependencyObject
    {
        while (d != null) { if (d is T t) return t; d = d is Visual or System.Windows.Media.Media3D.Visual3D ? VisualTreeHelper.GetParent(d) : LogicalTreeHelper.GetParent(d); }
        return null;
    }

    private void Gallery_MarqueeDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ClickCount > 1 || Ancestor<ScrollBar>(e.OriginalSource as DependencyObject) != null || Ancestor<CheckBox>(e.OriginalSource as DependencyObject) != null) return;
        _dragItem = null; _deferredClick = null;
        _mqStart = e.GetPosition(GalleryList); _mqDown = true; _mqActive = false;
    }

    private void Gallery_MarqueeMove(object sender, MouseEventArgs e)
    {
        if (!_mqDown) return;
        if (e.LeftButton != MouseButtonState.Pressed) { _mqDown = false; return; }
        var p = e.GetPosition(GalleryList);
        if (!_mqActive)
        {
            if (Math.Abs(p.X - _mqStart.X) < 6 && Math.Abs(p.Y - _mqStart.Y) < 6) return;   // a click, not a drag
            if (_dragItem != null)
            {
                // dragging FROM a thumbnail: take the file(s) to Explorer, OneDrive, a browser upload, an e-mail...
                var item = _dragItem; _dragItem = null; _deferredClick = null; _mqDown = false;
                StartFileDrag(GalleryList, item);
                e.Handled = true; return;
            }
            _mqActive = true; GalleryList.CaptureMouse();
            bool ctrl = Keyboard.Modifiers.HasFlag(ModifierKeys.Control) || Keyboard.Modifiers.HasFlag(ModifierKeys.Shift);
            _mqBase = ctrl ? _vm.Gallery.Items.Where(i => i.IsChecked).ToHashSet() : new HashSet<MediaItemViewModel>();
            if (!ctrl && _vm.HasChecked) _vm.ClearCheckedCommand.Execute(null);
        }
        var rect = new Rect(Math.Min(_mqStart.X, p.X), Math.Min(_mqStart.Y, p.Y), Math.Abs(p.X - _mqStart.X), Math.Abs(p.Y - _mqStart.Y));
        // the overlay canvas sits in the same cell as the list, so list coordinates are canvas coordinates
        System.Windows.Controls.Canvas.SetLeft(MarqueeRect, rect.X); System.Windows.Controls.Canvas.SetTop(MarqueeRect, rect.Y);
        MarqueeRect.Width = rect.Width; MarqueeRect.Height = rect.Height; MarqueeRect.Visibility = Visibility.Visible;
        MarqueeSelect(rect);
        e.Handled = true;
    }

    private void MarqueeSelect(Rect rect)
    {
        foreach (var tile in FindTiles(GalleryList))
        {
            if (tile.DataContext is not MediaItemViewModel item) continue;
            var origin = tile.TransformToAncestor(GalleryList).Transform(new Point(0, 0));
            var bounds = new Rect(origin, new Size(tile.ActualWidth, tile.ActualHeight));
            bool hit = bounds.IntersectsWith(rect);
            bool want = hit || (_mqBase?.Contains(item) ?? false);
            if (item.IsChecked != want) item.IsChecked = want;
        }
    }

    private static IEnumerable<Border> FindTiles(DependencyObject root)
    {
        int n = VisualTreeHelper.GetChildrenCount(root);
        for (int i = 0; i < n; i++)
        {
            var c = VisualTreeHelper.GetChild(root, i);
            if (c is Border { Name: "Tile" } b) { yield return b; continue; }
            foreach (var d in FindTiles(c)) yield return d;
        }
    }

    /// <summary>Starts a normal Windows drag of the real files (copy only: the originals are never moved or changed).</summary>
    private void StartFileDrag(DependencyObject source, MediaItemViewModel item)
    {
        var files = item.IsChecked ? _vm.Gallery.Items.Where(i => i.IsChecked).Select(i => i.File.Path) : new[] { item.File.Path };
        var paths = files.Where(System.IO.File.Exists).ToArray();
        if (paths.Length == 0) return;
        try
        {
            var data = new DataObject(DataFormats.FileDrop, paths);
            DragDrop.DoDragDrop(source, data, DragDropEffects.Copy);
        }
        catch (Exception ex) when (ex is System.Runtime.InteropServices.COMException or InvalidOperationException) { }
    }

    private bool EndMarquee()
    {
        bool was = _mqActive;
        _mqDown = false; _mqActive = false; _mqBase = null;
        if (was) { MarqueeRect.Visibility = Visibility.Collapsed; GalleryList.ReleaseMouseCapture(); }
        return was;
    }

    private void Tile_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: MediaItemViewModel item })
        {
            _dragItem = item;
            if (e.ClickCount >= 2) _vm.OpenItemCommand.Execute(item);
            else if (item.IsChecked && _vm.CheckedCount > 1 && Keyboard.Modifiers == ModifierKeys.None) _deferredClick = item;   // keep the multi-selection so it can be dragged
            else _vm.ClickItem(item, Keyboard.Modifiers);
            e.Handled = true;
        }
    }

    // ---- filmstrip ----
    private MediaItemViewModel? _filmDragItem; private Point _filmDragStart;

    private void FilmItem_MouseMove(object sender, MouseEventArgs e)
    {
        if (_filmDragItem == null) return;
        if (e.LeftButton != MouseButtonState.Pressed) { _filmDragItem = null; return; }
        var p = e.GetPosition(this);
        if (Math.Abs(p.X - _filmDragStart.X) < 8 && Math.Abs(p.Y - _filmDragStart.Y) < 8) return;
        var item = _filmDragItem; _filmDragItem = null;
        StartFileDrag(FilmList, item);
    }

    private void FilmItem_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: MediaItemViewModel item })
        {
            _filmDragItem = item; _filmDragStart = e.GetPosition(this);
            _vm.OpenItemCommand.Execute(item); e.Handled = true;
        }
    }

    private void DetailsResize_DragDelta(object sender, DragDeltaEventArgs e) => _vm.DetailsWidth -= (int)Math.Round(e.HorizontalChange);
    private void DetailsResize_DragCompleted(object sender, DragCompletedEventArgs e) => _vm.SaveSettings();

    private void FilmResize_DragDelta(object sender, System.Windows.Controls.Primitives.DragDeltaEventArgs e) => _vm.FilmstripHeight -= (int)Math.Round(e.VerticalChange);
    private void FilmResize_DragCompleted(object sender, System.Windows.Controls.Primitives.DragCompletedEventArgs e) => _vm.SaveSettings();

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

    // ---- right-click menu for pictures, videos and thumbnails ----
    private void Tile_MouseRightButtonUp(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: MediaItemViewModel item }) { _vm.ContextSelect(item); ShowFileMenu(); e.Handled = true; }
    }

    private void Film_MouseRightButtonUp(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: MediaItemViewModel item }) { _vm.OpenItemCommand.Execute(item); ShowFileMenu(); e.Handled = true; }
    }

    private void Viewer_MouseRightButtonUp(object sender, MouseButtonEventArgs e) { ShowFileMenu(); e.Handled = true; }

    private void ShowFileMenu()
    {
        if (_vm.TargetFile == null) return;
        bool gallery = _vm.IsGallery, photo = _vm.IsPhoto, video = _vm.IsVideo;
        int n = _vm.CheckedCount;
        var menu = new ContextMenu();
        if (gallery) menu.Items.Add(Item("Open", () => _vm.OpenTarget(), gesture: "Enter"));
        if (photo)
        {
            menu.Items.Add(Item("Rotate left", () => _vm.RotateLeftCommand.Execute(null), gesture: "L"));
            menu.Items.Add(Item("Rotate right", () => _vm.RotateRightCommand.Execute(null), gesture: "R"));
            menu.Items.Add(Item("Flip horizontally", () => _vm.FlipCommand.Execute(null)));
            menu.Items.Add(Item("Fit to window", () => _vm.FitCommand.Execute(null), gesture: "0"));
            menu.Items.Add(Item("Actual size (100%)", () => _vm.ActualSizeCommand.Execute(null), gesture: "1"));
            menu.Items.Add(new Separator());
            menu.Items.Add(Item("Copy image", () => _vm.CopyCommand.Execute(null), gesture: "Ctrl+C"));
        }
        if (video)
        {
            menu.Items.Add(Item(_vm.Video.IsPlaying ? "Pause" : "Play", () => _vm.Video.TogglePlay(), gesture: "Space"));
            menu.Items.Add(Item("Full screen", () => _vm.ToggleFullscreenCommand.Execute(null), gesture: "F11"));
            menu.Items.Add(Item("Save screenshot of this frame", () => _vm.Video.ScreenshotCommand.Execute(null), gesture: "Ctrl+S", enabled: _vm.Video.CanScreenshot));
            menu.Items.Add(new Separator());
        }
        int vids = _vm.TargetVideoCount;
        if (vids > 0 && !video)
        {
            menu.Items.Add(Item(vids > 1 ? $"Play {vids} videos as a playlist" : "Play video", () => _vm.PlaySelectionAsPlaylist()));
            menu.Items.Add(Item(vids > 1 ? $"Add {vids} videos to the playlist" : "Add to playlist", () => _vm.AddSelectionToPlaylist()));
        }
        if (_vm.Video.Playlist.Count > 0 && !video) menu.Items.Add(Item($"Play the playlist ({_vm.Video.Playlist.Count} videos)", () => _vm.PlayPlaylist()));
        if (video) menu.Items.Add(Item("Add all videos of this folder to the playlist", () => { if (_vm.Video.FolderVideosProvider?.Invoke() is { } v) _vm.Video.AddToPlaylist(v); }));
        menu.Items.Add(new Separator());
        menu.Items.Add(Item(n > 1 ? $"Copy {n} files" : "Copy file", () => _vm.CopyFilesToClipboard()));
        menu.Items.Add(Item("Copy file path", () => _vm.CopyPathToClipboard()));
        menu.Items.Add(Item("Show in Explorer", () => _vm.ShowTargetInExplorer()));
        menu.Items.Add(new Separator());
        menu.Items.Add(Item(_vm.IsFavorite ? "Remove from favorites" : "Add to favorites", () => _vm.ToggleFavoriteCommand.Execute(null), gesture: "Ctrl+D"));
        if (photo)
        {
            menu.Items.Add(Item("Set as wallpaper...", () => _vm.SetWallpaperCommand.Execute(null)));
            menu.Items.Add(Item("Print...", () => _vm.PrintCommand.Execute(null), gesture: "Ctrl+P"));
            menu.Items.Add(Item("Compare side by side", () => _vm.CompareCommand.Execute(null), check: _vm.IsCompare));
        }
        if (photo || video) menu.Items.Add(Item("Slideshow", () => _vm.ToggleSlideshowCommand.Execute(null), gesture: "S"));
        menu.Items.Add(new Separator());
        menu.Items.Add(Item("Select all", () => _vm.SelectAllCommand.Execute(null), gesture: "Ctrl+A", enabled: gallery || n > 0));
        menu.Items.Add(Item("Details panel", () => _vm.ToggleInfoCommand.Execute(null), check: _vm.ShowInfo, gesture: "I"));
        menu.Items.Add(new Separator());
        menu.Items.Add(Item(n > 1 ? $"Delete {n} files (Recycle Bin)..." : "Delete (Recycle Bin)...", () => _vm.DeleteCommand.Execute(null), gesture: "Del"));
        menu.PlacementTarget = this; menu.Placement = PlacementMode.MousePoint; menu.IsOpen = true;
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
