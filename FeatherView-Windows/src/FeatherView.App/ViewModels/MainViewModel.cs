using System.Collections.ObjectModel;
using System.IO;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Media;
using FeatherView.Core.Mvvm;
using FeatherView.Core.Settings;
using FeatherView.Core.Tabs;
using FeatherView.Services;

namespace FeatherView.ViewModels;

public enum ViewMode { Photo, Gallery, Video }
public sealed record FolderItem(string Name, string Path);
public enum NavSection { Gallery, Folders, Favorites, Recent, Search }

/// <summary>The view surface (zoom/pan control) as seen by the view-model.</summary>
public interface IViewport { void ZoomIn(); void ZoomOut(); void Fit(); void ActualSize(); void ZoomTo(double factor); }

public sealed class MainViewModel : ObservableObject
{
    private readonly SettingsService _settingsService;
    private readonly ILog _log;
    private readonly ThumbnailService _thumbs;
    private readonly IDialogService _dialogs;
    private readonly ThemeService _theme;
    private readonly DispatcherTimer _slideTimer = new();

    private List<MediaFile> _source = new();      // everything in the current folder/collection
    private List<MediaFile> _files = new();       // what the quick filter and search leave (indexes refer to this list)
    private QuickFilter _filter = QuickFilter.All;
    private string _searchText = "";
    private bool _showSearch;
    private NavSection _section = NavSection.Gallery;
    private bool _xDetails = true, _xCamera = true, _xLocation = true, _xExif, _xHist, _xText;
    private string _cameraSummary = "", _locationText = "", _ocrText = "", _ocrStatus = "", _compareName = "";
    private string? _locationUrl;
    private bool _compare;
    private ImageSource? _compareImage;
    private object? _compareId;
    private CancellationTokenSource? _cmpCts;
    private readonly ShuffleOrder _shuffle = new();
    private Dictionary<QuickFilter, int> _counts = Enum.GetValues<QuickFilter>().ToDictionary(k => k, _ => 0);
    private string _emptyMessage = "Open a photo or a folder, or drop them here.";
    private string _infoTitle = "", _infoSubtitle = "", _infoDimensions = "";
    private HistogramShapes? _histogram;
    private CancellationTokenSource? _histCts;
    private int _index = -1;
    private CancellationTokenSource? _loadCts;
    private BitmapSource? _base, _gifFrame;
    private GifAnimator? _gif;
    private int _rotation;
    private bool _flip, _fullscreenBySlideshow;

    private ViewMode _mode = ViewMode.Gallery, _lastViewMode = ViewMode.Photo;
    private ImageSource? _currentImage;
    private object? _imageId;
    private string? _errorText;
    private string _statusText = "";
    private string _zoomText = "";
    private bool _isSlideshow, _isFullscreen, _showInfo;
    private string _folderPath = "";
    private bool _folderTruncated;
    private Guid _videoOwner;      // the tab that last showed the video that is loaded in the player

