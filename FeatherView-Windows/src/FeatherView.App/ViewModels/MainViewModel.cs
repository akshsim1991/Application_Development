using System.Collections.ObjectModel;
using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Media;
using FeatherView.Core.Mvvm;
using FeatherView.Core.Settings;
using FeatherView.Services;

namespace FeatherView.ViewModels;

public enum ViewMode { Photo, Gallery }

/// <summary>The view surface (zoom/pan control) as seen by the view-model.</summary>
public interface IViewport { void ZoomIn(); void ZoomOut(); void Fit(); void ActualSize(); }

public sealed class MainViewModel : ObservableObject
{
    private readonly SettingsService _settingsService;
    private readonly ILog _log;
    private readonly ThumbnailService _thumbs;
    private readonly IDialogService _dialogs;
    private readonly ThemeService _theme;
    private readonly DispatcherTimer _slideTimer = new();

    private List<MediaFile> _files = new();
    private int _index = -1;
    private CancellationTokenSource? _loadCts;
    private BitmapSource? _base, _gifFrame;
    private GifAnimator? _gif;
    private int _rotation;
    private bool _flip, _fullscreenBySlideshow;

    private ViewMode _mode = ViewMode.Gallery;
    private ImageSource? _currentImage;
    private object? _imageId;
    private string? _errorText;
    private string _statusText = "";
    private string _zoomText = "";
    private bool _isSlideshow, _isFullscreen, _showInfo;
    private string _folderPath = "";

    public MainViewModel(AppSettings settings, SettingsService settingsService, ThumbnailService thumbs,
                         IDialogService dialogs, ThemeService theme, ILog log)
    {
        Settings = settings; _settingsService = settingsService; _thumbs = thumbs; _dialogs = dialogs; _theme = theme; _log = log;
        Gallery = new GalleryViewModel();
        Gallery.SetThumbSize(settings.ThumbnailSize);
        _showInfo = settings.ShowInfoPanelOnOpen;
        _slideTimer.Tick += (_, _) => SlideTick();

        OpenFileCommand = new RelayCommand(OpenFile);
        OpenFolderCommand = new RelayCommand(OpenFolder);
        OpenRecentCommand = new RelayCommand(p => { if (p is string s) OpenPath(s); });
        NextCommand = new RelayCommand(() => Step(1));
        PrevCommand = new RelayCommand(() => Step(-1));
        UpCommand = new RelayCommand(() => { if (Mode == ViewMode.Gallery) Gallery.MoveRows(-1); });
        DownCommand = new RelayCommand(() => { if (Mode == ViewMode.Gallery) Gallery.MoveRows(1); });
        FirstCommand = new RelayCommand(() => JumpTo(0));
        LastCommand = new RelayCommand(() => JumpTo(_files.Count - 1));
        ToggleGalleryCommand = new RelayCommand(ToggleGallery);
        OpenItemCommand = new RelayCommand(p => { if (p is MediaItemViewModel m) { Gallery.Select(_files.IndexOf(m.File)); OpenSelected(); } });
        SelectItemCommand = new RelayCommand(p => { if (p is MediaItemViewModel m) Gallery.Select(_files.IndexOf(m.File)); });
        OpenSelectedCommand = new RelayCommand(OpenSelected);
        RotateLeftCommand = new RelayCommand(() => Rotate(-1));
        RotateRightCommand = new RelayCommand(() => Rotate(1));
        FlipCommand = new RelayCommand(Flip);
        DeleteCommand = new RelayCommand(DeleteCurrent);
        CopyCommand = new RelayCommand(CopyImage);
        ToggleSlideshowCommand = new RelayCommand(ToggleSlideshow);
        ToggleFullscreenCommand = new RelayCommand(() => IsFullscreen = !IsFullscreen);
        ToggleInfoCommand = new RelayCommand(() => ShowInfo = !ShowInfo);
        EscapeCommand = new RelayCommand(Escape);
        ZoomInCommand = new RelayCommand(() => Viewport?.ZoomIn());
        ZoomOutCommand = new RelayCommand(() => Viewport?.ZoomOut());
        FitCommand = new RelayCommand(() => Viewport?.Fit());
        ActualSizeCommand = new RelayCommand(() => Viewport?.ActualSize());
        ReloadCommand = new RelayCommand(Reload);
        ShowInExplorerCommand = new RelayCommand(() => { if (Current != null) FileOperations.ShowInExplorer(Current.Path); });
        SettingsCommand = new RelayCommand(OpenSettings);
        CheckUpdatesCommand = new AsyncRelayCommand(() => Updates?.CheckAsync(true) ?? Task.CompletedTask);
    }

