using System.Collections.ObjectModel;
using FeatherView.Core.Mvvm;

namespace FeatherView.ViewModels;

public sealed class GalleryRow
{
    public GalleryRow(IReadOnlyList<MediaItemViewModel> items) { Items = items; }
    public IReadOnlyList<MediaItemViewModel> Items { get; }
}

/// <summary>Arranges items in rows for a virtualized, wrapping thumbnail grid and tracks the selected tile.</summary>
public sealed class GalleryViewModel : ObservableObject
{
    private List<MediaItemViewModel> _items = new();
    private int _columns = 4, _selected = -1, _thumbSize = 160;
    private double _width = 900;

    public ObservableCollection<GalleryRow> Rows { get; } = new();
    public IReadOnlyList<MediaItemViewModel> Items => _items;   // flat list for the filmstrip
    public event Action<int>? ScrollToRow;

    public int ThumbSize { get => _thumbSize; private set { if (Set(ref _thumbSize, value)) { OnPropertyChanged(nameof(TileWidth)); OnPropertyChanged(nameof(TileHeight)); } } }
    public double TileWidth => _thumbSize + 16;
    public double TileHeight => _thumbSize + 40;
    public int Count => _items.Count;
    public int SelectedIndex => _selected;
    public MediaItemViewModel? Selected => _selected >= 0 && _selected < _items.Count ? _items[_selected] : null;

    public void SetItems(IEnumerable<MediaItemViewModel> items, int selected)
    {
        _items = items.ToList();
        _selected = -1;
        OnPropertyChanged(nameof(Items)); OnPropertyChanged(nameof(Count));
        Rebuild();
        Select(selected, scroll: true);
    }

    public void SetThumbSize(int size) { ThumbSize = size; Rebuild(); }

    public void SetViewportWidth(double width)
    {
        _width = width;
        int cols = Math.Max(1, (int)((width - 28) / TileWidth));
        if (cols != _columns) { _columns = cols; Rebuild(); }
    }

    public void Select(int index, bool scroll = false)
    {
        if (_items.Count == 0) { _selected = -1; return; }
        index = Math.Clamp(index, 0, _items.Count - 1);
        if (_selected >= 0 && _selected < _items.Count) _items[_selected].IsSelected = false;
        _selected = index; _items[index].IsSelected = true;
        OnPropertyChanged(nameof(SelectedIndex)); OnPropertyChanged(nameof(Selected));
        if (scroll) ScrollToRow?.Invoke(index / _columns);
    }

    public void Move(int delta) => Select(_selected + delta, scroll: true);
    public void MoveRows(int rows) => Select(_selected + rows * _columns, scroll: true);

    private void Rebuild()
    {
        Rows.Clear();
        for (int i = 0; i < _items.Count; i += _columns)
            Rows.Add(new GalleryRow(_items.Skip(i).Take(_columns).ToList()));
    }
}