    public MainViewModel(AppSettings settings, SettingsService settingsService, ThumbnailService thumbs,
                         IDialogService dialogs, ThemeService theme, ILog log, Func<MainViewModel, VideoPlayerViewModel> videoFactory)
    {
        Settings = settings; _settingsService = settingsService; _thumbs = thumbs; _dialogs = dialogs; _theme = theme; _log = log;
        Gallery = new GalleryViewModel();
        Gallery.SetThumbSize(settings.ThumbnailSize);
        MediaItemViewModel.CheckedChanged += () => { OnPropertyChanged(nameof(SelectedCountText)); OnPropertyChanged(nameof(CheckedCount)); OnPropertyChanged(nameof(HasChecked)); };
        Gallery.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(GalleryViewModel.SelectedIndex)) OnPropertyChanged(nameof(SelectedCountText)); };
        Video = videoFactory(this);
        Tabs = new TabsViewModel(this, dialogs);
        Video.InfoReady += lines => { Info.Clear(); foreach (var (l, v) in lines) Info.Add(new InfoLine(l, v)); };
        Video.ToggleFullscreenRequested += () => IsFullscreen = !IsFullscreen;
        Video.PropertyChanged += (_, e) =>
        {
            if (e.PropertyName is nameof(VideoPlayerViewModel.IsPlaying) or nameof(VideoPlayerViewModel.HasMedia))
                Tabs.SetPlayingTab(Video.HasMedia && Video.IsPlaying ? _videoOwner : null);
        };
        _showInfo = settings.ShowInfoPanelOnOpen;
        _slideTimer.Tick += (_, _) => SlideTick();

        OpenFileCommand = new RelayCommand(OpenFile);
        OpenFolderCommand = new RelayCommand(OpenFolder);
        OpenRecentCommand = new RelayCommand(p => { if (p is string s) OpenPath(s); });
        NextCommand = new RelayCommand(() => Step(1));
        PrevCommand = new RelayCommand(() => Step(-1));
        NextFileCommand = new RelayCommand(() => StepFile(1));
        PrevFileCommand = new RelayCommand(() => StepFile(-1));
        SpaceCommand = new RelayCommand(() => { if (Mode == ViewMode.Video) Video.TogglePlay(); else Step(1); });
        SeekLongBackCommand = new RelayCommand(() => { if (Mode == ViewMode.Video) Video.SeekRelative(-30); else MoveCursor("L"); });
        SeekLongForwardCommand = new RelayCommand(() => { if (Mode == ViewMode.Video) Video.SeekRelative(30); else MoveCursor("R"); });
        UpCommand = new RelayCommand(() => { if (Mode == ViewMode.Gallery) Gallery.MoveRows(-1); else if (Mode == ViewMode.Video) Video.Volume += 5; });
        DownCommand = new RelayCommand(() => { if (Mode == ViewMode.Gallery) Gallery.MoveRows(1); else if (Mode == ViewMode.Video) Video.Volume -= 5; });
        FirstCommand = new RelayCommand(() => JumpTo(0));
        LastCommand = new RelayCommand(() => JumpTo(_files.Count - 1));
        ToggleGalleryCommand = new RelayCommand(ToggleGallery);
        OpenItemCommand = new RelayCommand(p => { if (p is MediaItemViewModel m) { Gallery.Select(_files.IndexOf(m.File)); OpenSelected(); } });
        SelectItemCommand = new RelayCommand(p => { if (p is MediaItemViewModel m) { _anchor = _files.IndexOf(m.File); Gallery.Select(_anchor); } });
        OpenSelectedCommand = new RelayCommand(OpenSelected);
        RotateLeftCommand = new RelayCommand(() => { if (Mode == ViewMode.Video) Video.RotateCommand.Execute(null); else Rotate(-1); });
        RotateRightCommand = new RelayCommand(() => { if (Mode == ViewMode.Video) Video.RotateCommand.Execute(null); else Rotate(1); });
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
        AboutCommand = new RelayCommand(() => _dialogs.Info("Feather View", $"Feather View {typeof(MainViewModel).Assembly.GetName().Version?.ToString(3)}\n\nA lightweight, offline photo and video viewer. No ads, no telemetry.\nDesigned and developed by Akshaya Simha."));
        OpenFolderInNewTabCommand = new RelayCommand(() => { var p = _dialogs.PickFolder(); if (p != null) Tabs.OpenInNewTab(p); });
        OpenFolderPathCommand = new RelayCommand(p => { if (p is string path && Directory.Exists(path)) LoadFolder(path, null, gallery: true); });
        GoUpCommand = new RelayCommand(GoUp, () => CanGoUp);
        ToggleSubfoldersCommand = new RelayCommand(() => { Settings.IncludeSubfolders = !Settings.IncludeSubfolders; SaveSettings(); OnPropertyChanged(nameof(IncludeSubfolders)); OnPropertyChanged(nameof(ShowFolderChips)); Reload(); });
        ShowSectionCommand = new RelayCommand(p => ShowSection(p as string));
        SetFilterCommand = new RelayCommand(p => { if (p is string n && Enum.TryParse<QuickFilter>(n, out var f)) { Filter = f; ApplyFilter(); } });
        ClearSearchCommand = new RelayCommand(() => { SearchText = ""; ShowSearch = false; Section = NavSection.Gallery; });
        ToggleFavoriteCommand = new RelayCommand(ToggleFavorite);
        SetWallpaperCommand = new RelayCommand(SetWallpaper);
        PrintCommand = new RelayCommand(PrintImage);
        ToggleFilmstripCommand = new RelayCommand(() => { Settings.ShowFilmstrip = !Settings.ShowFilmstrip; OnPropertyChanged(nameof(FilmstripVisible)); SaveSettings(); });
        ToggleSidebarCommand = new RelayCommand(() => { Settings.ShowSidebar = !Settings.ShowSidebar; OnPropertyChanged(nameof(ShowSidebar)); SaveSettings(); });
        ZoomPresetCommand = new RelayCommand(p =>
        {
            if (p is not string t) return;
            if (t == "fit") Viewport?.Fit();
            else if (double.TryParse(t, System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var z)) Viewport?.ZoomTo(z);
        });
        CompareCommand = new RelayCommand(() => { if (Mode == ViewMode.Photo) IsCompare = !IsCompare; });
        ToggleShuffleCommand = new RelayCommand(() => { Settings.SlideshowShuffle = !Settings.SlideshowShuffle; _shuffle.Reset(); OnPropertyChanged(nameof(IsShuffle)); SaveSettings(); });
        ExtractTextCommand = new AsyncRelayCommand(ExtractTextAsync);
        CopyOcrTextCommand = new RelayCommand(() => { if (_ocrText.Length > 0) { try { Clipboard.SetText(_ocrText); StatusText = "Text copied"; } catch { } } });
        OpenLocationCommand = new RelayCommand(OpenLocation);
        ShowInExplorerCommand = new RelayCommand(() => { if (Current != null) FileOperations.ShowInExplorer(Current.Path); });
        SettingsCommand = new RelayCommand(OpenSettings);
        CheckUpdatesCommand = new AsyncRelayCommand(() => Updates?.CheckAsync(true) ?? Task.CompletedTask);
    }

    // ---- state exposed to the view ----
    public AppSettings Settings { get; private set; }
    public GalleryViewModel Gallery { get; }
    /// <summary>Folders inside the opened folder, shown above the pictures so you can go into them.</summary>
    public ObservableCollection<FolderItem> SubFolders { get; } = new();
    public bool ShowFolderStrip => !string.IsNullOrEmpty(_folderPath) && Section == NavSection.Gallery;
    public bool ShowFolderChips => !Settings.IncludeSubfolders;
    public bool HasSubFolders => SubFolders.Count > 0;
    public string FolderDisplay => _folderPath;
    public bool CanGoUp => !string.IsNullOrEmpty(_folderPath) && Path.GetDirectoryName(_folderPath.TrimEnd('\\', '/')) is { Length: > 0 };
    public bool IncludeSubfolders => Settings.IncludeSubfolders;
    public VideoPlayerViewModel Video { get; }
    public TabsViewModel Tabs { get; }
    public string? SessionPath { get; set; }
    public IViewport? Viewport { get; set; }
    public UpdateController? Updates { get; set; }
    public ObservableCollection<InfoLine> Info { get; } = new();
    public ObservableCollection<string> RecentFiles { get; } = new();

    public ViewMode Mode { get => _mode; private set { if (Set(ref _mode, value)) { OnPropertyChanged(nameof(IsGallery)); OnPropertyChanged(nameof(IsPhoto)); OnPropertyChanged(nameof(IsVideo)); OnPropertyChanged(nameof(ShowEmptyState)); OnPropertyChanged(nameof(FilmstripVisible)); if (value != ViewMode.Photo && _compare) IsCompare = false; Video.SetBackground(value != ViewMode.Video); } } }
    public bool IsGallery => _mode == ViewMode.Gallery;
    public bool IsPhoto => _mode == ViewMode.Photo;
    public bool IsVideo => _mode == ViewMode.Video;
    public ImageSource? CurrentImage { get => _currentImage; private set => Set(ref _currentImage, value); }
    public object? ImageId { get => _imageId; private set => Set(ref _imageId, value); }
    public string? ErrorText { get => _errorText; private set { Set(ref _errorText, value); OnPropertyChanged(nameof(HasError)); } }
    public bool HasError => !string.IsNullOrEmpty(_errorText);
    public string StatusText { get => _statusText; private set => Set(ref _statusText, value); }
    public string ZoomText { get => _zoomText; private set => Set(ref _zoomText, value); }
    public bool ShowEmptyState => _files.Count == 0;
    public bool IsSlideshow { get => _isSlideshow; private set { if (Set(ref _isSlideshow, value)) OnPropertyChanged(nameof(SlideshowIcon)); } }
    public bool IsFullscreen { get => _isFullscreen; set { if (Set(ref _isFullscreen, value)) { OnPropertyChanged(nameof(ShowChrome)); OnPropertyChanged(nameof(FilmstripVisible)); Video.SetFullscreen(value); } } }
    public bool ShowChrome => !_isFullscreen;
    public bool ShowInfo { get => _showInfo; set => Set(ref _showInfo, value); }
    public string SlideshowIcon => _isSlideshow ? "Pause" : "Play";
    public MediaFile? Current => _index >= 0 && _index < _files.Count ? _files[_index] : null;

    // ---- sidebar, filters, search ----
    public NavSection Section { get => _section; private set => Set(ref _section, value); }
    public QuickFilter Filter { get => _filter; private set => Set(ref _filter, value); }
    public string SearchText { get => _searchText; set { if (Set(ref _searchText, value)) ApplyFilter(); } }
    public bool ShowSearch { get => _showSearch; private set => Set(ref _showSearch, value); }
    public event Action? FocusSearchRequested;
    public int CountAll => _counts[QuickFilter.All];
    public int CountJpg => _counts[QuickFilter.Jpg];
    public int CountPng => _counts[QuickFilter.Png];
    public int CountGif => _counts[QuickFilter.Gif];
    public int CountVideo => _counts[QuickFilter.Video];
    public int CountOther => _counts[QuickFilter.Other];
    public string ItemCountText => _files.Count == 1 ? "1 item" : $"{_files.Count} items";
    public string SelectedCountText => CheckedCount > 0 ? $"{CheckedCount} selected" : Gallery.SelectedIndex >= 0 && _files.Count > 0 ? "1 selected" : "0 selected";
    public int CheckedCount => Gallery.Items.Count(i => i.IsChecked);
    public bool HasChecked => CheckedCount > 0;
    public RelayCommand SelectAllCommand => _selAll ??= new RelayCommand(() => { foreach (var i in Gallery.Items) i.IsChecked = true; });
    public RelayCommand ClearCheckedCommand => _clrChk ??= new RelayCommand(() => { foreach (var i in Gallery.Items) i.IsChecked = false; });
    private RelayCommand? _selAll, _clrChk;
    public string EmptyMessage { get => _emptyMessage; private set => Set(ref _emptyMessage, value); }
    public bool ShowSidebar => Settings.ShowSidebar;
    public bool FilmstripVisible => Settings.ShowFilmstrip && _mode != ViewMode.Gallery && _files.Count > 1 && !_isFullscreen;
    public bool IsFavorite => Current != null && Settings.IsFavorite(Current.Path);

    // ---- details panel ----
    public bool ExpandDetails { get => _xDetails; set => Set(ref _xDetails, value); }
    public bool ExpandCamera { get => _xCamera; set => Set(ref _xCamera, value); }
    public bool ExpandLocation { get => _xLocation; set => Set(ref _xLocation, value); }
    public bool ExpandExif { get => _xExif; set => Set(ref _xExif, value); }
    public bool ExpandHistogram { get => _xHist; set { if (Set(ref _xHist, value) && value) _ = RefreshHistogramAsync(); } }
    public bool ExpandText { get => _xText; set => Set(ref _xText, value); }
    public string CameraSummary { get => _cameraSummary; private set { Set(ref _cameraSummary, value); OnPropertyChanged(nameof(HasCameraCard)); } }
    public bool HasCameraCard => _cameraSummary.Length > 0 || TakenLines.Count > 0;
    public string LocationText { get => _locationText; private set { Set(ref _locationText, value); OnPropertyChanged(nameof(HasLocation)); } }
    public bool HasLocation => _locationText.Length > 0;
    public string OcrText { get => _ocrText; private set { Set(ref _ocrText, value); OnPropertyChanged(nameof(HasOcrText)); } }
    public bool HasOcrText => _ocrText.Length > 0;
    public string OcrStatus { get => _ocrStatus; private set => Set(ref _ocrStatus, value); }

    // ---- side-by-side comparison and shuffle ----
    public bool IsCompare
    {
        get => _compare;
        set
        {
            if (!Set(ref _compare, value)) return;
            if (value) _ = LoadCompareAsync();
            else { _cmpCts?.Cancel(); CompareImage = null; CompareId = null; CompareName = ""; }
        }
    }
    public ImageSource? CompareImage { get => _compareImage; private set => Set(ref _compareImage, value); }
    public object? CompareId { get => _compareId; private set => Set(ref _compareId, value); }
    public string CompareName { get => _compareName; private set => Set(ref _compareName, value); }
    public bool IsShuffle => Settings.SlideshowShuffle;
    public string InfoTitle { get => _infoTitle; private set => Set(ref _infoTitle, value); }
    public string InfoSubtitle { get => _infoSubtitle; private set => Set(ref _infoSubtitle, value); }
    public string InfoDimensions { get => _infoDimensions; private set => Set(ref _infoDimensions, value); }
    public ObservableCollection<InfoLine> TakenLines { get; } = new();
    public ObservableCollection<InfoLine> ExifLines { get; } = new();
    public bool HasTaken => TakenLines.Count > 0;
    public bool HasExif => ExifLines.Count > 0;
    public HistogramShapes? Histogram { get => _histogram; private set { Set(ref _histogram, value); OnPropertyChanged(nameof(HasHistogram)); } }
    public bool HasHistogram => _histogram != null;

    public RelayCommand OpenFileCommand { get; }
    public RelayCommand OpenFolderCommand { get; }
    public RelayCommand OpenRecentCommand { get; }
    public RelayCommand NextCommand { get; }
    public RelayCommand NextFileCommand { get; }
    public RelayCommand PrevFileCommand { get; }
    public RelayCommand SpaceCommand { get; }
    public RelayCommand SeekLongBackCommand { get; }
    public RelayCommand SeekLongForwardCommand { get; }
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
    public RelayCommand AboutCommand { get; }
    public RelayCommand OpenFolderPathCommand { get; }
    public RelayCommand GoUpCommand { get; }
    public RelayCommand ToggleSubfoldersCommand { get; }
    public RelayCommand OpenFolderInNewTabCommand { get; }
    public RelayCommand ShowSectionCommand { get; }
    public RelayCommand SetFilterCommand { get; }
    public RelayCommand ClearSearchCommand { get; }
    public RelayCommand ToggleFavoriteCommand { get; }
    public RelayCommand SetWallpaperCommand { get; }
    public RelayCommand PrintCommand { get; }
    public RelayCommand ToggleFilmstripCommand { get; }
    public RelayCommand ToggleSidebarCommand { get; }
    public RelayCommand ZoomPresetCommand { get; }
    public RelayCommand CompareCommand { get; }
    public RelayCommand ToggleShuffleCommand { get; }
    public AsyncRelayCommand ExtractTextCommand { get; }
    public RelayCommand CopyOcrTextCommand { get; }
    public RelayCommand OpenLocationCommand { get; }
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
            LoadFolder(Path.GetDirectoryName(path)!, path, gallery: false, recursive: false);
            Settings.AddRecent(path); SaveSettings(); RefreshRecent();
        }
        catch (Exception ex) { _log.Error("Open failed", ex); _dialogs.Error("Open", "The location could not be opened."); }
    }

    private CancellationTokenSource? _scanCts;
    private bool _scanning;
    public bool IsScanning { get => _scanning; private set => Set(ref _scanning, value); }

    /// <summary>
    /// Opens a folder. With "include subfolders" (the default) the whole tree is scanned on a background thread with a live count,
    /// then every picture appears at once; otherwise just this folder is read immediately.
    /// </summary>
    private void LoadFolder(string folder, string? selectFile, bool gallery, Action? onLoaded = null, bool? recursive = null)
    {
        StopSlideshow();
        _scanCts?.Cancel();
        _folderPath = folder;
        Section = NavSection.Gallery; ShowSearch = false; _searchText = ""; OnPropertyChanged(nameof(SearchText));
        Filter = QuickFilter.All;
        SubFolders.Clear();
        foreach (var name in MediaScanner.ListFolders(folder)) SubFolders.Add(new FolderItem(name, Path.Combine(folder, name)));
        OnPropertyChanged(nameof(ShowFolderStrip)); OnPropertyChanged(nameof(FolderDisplay)); OnPropertyChanged(nameof(CanGoUp)); OnPropertyChanged(nameof(HasSubFolders)); GoUpCommand.RaiseCanExecuteChanged();
        bool recurse = recursive ?? Settings.IncludeSubfolders;

        if (!recurse)
        {
            IsScanning = false;
            FinishLoad(folder, selectFile, gallery, MediaScanner.ScanDetailed(folder, Settings.Sort, Settings.SortDescending), onLoaded);
            return;
        }

        // show an empty, labelled view right away; the pictures arrive when the scan is done
        var cts = _scanCts = new CancellationTokenSource();
        IsScanning = true;
        StopGif(); Video.Stop();
        _source = new(); EmptyMessage = "Looking for pictures and videos in this folder and all folders underneath...";
        RebuildFiles(null, 0, out _);
        CurrentImage = null; Mode = ViewMode.Gallery; StatusText = "Scanning...";
        Tabs.UpdateActiveTitle();
        _ = ScanThenFinishAsync(folder, selectFile, gallery, cts, onLoaded);
    }

    private async Task ScanThenFinishAsync(string folder, string? selectFile, bool gallery, CancellationTokenSource cts, Action? onLoaded)
    {
        var progress = new Progress<int>(n => { if (!cts.IsCancellationRequested) StatusText = $"Scanning... {n:N0} pictures and videos found"; });
        var sort = Settings.Sort; var desc = Settings.SortDescending;
        try
        {
            var scan = await Task.Run(() => MediaScanner.ScanDetailed(folder, sort, desc, recurse: true, ct: cts.Token, progress: progress), cts.Token);
            if (cts.IsCancellationRequested) return;
            FinishLoad(folder, selectFile, gallery, scan, onLoaded);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { _log.Warn($"Folder scan failed ({ex.GetType().Name})."); if (!cts.IsCancellationRequested) StatusText = "The folder could not be read."; }
        finally { if (ReferenceEquals(_scanCts, cts)) IsScanning = false; }
    }

    private void FinishLoad(string folder, string? selectFile, bool gallery, ScanResult scan, Action? onLoaded)
    {
        var list = scan.Files;
        _folderTruncated = scan.Truncated;
        if (selectFile != null && !list.Any(f => f.Path.Equals(selectFile, StringComparison.OrdinalIgnoreCase)) && MediaTypes.Classify(selectFile) != MediaKind.Unknown)
        {
            var fi = new FileInfo(selectFile);   // file the scanner skipped (e.g. hidden)
            list.Insert(0, new MediaFile(fi.FullName, fi.Name, fi.Length, fi.LastWriteTimeUtc, MediaTypes.Classify(selectFile)));
        }
        _source = list;
        EmptyMessage = SubFolders.Count > 0 && !Settings.IncludeSubfolders
            ? "There are no pictures or videos directly in this folder. Open one of the folders above, or turn on 'Include subfolders' in the ... menu."
            : Settings.IncludeSubfolders && SubFolders.Count > 0 ? "No pictures or videos were found in this folder or in the folders underneath." : "Open a photo or a folder, or drop them here.";
        int idx = RebuildFiles(selectFile, 0, out _);
        Tabs.UpdateActiveTitle();
        if (_files.Count == 0) { Mode = ViewMode.Gallery; CurrentImage = null; StatusText = "No photos or videos found"; onLoaded?.Invoke(); return; }
        if (_folderTruncated) StatusText = $"Showing the first {_files.Count:N0} items (the limit). Open a smaller folder to see everything.";
        else if (gallery) StatusText = $"{_files.Count:N0} pictures and videos";
        if (gallery) { Mode = ViewMode.Gallery; _index = idx; }
        else { Mode = ViewMode.Photo; _ = ShowIndexAsync(idx); }
        onLoaded?.Invoke();
    }

    /// <summary>Applies the quick filter and search to the source list and refreshes counts and the gallery. Returns the selected index.</summary>
    private int RebuildFiles(string? keepPath, int fallbackIndex, out bool found)
    {
        _files = _source.Where(f => MediaFilter.Matches(f, _filter, _searchText)).ToList();
        _counts = MediaFilter.Count(_source);
        int idx = keepPath == null ? -1 : _files.FindIndex(f => f.Path.Equals(keepPath, StringComparison.OrdinalIgnoreCase));
        List<string>? labels = null;
        if (Settings.GroupBy != GroupMode.None)
        {
            var keep = keepPath ?? (fallbackIndex >= 0 && fallbackIndex < _files.Count ? _files[fallbackIndex].Path : null);
            var g = MediaGrouping.Apply(_files, Settings.GroupBy, Settings.SortDescending);
            _files = g.Files; labels = g.Labels;
            if (keep != null) { idx = _files.FindIndex(f => f.Path.Equals(keep, StringComparison.OrdinalIgnoreCase)); if (keepPath == null && idx >= 0) fallbackIndex = idx; }
        }
        found = idx >= 0;
        if (!found) idx = Math.Clamp(fallbackIndex, 0, Math.Max(0, _files.Count - 1));
        Gallery.SetItems(_files.Select(f => new MediaItemViewModel(f, _thumbs)), idx, labels);
        _index = _files.Count == 0 ? -1 : idx;
        foreach (var n in new[] { nameof(CountAll), nameof(CountJpg), nameof(CountPng), nameof(CountGif), nameof(CountVideo), nameof(CountOther), nameof(ItemCountText),
                                  nameof(SelectedCountText), nameof(ShowEmptyState), nameof(FilmstripVisible), nameof(IsFavorite) })
            OnPropertyChanged(n);
        return idx;
    }

    /// <summary>Called when the quick filter or the search text changes.</summary>
    private void ApplyFilter()
    {
        var keep = Current?.Path;
        RebuildFiles(keep, 0, out var found);
        if (_files.Count == 0)
        {
            StopVideoUnlessBackground(); StopGif(); CurrentImage = null; Mode = ViewMode.Gallery;
            StatusText = _searchText.Length > 0 || _filter != QuickFilter.All ? "Nothing matches this filter" : StatusText;
            EmptyMessage = _searchText.Length > 0 || _filter != QuickFilter.All ? "Nothing matches this filter." : EmptyMessage;
            return;
        }
        if (Mode != ViewMode.Gallery && !found) _ = ShowIndexAsync(_index);   // the open file was filtered out: show the first match instead
        else UpdateStatus();
    }

    /// <summary>Stops the video unless it is playing for a different tab and background play is on.</summary>
    private void StopVideoUnlessBackground()
    {
        if (BackgroundVideoPolicy.ShouldStopForOtherContent(Settings.KeepVideoPlayingAcrossTabs, Video.HasMedia, _videoOwner, Tabs.ActiveId)) Video.Stop();
    }

    /// <summary>Called after tabs change: a background video whose tab was closed is stopped.</summary>
    public void ValidateVideoOwner(IReadOnlyCollection<Guid> openTabs)
    {
        if (BackgroundVideoPolicy.OwnerGone(Video.HasMedia, _videoOwner, openTabs)) Video.Stop();
    }

    private void ShowSection(string? name)
    {
        if (name == null || !Enum.TryParse<NavSection>(name, out var s)) return;
        switch (s)
        {
            case NavSection.Gallery:
                if (Section != NavSection.Gallery && !string.IsNullOrEmpty(_folderPath)) { LoadFolder(_folderPath, Current?.Path, gallery: true); return; }
                ShowSearch = false; Section = NavSection.Gallery;
                if (_searchText.Length > 0) { _searchText = ""; OnPropertyChanged(nameof(SearchText)); ApplyFilter(); }
                if (_files.Count > 0 && Mode != ViewMode.Gallery) ToggleGallery();
                break;
            case NavSection.Folders: OpenFolder(); break;
            case NavSection.Favorites: LoadCollection(Settings.Favorites, NavSection.Favorites, "No favorites yet. Press Ctrl+D on a picture or video to add it."); break;
            case NavSection.Recent: LoadCollection(Settings.RecentFiles, NavSection.Recent, "Nothing opened recently."); break;
            case NavSection.Search:
                Section = NavSection.Search; ShowSearch = true;
                if (_files.Count > 0 && Mode != ViewMode.Gallery) ToggleGallery();
                FocusSearchRequested?.Invoke();
                break;
        }
    }

    private void LoadCollection(IEnumerable<string> paths, NavSection section, string emptyMessage)
    {
        StopSlideshow(); StopGif(); StopVideoUnlessBackground();
        Section = section; ShowSearch = false; _searchText = ""; OnPropertyChanged(nameof(SearchText)); Filter = QuickFilter.All;
        var list = new List<MediaFile>();
        foreach (var p in paths)
        {
            try
            {
                var kind = MediaTypes.Classify(p);
                if (kind == MediaKind.Unknown || !File.Exists(p)) continue;
                var fi = new FileInfo(p);
                list.Add(new MediaFile(fi.FullName, fi.Name, fi.Length, fi.LastWriteTimeUtc, kind));
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        }
        _source = list; EmptyMessage = emptyMessage;
        RebuildFiles(null, 0, out _);
        Mode = ViewMode.Gallery; CurrentImage = null; _index = _files.Count == 0 ? -1 : 0;
        StatusText = _files.Count == 0 ? emptyMessage : "";
        Tabs.UpdateActiveTitle();
    }

    private void ToggleFavorite()
    {
        var f = Mode == ViewMode.Gallery ? Gallery.Selected?.File : Current;
        if (f == null) return;
        bool now = Settings.ToggleFavorite(f.Path);
        SaveSettings();
        OnPropertyChanged(nameof(IsFavorite));
        if (Section == NavSection.Favorites) { var keep = Current?.Path; LoadCollection(Settings.Favorites, NavSection.Favorites, EmptyMessage); }
        else StatusText = now ? "Added to favorites" : "Removed from favorites";
    }

    private void SetWallpaper()
    {
        if (CurrentImage is not BitmapSource bs || Current == null) { _dialogs.Info("Set as wallpaper", "Open a picture first."); return; }
        if (!_dialogs.Confirm("Set as wallpaper", $"Use \"{Current.Name}\" as your desktop background?", "Set wallpaper")) return;
        var folder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "FeatherView");
        if (FileOperations.SetWallpaper(bs, folder)) StatusText = "Desktop wallpaper changed";
        else _dialogs.Error("Set as wallpaper", "Windows could not change the wallpaper.");
    }

    private void PrintImage()
    {
        if (CurrentImage is not BitmapSource bs || Current == null) { _dialogs.Info("Print", "Open a picture first."); return; }
        try { FileOperations.Print(bs, Current.Name); }
        catch (Exception ex) { _log.Warn($"Print failed ({ex.GetType().Name})."); _dialogs.Error("Print", "The picture could not be printed."); }
    }

    private async Task LoadCompareAsync()
    {
        _cmpCts?.Cancel();
        var cts = _cmpCts = new CancellationTokenSource();
        int partner = -1;
        for (int k = 1; k < _files.Count; k++) { int i = (_index + k) % _files.Count; if (_files[i].Kind == MediaKind.Image) { partner = i; break; } }
        if (partner < 0) { IsCompare = false; StatusText = "Comparing needs at least two pictures"; return; }
        var f = _files[partner];
        try
        {
            var li = await Task.Run(() => ImageLoader.Load(f.Path), cts.Token);
            if (cts.IsCancellationRequested || !_compare) return;
            CompareImage = li.Image; CompareId = f.Path; CompareName = f.Name;
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { _log.Warn($"Compare picture failed ({ex.GetType().Name})."); CompareImage = null; CompareName = f.Name + " (cannot open)"; }
    }

    private async Task ExtractTextAsync()
    {
        if (CurrentImage is not BitmapSource bs) { OcrStatus = "Open a picture first."; return; }
        OcrStatus = "Reading text..."; OcrText = "";
        var r = await OcrService.RecognizeAsync(bs);
        OcrText = r.Text ?? "";
        OcrStatus = r.Error ?? "Text found. Select it, or copy it with the button.";
    }

    private void OpenLocation()
    {
        if (_locationUrl == null) return;
        if (!_dialogs.Confirm("Open map", "This opens the picture's location in your web browser (OpenStreetMap) and tells that website the coordinates. Continue?", "Open map")) return;
        try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(_locationUrl) { UseShellExecute = true }); }
        catch (Exception ex) { _log.Warn($"Map not opened ({ex.GetType().Name})."); }
    }

    private async Task RefreshHistogramAsync()
    {
        _histCts?.Cancel();
        if (!_xHist || CurrentImage is not BitmapSource bs) { Histogram = null; return; }
        var cts = _histCts = new CancellationTokenSource();
        var shapes = await Task.Run(() => HistogramService.Compute(bs));
        if (!cts.IsCancellationRequested) Histogram = shapes;
    }

    // ---- tab state ----
    public string FolderPath => _folderPath;

    public void CaptureTabState(TabState t)
    {
        t.Section = Section == NavSection.Search ? "Gallery" : Section.ToString();
        t.FolderPath = string.IsNullOrEmpty(_folderPath) ? null : _folderPath;
        t.SelectedPath = Current?.Path ?? Gallery.Selected?.File.Path;
        t.Filter = Filter.ToString();
        t.Search = _searchText;
        t.Mode = Mode.ToString();
    }

    /// <summary>Shows a tab's saved state: re-reads its folder (cheap) instead of keeping anything heavy in memory.</summary>
    public void RestoreTabState(TabState t)
    {
        StopSlideshow(); StopGif();
        if (BackgroundVideoPolicy.ShouldStopOnTabSwitch(Settings.KeepVideoPlayingAcrossTabs, Video.HasMedia)) Video.Stop();   // otherwise the video carries on (or is simply shown again)
        if (t.Section == nameof(NavSection.Favorites)) { LoadCollection(Settings.Favorites, NavSection.Favorites, "No favorites yet. Press Ctrl+D on a picture or video to add it."); ApplyTabFilter(t); return; }
        if (t.Section == nameof(NavSection.Recent)) { LoadCollection(Settings.RecentFiles, NavSection.Recent, "Nothing opened recently."); ApplyTabFilter(t); return; }
        if (string.IsNullOrEmpty(t.FolderPath) || !Directory.Exists(t.FolderPath)) { ClearView(); return; }
        LoadFolder(t.FolderPath, t.SelectedPath, gallery: t.Mode == nameof(ViewMode.Gallery), onLoaded: () => ApplyTabFilter(t));
    }

    private void ApplyTabFilter(TabState t)
    {
        bool hasFilter = Enum.TryParse<QuickFilter>(t.Filter, out var f) && f != QuickFilter.All;
        if (!hasFilter && string.IsNullOrEmpty(t.Search)) return;
        if (hasFilter) Filter = Enum.Parse<QuickFilter>(t.Filter);
        _searchText = t.Search ?? ""; OnPropertyChanged(nameof(SearchText)); ShowSearch = _searchText.Length > 0;
        if (ShowSearch) Section = NavSection.Search;
        ApplyFilter();
    }

    /// <summary>Empty view for a new tab.</summary>
    private void ClearView()
    {
        _folderPath = ""; _source = new(); Section = NavSection.Gallery; Filter = QuickFilter.All;
        _searchText = ""; OnPropertyChanged(nameof(SearchText)); ShowSearch = false;
        EmptyMessage = "Open a photo or a folder, or drop them here.";
        RebuildFiles(null, 0, out _);
        _base = null; CurrentImage = null; Mode = ViewMode.Gallery; _index = -1; StatusText = "";
        Info.Clear(); TakenLines.Clear(); ExifLines.Clear(); Histogram = null; InfoTitle = InfoSubtitle = InfoDimensions = "";
        OnPropertyChanged(nameof(HasTaken)); OnPropertyChanged(nameof(HasExif)); OnPropertyChanged(nameof(IsFavorite));
        Tabs.UpdateActiveTitle();
    }

    private void GoUp()
    {
        if (string.IsNullOrEmpty(_folderPath)) return;
        var parent = Path.GetDirectoryName(_folderPath.TrimEnd('\\', '/'));
        if (!string.IsNullOrEmpty(parent) && Directory.Exists(parent)) LoadFolder(parent, null, gallery: true);
    }

    private void Reload()
    {
        if (string.IsNullOrEmpty(_folderPath) || Section is NavSection.Favorites or NavSection.Recent) return;
        var keep = Current?.Path;
        LoadFolder(_folderPath, keep, gallery: IsGallery);
    }

    // ---- navigation ----
    /// <summary>Left/Right: previous/next picture, move the gallery selection, or seek inside a video.</summary>
    // ---- multi-selection in the gallery (Explorer-style: Ctrl+click toggles, Shift+click / Shift+arrows select a range) ----
    private int _anchor = -1;

    private void SetRange(int a, int b)
    {
        int lo = Math.Min(a, b), hi = Math.Max(a, b);
        var items = Gallery.Items;
        for (int i = 0; i < items.Count; i++) items[i].IsChecked = i >= lo && i <= hi;
    }

    /// <summary>Click on a gallery tile with the current keyboard modifiers.</summary>
    public void ClickItem(MediaItemViewModel m, ModifierKeys mods)
    {
        int idx = _files.IndexOf(m.File);
        if (idx < 0) return;
        if (_anchor < 0) _anchor = Math.Max(0, Gallery.SelectedIndex);
        if (mods.HasFlag(ModifierKeys.Shift)) { SetRange(_anchor, idx); Gallery.Select(idx); }
        else if (mods.HasFlag(ModifierKeys.Control))
        {
            if (CheckedCount == 0 && Gallery.Selected is { } cur && cur != m) cur.IsChecked = true;   // the tile you were on becomes part of the selection
            m.IsChecked = !m.IsChecked; _anchor = idx; Gallery.Select(idx);
        }
        else { if (HasChecked) ClearCheckedCommand.Execute(null); _anchor = idx; Gallery.Select(idx); }
    }

    /// <summary>Shift+arrow keys: move the cursor and extend the selection from the anchor. "L","R" = one tile; "U","D" = one row; "H","E" = first/last.</summary>
    public void ExtendSelection(string how)
    {
        if (Mode != ViewMode.Gallery || _files.Count == 0) return;
        if (_anchor < 0 || _anchor >= _files.Count) _anchor = Math.Max(0, Gallery.SelectedIndex);
        switch (how)
        {
            case "L": Gallery.Move(-1); break;
            case "R": Gallery.Move(1); break;
            case "U": Gallery.MoveRows(-1); break;
            case "D": Gallery.MoveRows(1); break;
            case "H": Gallery.Select(0, scroll: true); break;
            case "E": Gallery.Select(_files.Count - 1, scroll: true); break;
        }
        SetRange(_anchor, Gallery.SelectedIndex);
    }

    /// <summary>Ctrl+arrow keys: move the cursor without changing the selection.</summary>
    public void MoveCursor(string how)
    {
        if (Mode != ViewMode.Gallery || _files.Count == 0) return;
        switch (how) { case "L": Gallery.Move(-1); break; case "R": Gallery.Move(1); break; case "U": Gallery.MoveRows(-1); break; case "D": Gallery.MoveRows(1); break; }
    }

    private void ToggleCursorChecked()
    {
        if (Mode != ViewMode.Gallery || Gallery.Selected is not { } s) return;
        s.IsChecked = !s.IsChecked; _anchor = Gallery.SelectedIndex;
    }

    public RelayCommand ExtendSelectionCommand => _extCmd ??= new RelayCommand(p => ExtendSelection(p as string ?? ""));
    public RelayCommand MoveCursorCommand => _curCmd ??= new RelayCommand(p => MoveCursor(p as string ?? ""));
    public RelayCommand ToggleCursorCheckedCommand => _tglCmd ??= new RelayCommand(ToggleCursorChecked);
    private RelayCommand? _extCmd, _curCmd, _tglCmd;

    private void Step(int delta)
    {
        if (Mode == ViewMode.Video) { Video.SeekRelative(delta * 5); return; }
        StepFile(delta);
    }

    private void StepFile(int delta)
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
        if (Mode != ViewMode.Gallery) { Gallery.Select(_index, scroll: true); _lastViewMode = Mode; Mode = ViewMode.Gallery; _gif?.Pause(); Video.Pause(); }
        else OpenSelected();
    }

    private void OpenSelected()
    {
        if (_files.Count == 0 || Gallery.SelectedIndex < 0) return;
        if (Gallery.SelectedIndex == _index && (CurrentImage != null || (Current?.Kind == MediaKind.Video && Video.HasMedia)))
        { Mode = Current?.Kind == MediaKind.Video ? ViewMode.Video : ViewMode.Photo; _gif?.Resume(); UpdateStatus(); return; }
        Mode = ViewMode.Photo;
        _ = ShowIndexAsync(Gallery.SelectedIndex);
    }

    private void Escape()
    {
        if (HasChecked) ClearCheckedCommand.Execute(null);
        else if (IsCompare) IsCompare = false;
        else if (IsSlideshow) StopSlideshow();
        else if (IsFullscreen) IsFullscreen = false;
        else if (Mode == ViewMode.Gallery && (CurrentImage != null || Video.HasMedia)) OpenSelected();
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
            _base = null; CurrentImage = null; ImageId = file.Path; Info.Clear(); TakenLines.Clear(); ExifLines.Clear(); Histogram = null;
            InfoTitle = file.Name; InfoSubtitle = $"{Path.GetExtension(file.Name).TrimStart('.').ToUpperInvariant()}  •  {FormatSize(file.Length)}"; InfoDimensions = "";
            OnPropertyChanged(nameof(HasTaken)); OnPropertyChanged(nameof(HasExif)); OnPropertyChanged(nameof(IsFavorite));
            CameraSummary = ""; LocationText = ""; OcrText = ""; OcrStatus = ""; if (_compare) IsCompare = false;
            Mode = ViewMode.Video;
            _videoOwner = Tabs.ActiveId;
            if (Video.IsCurrent(file)) Video.RefreshInfo();   // already loaded (it kept playing in the background): just show it again
            else Video.Open(file);
            return;
        }
        StopVideoUnlessBackground();
        if (Mode == ViewMode.Video) Mode = ViewMode.Photo;
        try
        {
            var li = await Task.Run(() => ImageLoader.Load(file.Path), ct);
            if (ct.IsCancellationRequested) return;
            _base = li.Image; _gifFrame = null; _rotation = 0; _flip = false;
            CurrentImage = _base;
            ImageId = file.Path;
            BuildInfo(file, li);
            UpdateStatus(li.PixelWidth, li.PixelHeight);
            if (_compare) _ = LoadCompareAsync();
            if (li.Gif != null) StartGif(li.Gif);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex)
        {
            if (ct.IsCancellationRequested) return;
            _log.Error($"Could not open an image ({ex.GetType().Name}: {ex.Message}).", ex);
            _base = null; CurrentImage = null; ImageId = file.Path; Info.Clear();
            ErrorText = "This file could not be opened. It may be damaged, or use a format this version of Windows cannot decode.";
        }
    }

    private void StartGif(GifData data)
    {
        try
        {
            _gif = new GifAnimator(data);
            _gif.FrameReady += f => { _gifFrame = f; CurrentImage = ImageLoader.Transform(f, _rotation, _flip); };
            _gif.Start();
        }
        catch (Exception ex) { _log.Warn($"GIF animation unavailable ({ex.GetType().Name})."); _gif = null; }
    }

    private void StopGif() { _gif?.Dispose(); _gif = null; _gifFrame = null; }

    private void BuildInfo(MediaFile f, LoadedImage li)
    {
        var ext = Path.GetExtension(f.Name).TrimStart('.').ToUpperInvariant();
        InfoTitle = f.Name;
        InfoSubtitle = $"{(string.IsNullOrEmpty(ext) ? li.Format : ext)}  •  {FormatSize(f.Length)}";
        var ratio = AspectRatio.Format(li.PixelWidth, li.PixelHeight);
        InfoDimensions = $"{li.PixelWidth} × {li.PixelHeight}" + (ratio.Length > 0 ? $" ({ratio})" : "");
        Info.Clear();
        Info.Add(new InfoLine("Dimensions", $"{li.PixelWidth} × {li.PixelHeight}"));
        if (li.BitsPerPixel > 0) Info.Add(new InfoLine("Color Depth", $"{li.BitsPerPixel} bit"));
        if (li.DpiX > 0) Info.Add(new InfoLine("DPI", $"{Math.Round(li.DpiX)} × {Math.Round(li.DpiY)}"));
        Info.Add(new InfoLine("Size", FormatSize(f.Length)));
        if (ratio.Length > 0) Info.Add(new InfoLine("Aspect Ratio", ratio));
        if (li.Gif != null) Info.Add(new InfoLine("Frames", li.Gif.Frames.Count.ToString()));
        TakenLines.Clear();
        if (li.Meta.Taken != null) TakenLines.Add(new InfoLine("Calendar", li.Meta.Taken));
        if (li.Meta.Camera != null) TakenLines.Add(new InfoLine("Camera", li.Meta.Camera));
        if (li.Meta.Lens != null) TakenLines.Add(new InfoLine("Lens", li.Meta.Lens));
        OnPropertyChanged(nameof(HasTaken));
        CameraSummary = FeatherView.Core.Imaging.CameraSummary.Build(li.Meta.Camera, li.Meta.FocalMm, li.Meta.FNumber, li.Meta.ExposureSeconds, li.Meta.Iso);
        if (li.Meta.Latitude is double la && li.Meta.Longitude is double lo)
        { LocationText = FeatherView.Core.Imaging.GpsMath.Format(la, lo); _locationUrl = FeatherView.Core.Imaging.GpsMath.MapUrl(la, lo); }
        else { LocationText = ""; _locationUrl = null; }
        OcrText = ""; OcrStatus = "";
        ExifLines.Clear();
        ExifLines.Add(new InfoLine("Format", li.Format));
        ExifLines.Add(new InfoLine("Modified", f.LastWriteUtc.ToLocalTime().ToString("g")));
        foreach (var l in li.Exif) ExifLines.Add(l);
        OnPropertyChanged(nameof(HasExif));
        _ = RefreshHistogramAsync();
        OnPropertyChanged(nameof(IsFavorite));
    }

    private void UpdateStatus(int w = 0, int h = 0)
    {
        OnPropertyChanged(nameof(SelectedCountText)); OnPropertyChanged(nameof(IsFavorite));
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
        try { CurrentImage = ImageLoader.Transform(src, _rotation, _flip); }   // view only: the original file is never modified
        catch (Exception ex) { _log.Error("Rotate/flip failed", ex); _dialogs.Error("Rotate", "This picture could not be rotated or flipped."); }
    }

    // ---- file operations ----
    private void DeleteChecked()
    {
        var files = Gallery.Items.Where(i => i.IsChecked).Select(i => i.File).ToList();
        if (files.Count == 0) return;
        if (Settings.ConfirmDelete && !_dialogs.Confirm("Delete files", $"Move {files.Count} selected file{(files.Count == 1 ? "" : "s")} to the Recycle Bin?\n\nYou can turn off this question in Settings.", "Delete")) return;
        int firstIdx = files.Min(f => _files.IndexOf(f));
        StopGif(); _base = null; CurrentImage = null; Video.Stop();
        int failed = 0;
        foreach (var f in files)
        {
            try { FileOperations.Recycle(f.Path); _source.Remove(f); }
            catch (Exception ex) when (ex is not OperationCanceledException) { failed++; _log.Warn($"Delete failed ({ex.GetType().Name})."); }
        }
        RebuildFiles(null, Math.Max(0, firstIdx), out _);
        if (_files.Count == 0) { _index = -1; Mode = ViewMode.Gallery; StatusText = "No more photos or videos here"; }
        else if (Mode == ViewMode.Gallery) UpdateStatus();
        else { _index = Math.Min(Math.Max(0, firstIdx), _files.Count - 1); _ = ShowIndexAsync(_index); }
        OnPropertyChanged(nameof(SelectedCountText)); OnPropertyChanged(nameof(HasChecked));
        if (failed > 0) _dialogs.Error("Delete", $"{failed} file(s) could not be moved to the Recycle Bin.");
    }

    private void DeleteCurrent()
    {
        if (HasChecked) { DeleteChecked(); return; }
        var f = Mode == ViewMode.Gallery ? Gallery.Selected?.File : Current;
        if (f == null) return;
        if (Settings.ConfirmDelete && !_dialogs.Confirm("Delete file", $"Move \"{f.Name}\" to the Recycle Bin?\n\nYou can turn off this question in Settings.", "Delete")) return;
        try
        {
            int idx = _files.IndexOf(f);
            if (idx == _index) { StopGif(); _base = null; CurrentImage = null; Video.Stop(); }
            FileOperations.Recycle(f.Path);
            _source.Remove(f);
            RebuildFiles(null, Math.Min(idx, Math.Max(0, _files.Count - 2)), out _);
            if (_files.Count == 0) { _index = -1; Mode = ViewMode.Gallery; StatusText = "No more photos or videos here"; return; }
            if (Mode == ViewMode.Gallery) UpdateStatus(); else _ = ShowIndexAsync(_index);
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
        if (Settings.SlideshowShuffle)
        {
            var pick = _shuffle.Next(_files.Count, _index, i => _files[i].Kind != MediaKind.Video);
            if (pick is int p) _ = ShowIndexAsync(p); else StopSlideshow();
            return;
        }
        int next = (_index + 1) % _files.Count;
        for (int guard = 0; guard < _files.Count && _files[next].Kind == MediaKind.Video; guard++) next = (next + 1) % _files.Count;   // slideshows show pictures only
        if (_files[next].Kind == MediaKind.Video) { StopSlideshow(); return; }
        _ = ShowIndexAsync(next);
    }

    // ---- settings ----
    private void OpenSettings()
    {
        var edited = _dialogs.ShowSettings(Settings.Clone(), () => Updates?.CheckAsync(true) ?? Task.CompletedTask, Video.EngineStatusText, () => Video.InstallEngineCommand.CanExecute(null) ? RunAsync(Video.InstallEngineCommand) : Task.CompletedTask);
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
        Video.ApplySettings();
        if (!s.KeepVideoPlayingAcrossTabs && Video.HasMedia && _videoOwner != Tabs.ActiveId) Video.Stop();   // background play was just switched off
        OnPropertyChanged(nameof(ShowSidebar)); OnPropertyChanged(nameof(FilmstripVisible)); OnPropertyChanged(nameof(IsShuffle));
    }

    private static Task RunAsync(AsyncRelayCommand c) { c.Execute(null); return Task.CompletedTask; }

    public void SetSort(SortMode mode, bool? descending = null)
    {
        Settings.Sort = mode; if (descending is bool d) Settings.SortDescending = d;
        SaveSettings(); Reload();
    }

    public void SetGroup(GroupMode mode)
    {
        Settings.GroupBy = mode; SaveSettings();
        var keep = Current?.Path ?? Gallery.Selected?.File.Path;
        RebuildFiles(keep, 0, out _);
        if (Mode == ViewMode.Gallery) UpdateStatus();
        StatusText = mode == GroupMode.None ? "Grouping off" : $"Grouped by {mode}";
    }

    public void SaveSettings() => _settingsService.Save(Settings);

    public void Shutdown()
    {
        if (SessionPath != null)
        {
            var store = new SessionStore(SessionPath);
            if (Settings.RestoreSession) store.Save(Tabs.ToSession()); else store.Delete();
        }
        Video.Dispose(); SaveSettings();
    }
    public void RefreshRecent() { RecentFiles.Clear(); foreach (var p in Settings.RecentFiles.Where(File.Exists)) RecentFiles.Add(p); }
}