    // ---- state exposed to the view ----
    public AppSettings Settings { get; private set; }
    public GalleryViewModel Gallery { get; }
    public IViewport? Viewport { get; set; }
    public UpdateController? Updates { get; set; }
    public ObservableCollection<InfoLine> Info { get; } = new();
    public ObservableCollection<string> RecentFiles { get; } = new();

    public ViewMode Mode { get => _mode; private set { if (Set(ref _mode, value)) { OnPropertyChanged(nameof(IsGallery)); OnPropertyChanged(nameof(IsPhoto)); OnPropertyChanged(nameof(ShowEmptyState)); } } }
    public bool IsGallery => _mode == ViewMode.Gallery;
    public bool IsPhoto => _mode == ViewMode.Photo;
    public ImageSource? CurrentImage { get => _currentImage; private set => Set(ref _currentImage, value); }
    public object? ImageId { get => _imageId; private set => Set(ref _imageId, value); }
    public string? ErrorText { get => _errorText; private set { Set(ref _errorText, value); OnPropertyChanged(nameof(HasError)); } }
    public bool HasError => !string.IsNullOrEmpty(_errorText);
    public string StatusText { get => _statusText; private set => Set(ref _statusText, value); }
    public string ZoomText { get => _zoomText; private set => Set(ref _zoomText, value); }
    public bool ShowEmptyState => _files.Count == 0;
    public bool IsSlideshow { get => _isSlideshow; private set { if (Set(ref _isSlideshow, value)) OnPropertyChanged(nameof(SlideshowIcon)); } }
    public bool IsFullscreen { get => _isFullscreen; set { if (Set(ref _isFullscreen, value)) OnPropertyChanged(nameof(ShowChrome)); } }
    public bool ShowChrome => !_isFullscreen;
    public bool ShowInfo { get => _showInfo; set => Set(ref _showInfo, value); }
    public string SlideshowIcon => _isSlideshow ? "Pause" : "Play";
    public MediaFile? Current => _index >= 0 && _index < _files.Count ? _files[_index] : null;

    public RelayCommand OpenFileCommand { get; }
    public RelayCommand OpenFolderCommand { get; }
    public RelayCommand OpenRecentCommand { get; }
    public RelayCommand NextCommand { get; }
    public RelayCommand PrevCommand { get; }
    public RelayCommand UpCommand { get; }
    public RelayCommand DownCommand { get; }
    public RelayCommand FirstCommand { get; }
    public RelayCommand LastCommand { get; }
    public RelayCommand ToggleGalleryCommand { get; }
    public RelayCommand OpenItemCommand { get; }
    public RelayCommand SelectItemCommand { get; }
    public RelayCommand OpenSelectedCommand { get; }
    public RelayCommand RotateLeftCommand { get; }
    public RelayCommand RotateRightCommand { get; }
    public RelayCommand FlipCommand { get; }
    public RelayCommand DeleteCommand { get; }
    public RelayCommand CopyCommand { get; }
    public RelayCommand ToggleSlideshowCommand { get; }
    public RelayCommand ToggleFullscreenCommand { get; }
    public RelayCommand ToggleInfoCommand { get; }
    public RelayCommand EscapeCommand { get; }
    public RelayCommand ZoomInCommand { get; }
    public RelayCommand ZoomOutCommand { get; }
    public RelayCommand FitCommand { get; }
    public RelayCommand ActualSizeCommand { get; }
    public RelayCommand ReloadCommand { get; }
    public RelayCommand ShowInExplorerCommand { get; }
    public RelayCommand SettingsCommand { get; }
    public AsyncRelayCommand CheckUpdatesCommand { get; }

    public void ReportZoom(double zoom) => ZoomText = $"{Math.Round(zoom * 100)}%";

