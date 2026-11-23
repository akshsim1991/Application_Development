using System.Collections.ObjectModel;
using FeatherView.Core.Mvvm;

namespace FeatherView.ViewModels;

public sealed class GalleryRow
{
    public GalleryRow(IReadOnlyList<MediaItemViewModel> items, string? header = null) { Items = items; Header = header; }
    public IReadOnlyList<MediaItemViewModel> Items { get; }
    /// <summary>Group title shown above the first row of a group (null on other rows).</summary>
    public string? Header { get; }
    public bool HasHeader => Header != null;
}

/// <summary>Arranges items in rows for a virtualized, wrapping thumbnail grid and tracks the selected tile.</summary>
public sealed class GalleryViewModel : ObservableObject
{
    private List<MediaItemViewModel> _items = new();
    private int _columns = 4, _selected = -1, _thumbSize = 160;
    private double _width = 900;
    private List<string>? _labels;
    private int[] _rowOf = Array.Empty<int>();
    private List<int> _rowFirst = new();

    private ObservableCollection<GalleryRow> _rows = new();
    public ObservableCollection<GalleryRow> Rows { get => _rows; private set => Set(ref _rows, value); }
    public IReadOnlyList<MediaItemViewModel> Items => _items;   // flat list for the filmstrip
    public event Action<int>? ScrollToRow;

    public int ThumbSize { get => _thumbSize; private set { if (Set(ref _thumbSize, value)) { OnPropertyChanged(nameof(TileWidth)); OnPropertyChanged(nameof(TileHeight)); } } }
    public double TileWidth => _thumbSize + 16;
    public double TileHeight => _thumbSize + 40;
    public int Count => _items.Count;
    public int SelectedIndex => _selected;
    public MediaItemViewModel? Selected => _selected >= 0 && _selected < _items.Count ? _items[_selected] : null;

    public void SetItems(IEnumerable<MediaItemViewModel> items, int selected, List<string>? labels = null)
    {
        _items = items.ToList();
        _labels = labels != null && labels.Count == _items.Count ? labels : null;
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
        if (scroll) ScrollToRow?.Invoke(_rowOf.Length > index ? _rowOf[index] : index / _columns);
    }

    public void Move(int delta) => Select(_selected + delta, scroll: true);
    public void MoveRows(int rows)
    {
        if (_selected < 0 || _rowOf.Length != _items.Count) { Select(_selected + rows * _columns, scroll: true); return; }
        int r = _rowOf[_selected], col = _selected - _rowFirst[r];
        int target = Math.Clamp(r + rows, 0, _rowFirst.Count - 1);
        int last = (target + 1 < _rowFirst.Count ? _rowFirst[target + 1] : _items.Count) - 1;
        Select(Math.Min(_rowFirst[target] + col, last), scroll: true);
    }

    private void Rebuild()
    {
        var rows = new List<GalleryRow>((_items.Count + _columns - 1) / _columns + 4);
        _rowOf = new int[_items.Count]; _rowFirst = new List<int>();
        int i = 0;
        while (i < _items.Count)
        {
            // a group ends where its label changes; rows never mix groups
            int end = _items.Count; string? header = null;
            if (_labels != null)
            {
                end = i + 1;
                while (end < _items.Count && _labels[end] == _labels[i]) end++;
                header = $"{_labels[i]}  ·  {end - i}";
            }
            for (int s = i; s < end; s += _columns)
            {
                int n = Math.Min(_columns, end - s);
                for (int k = 0; k < n; k++) _rowOf[s + k] = rows.Count;
                _rowFirst.Add(s);
                rows.Add(new GalleryRow(_items.GetRange(s, n), s == i ? header : null));   // O(n) overall, important for tens of thousands of pictures
            }
            i = end;
        }
        Rows = new ObservableCollection<GalleryRow>(rows);
    }
}
