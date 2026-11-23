using System.Windows.Media;
using FeatherView.Core.Media;
using FeatherView.Core.Mvvm;
using FeatherView.Services;

namespace FeatherView.ViewModels;

/// <summary>
/// One tile in the gallery. The thumbnail loads lazily: the binding only reads <see cref="Thumbnail"/> for tiles that the
/// virtualized list actually realizes, so off-screen files cost nothing. Pixels live in the bounded LRU cache, not here.
/// </summary>
public sealed class MediaItemViewModel : ObservableObject
{
    private readonly ThumbnailService _thumbs;
    private bool _requested, _failed, _selected, _gaveUp, _hasThumb;

    public MediaItemViewModel(MediaFile file, ThumbnailService thumbs) { File = file; _thumbs = thumbs; }

    public MediaFile File { get; }
    public string Name => File.Name;
    /// <summary>"VIDEO" text only while there is no preview picture; with one, a small play badge is shown instead.</summary>
    public bool ShowVideoLabel => IsVideo && !_hasThumb;
    public bool ShowPlayBadge => IsVideo && _hasThumb;
    public bool IsVideo => File.Kind == MediaKind.Video;
    public bool Failed { get => _failed; private set => Set(ref _failed, value); }
    /// <summary>Ticked with the check box in the thumbnail (multi-selection for batch actions).</summary>
    public bool IsChecked { get => _checked; set { if (Set(ref _checked, value)) CheckedChanged?.Invoke(); } }
    private bool _checked;
    public static event Action? CheckedChanged;
    public bool IsSelected { get => _selected; set => Set(ref _selected, value); }

    public ImageSource? Thumbnail
    {
        get
        {
            if (_thumbs.TryGet(File, out var t)) { _hasThumb = true; return t; }
            if (!_requested && !_gaveUp && File.Kind != MediaKind.Unknown) { _requested = true; _ = LoadAsync(); }
            return null;
        }
    }

    private async Task LoadAsync()
    {
        try
        {
            var t = await _thumbs.LoadAsync(File);
            if (t == null) { _gaveUp = true; if (File.Kind == MediaKind.Image) Failed = true; }
            else _hasThumb = true;
        }
        catch (OperationCanceledException) { }
        finally { _requested = false; OnPropertyChanged(nameof(Thumbnail)); OnPropertyChanged(nameof(ShowVideoLabel)); OnPropertyChanged(nameof(ShowPlayBadge)); }
    }

    /// <summary>Lets a recycled tile request its thumbnail again if the cache evicted it.</summary>
    public void Refresh() { _requested = false; OnPropertyChanged(nameof(Thumbnail)); }
}