    // ---- opening ----
    private void OpenFile() { var p = _dialogs.PickFile(); if (p != null) OpenPath(p); }
    private void OpenFolder() { var p = _dialogs.PickFolder(); if (p != null) OpenPath(p); }

    public void OpenPath(string path)
    {
        try
        {
            if (Directory.Exists(path)) { LoadFolder(path, null, gallery: true); return; }
            if (!File.Exists(path)) { _dialogs.Error("Open", "That file could not be found."); return; }
            LoadFolder(Path.GetDirectoryName(path)!, path, gallery: false);
            Settings.AddRecent(path); SaveSettings(); RefreshRecent();
        }
        catch (Exception ex) { _log.Error("Open failed", ex); _dialogs.Error("Open", "The location could not be opened."); }
    }

    private void LoadFolder(string folder, string? selectFile, bool gallery)
    {
        StopSlideshow();
        _folderPath = folder;
        _files = MediaScanner.Scan(folder, Settings.Sort, Settings.SortDescending);
        if (selectFile != null && !_files.Any(f => f.Path.Equals(selectFile, StringComparison.OrdinalIgnoreCase)) && MediaTypes.Classify(selectFile) != MediaKind.Unknown)
        {
            var fi = new FileInfo(selectFile);   // file the scanner skipped (e.g. hidden)
            _files.Insert(0, new MediaFile(fi.FullName, fi.Name, fi.Length, fi.LastWriteTimeUtc, MediaTypes.Classify(selectFile)));
        }
        int idx = selectFile == null ? 0 : Math.Max(0, _files.FindIndex(f => f.Path.Equals(selectFile, StringComparison.OrdinalIgnoreCase)));
        Gallery.SetItems(_files.Select(f => new MediaItemViewModel(f, _thumbs)), idx);
        OnPropertyChanged(nameof(ShowEmptyState));
        if (_files.Count == 0) { Mode = ViewMode.Gallery; CurrentImage = null; StatusText = "No photos or videos in this folder"; return; }
        if (gallery) { Mode = ViewMode.Gallery; _index = idx; UpdateStatus(); }
        else { Mode = ViewMode.Photo; _ = ShowIndexAsync(idx); }
    }

    private void Reload()
    {
        if (string.IsNullOrEmpty(_folderPath)) return;
        var keep = Current?.Path;
        LoadFolder(_folderPath, keep, gallery: IsGallery);
    }

    // ---- navigation ----
    private void Step(int delta)
    {
        if (_files.Count == 0) return;
        if (Mode == ViewMode.Gallery) { Gallery.Move(delta); return; }
        _ = ShowIndexAsync(((_index + delta) % _files.Count + _files.Count) % _files.Count);
    }

    private void JumpTo(int i)
    {
        if (_files.Count == 0) return;
        if (Mode == ViewMode.Gallery) Gallery.Select(i, scroll: true); else _ = ShowIndexAsync(Math.Clamp(i, 0, _files.Count - 1));
    }

    private void ToggleGallery()
    {
        if (_files.Count == 0) return;
        StopSlideshow();
        if (Mode == ViewMode.Photo) { Gallery.Select(_index, scroll: true); Mode = ViewMode.Gallery; _gif?.Pause(); }
        else OpenSelected();
    }

    private void OpenSelected()
    {
        if (_files.Count == 0 || Gallery.SelectedIndex < 0) return;
        Mode = ViewMode.Photo;
        if (Gallery.SelectedIndex == _index && CurrentImage != null) { _gif?.Resume(); UpdateStatus(); return; }
        _ = ShowIndexAsync(Gallery.SelectedIndex);
    }

    private void Escape()
    {
        if (IsSlideshow) StopSlideshow();
        else if (IsFullscreen) IsFullscreen = false;
        else if (Mode == ViewMode.Gallery && CurrentImage != null) OpenSelected();
    }

    // ---- showing a photo ----
    private async Task ShowIndexAsync(int index)
    {
        if (index < 0 || index >= _files.Count) return;
        _loadCts?.Cancel();
        var cts = _loadCts = new CancellationTokenSource();
        var ct = cts.Token;
        _index = index;
        Gallery.Select(index);
        var file = _files[index];
        StopGif();
        ErrorText = null;
        UpdateStatus();

        if (file.Kind == MediaKind.Video)
        {
            _base = null; CurrentImage = null; ImageId = file.Path; Info.Clear();
            ErrorText = "Video playback is not available yet in this version.";
            return;
        }
        try
        {
            var li = await Task.Run(() => ImageLoader.Load(file.Path), ct);
            if (ct.IsCancellationRequested) return;
            _base = li.Image; _gifFrame = null; _rotation = 0; _flip = false;
            CurrentImage = _base;
            ImageId = file.Path;
            BuildInfo(file, li);
            UpdateStatus(li.PixelWidth, li.PixelHeight);
            if (li.Gif != null) StartGif(li.Gif);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex)
        {
            if (ct.IsCancellationRequested) return;
            _log.Warn($"Could not open an image ({ex.GetType().Name}).");
            _base = null; CurrentImage = null; ImageId = file.Path; Info.Clear();
            ErrorText = "This file could not be opened. It may be damaged, or use a format this version of Windows cannot decode.";
        }
    }

    private void StartGif(GifBitmapDecoder decoder)
    {
        try
        {
            _gif = new GifAnimator(decoder);
            _gif.FrameReady += f => { _gifFrame = f; CurrentImage = ImageLoader.Transform(f, _rotation, _flip); };
            _gif.Start();
        }
        catch (Exception ex) { _log.Warn($"GIF animation unavailable ({ex.GetType().Name})."); _gif = null; }
    }

    private void StopGif() { _gif?.Dispose(); _gif = null; _gifFrame = null; }

    private void BuildInfo(MediaFile f, LoadedImage li)
    {
        Info.Clear();
        Info.Add(new InfoLine("Name", f.Name));
        Info.Add(new InfoLine("Type", li.Format));
        Info.Add(new InfoLine("Dimensions", $"{li.PixelWidth} × {li.PixelHeight} px"));
        Info.Add(new InfoLine("Size", FormatSize(f.Length)));
        Info.Add(new InfoLine("Modified", f.LastWriteUtc.ToLocalTime().ToString("g")));
        if (li.Gif != null) Info.Add(new InfoLine("Frames", li.Gif.Frames.Count.ToString()));
        foreach (var l in li.Exif) Info.Add(l);
    }

    private void UpdateStatus(int w = 0, int h = 0)
    {
        var f = Current;
        if (f == null) { StatusText = ""; return; }
        var dims = w > 0 ? $"  ·  {w}×{h}" : "";
        StatusText = $"{_index + 1} / {_files.Count}  ·  {f.Name}{dims}  ·  {FormatSize(f.Length)}";
    }

    public static string FormatSize(long b) =>
        b >= 1L << 30 ? $"{b / (double)(1L << 30):0.0} GB" : b >= 1L << 20 ? $"{b / (double)(1L << 20):0.0} MB" : b >= 1024 ? $"{b / 1024.0:0} KB" : $"{b} B";

    // ---- transforms ----
    private void Rotate(int quarterTurns) { _rotation = ((_rotation + quarterTurns) % 4 + 4) % 4; ApplyTransform(); }
    private void Flip() { _flip = !_flip; ApplyTransform(); }
    private void ApplyTransform()
    {
        var src = _gifFrame ?? _base;
        if (src == null) return;
        CurrentImage = ImageLoader.Transform(src, _rotation, _flip);   // view only: the original file is never modified
    }

    // ---- file operations ----
    private void DeleteCurrent()
    {
        var f = Mode == ViewMode.Gallery ? Gallery.Selected?.File : Current;
        if (f == null) return;
        if (!_dialogs.Confirm("Delete file", $"Move \"{f.Name}\" to the Recycle Bin?", "Delete")) return;
        try
        {
            int idx = _files.IndexOf(f);
            if (idx == _index) { StopGif(); _base = null; CurrentImage = null; }
            FileOperations.Recycle(f.Path);
            _files.RemoveAt(idx);
            Gallery.SetItems(_files.Select(x => new MediaItemViewModel(x, _thumbs)), Math.Min(idx, _files.Count - 1));
            OnPropertyChanged(nameof(ShowEmptyState));
            if (_files.Count == 0) { _index = -1; Mode = ViewMode.Gallery; StatusText = "No more photos or videos in this folder"; return; }
            if (Mode == ViewMode.Photo) _ = ShowIndexAsync(Math.Min(idx, _files.Count - 1)); else { _index = Math.Min(idx, _files.Count - 1); UpdateStatus(); }
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { _log.Warn($"Delete failed ({ex.GetType().Name})."); _dialogs.Error("Delete", "The file could not be moved to the Recycle Bin."); }
    }

    private void CopyImage()
    {
        if (CurrentImage is not BitmapSource bs) return;
        try
        {
            var data = new DataObject();
            data.SetImage(bs);
            if (Current != null) data.SetFileDropList(new System.Collections.Specialized.StringCollection { Current.Path });
            Clipboard.SetDataObject(data, true);
        }
        catch (Exception ex) { _log.Warn($"Copy failed ({ex.GetType().Name})."); _dialogs.Error("Copy", "The image could not be copied to the clipboard."); }
    }

    // ---- slideshow ----
    private void ToggleSlideshow()
    {
        if (IsSlideshow) { StopSlideshow(); return; }
        if (_files.Count == 0) return;
        if (Mode == ViewMode.Gallery) OpenSelected();
        IsSlideshow = true;
        if (Settings.SlideshowFullscreen && !IsFullscreen) { IsFullscreen = true; _fullscreenBySlideshow = true; }
        _slideTimer.Interval = TimeSpan.FromSeconds(Settings.SlideshowSeconds);
        _slideTimer.Start();
    }

    private void StopSlideshow()
    {
        if (!IsSlideshow) return;
        _slideTimer.Stop(); IsSlideshow = false;
        if (_fullscreenBySlideshow) IsFullscreen = false;
        _fullscreenBySlideshow = false;
    }

    private void SlideTick()
    {
        if (_files.Count == 0) { StopSlideshow(); return; }
        if (_index >= _files.Count - 1 && !Settings.SlideshowLoop) { StopSlideshow(); return; }
        _ = ShowIndexAsync((_index + 1) % _files.Count);
    }

    // ---- settings ----
    private void OpenSettings()
    {
        var edited = _dialogs.ShowSettings(Settings.Clone(), () => Updates?.CheckAsync(true) ?? Task.CompletedTask);
        if (edited == null) return;
        ApplySettings(edited);
    }

    public void ApplySettings(AppSettings s)
    {
        s.Normalize();
        bool sortChanged = s.Sort != Settings.Sort || s.SortDescending != Settings.SortDescending;
        bool thumbChanged = s.ThumbnailSize != Settings.ThumbnailSize || s.ThumbnailCacheItems != Settings.ThumbnailCacheItems || s.ThumbnailParallelism != Settings.ThumbnailParallelism;
        // keep values the settings window does not edit
        s.RecentFiles = Settings.RecentFiles; s.LastUpdateCheckUtc = Settings.LastUpdateCheckUtc; s.SkippedVersion = Settings.SkippedVersion;
        s.WindowLeft = Settings.WindowLeft; s.WindowTop = Settings.WindowTop; s.WindowWidth = Settings.WindowWidth; s.WindowHeight = Settings.WindowHeight; s.WindowMaximized = Settings.WindowMaximized;
        Settings = s;
        _theme.Apply(s.Theme);
        if (_slideTimer.IsEnabled) _slideTimer.Interval = TimeSpan.FromSeconds(s.SlideshowSeconds);
        if (thumbChanged) { _thumbs.Reconfigure(s.ThumbnailSize, s.ThumbnailCacheItems, s.ThumbnailParallelism); Gallery.SetThumbSize(s.ThumbnailSize); foreach (var r in Gallery.Rows) foreach (var i in r.Items) i.Refresh(); }
        if (sortChanged && !string.IsNullOrEmpty(_folderPath)) Reload();
        SaveSettings();
        OnPropertyChanged(nameof(Settings));
    }

    public void SaveSettings() => _settingsService.Save(Settings);
    public void RefreshRecent() { RecentFiles.Clear(); foreach (var p in Settings.RecentFiles.Where(File.Exists)) RecentFiles.Add(p); }
}
