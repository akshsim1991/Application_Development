namespace FeatherView.Core.Video;

/// <summary>What happens when a video ends: stop after the last one, repeat the whole list, or repeat the current video.</summary>
public enum RepeatMode { Off, All, One }

/// <summary>
/// An ordered play queue of video files with shuffle and repeat. Plain data and logic (no UI), so it can be unit tested.
/// Files are identified by path; duplicates are not added twice.
/// </summary>
public sealed class Playlist
{
    private readonly List<string> _items = new();
    private readonly HashSet<int> _played = new();       // shuffle: indexes already played in this round
    private readonly Stack<int> _history = new();        // shuffle: for "previous"
    private readonly Random _rng;
    private int _index = -1;

    public Playlist(int? seed = null) { _rng = seed is int s ? new Random(s) : new Random(); }

    public IReadOnlyList<string> Items => _items;
    public int Count => _items.Count;
    public int Index => _index;
    public string? Current => _index >= 0 && _index < _items.Count ? _items[_index] : null;
    public bool Shuffle { get; set; }
    public RepeatMode Repeat { get; set; } = RepeatMode.Off;

    /// <summary>Replaces the list and makes <paramref name="start"/> the current item.</summary>
    public void SetItems(IEnumerable<string> paths, int start = 0)
    {
        _items.Clear(); _played.Clear(); _history.Clear();
        foreach (var p in paths) if (!_items.Contains(p, StringComparer.OrdinalIgnoreCase)) _items.Add(p);
        _index = _items.Count == 0 ? -1 : Math.Clamp(start, 0, _items.Count - 1);
        if (_index >= 0) _played.Add(_index);
    }

    /// <summary>Adds files that are not in the list yet. Returns how many were added.</summary>
    public int Add(IEnumerable<string> paths)
    {
        int added = 0;
        foreach (var p in paths)
            if (!_items.Contains(p, StringComparer.OrdinalIgnoreCase)) { _items.Add(p); added++; }
        if (_index < 0 && _items.Count > 0) { _index = 0; _played.Add(0); }
        return added;
    }

    public void Remove(int index)
    {
        if (index < 0 || index >= _items.Count) return;
        _items.RemoveAt(index);
        _played.Clear(); _history.Clear();
        if (_items.Count == 0) _index = -1;
        else if (index < _index) _index--;
        else if (index == _index) _index = Math.Min(_index, _items.Count - 1);
        if (_index >= 0) _played.Add(_index);
    }

    public void Clear() { _items.Clear(); _played.Clear(); _history.Clear(); _index = -1; }

    public bool JumpTo(int index)
    {
        if (index < 0 || index >= _items.Count) return false;
        if (Shuffle && _index >= 0) _history.Push(_index);
        _index = index; _played.Add(index);
        return true;
    }

    /// <summary>Makes <paramref name="path"/> the current item if it is in the list (used when a listed video is opened by hand).</summary>
    public void SyncTo(string path)
    {
        int i = _items.FindIndex(p => p.Equals(path, StringComparison.OrdinalIgnoreCase));
        if (i >= 0 && i != _index) { _index = i; _played.Add(i); }
    }

    /// <summary>The video to play next. <paramref name="auto"/> = the current one just ended (Repeat One then replays it); false = the user pressed Next.</summary>
    public string? Next(bool auto)
    {
        if (_items.Count == 0) return null;
        if (auto && Repeat == RepeatMode.One) return Current;
        int? next = Shuffle ? NextShuffled() : NextSequential();
        if (next is not int n) return null;
        if (Shuffle && _index >= 0) _history.Push(_index);
        _index = n; _played.Add(n);
        return _items[n];
    }

    public string? Previous()
    {
        if (_items.Count == 0) return null;
        if (Shuffle && _history.Count > 0) { _index = _history.Pop(); return _items[_index]; }
        int p = _index - 1;
        if (p < 0) { if (Repeat == RepeatMode.Off) return null; p = _items.Count - 1; }
        _index = p; _played.Add(p);
        return _items[p];
    }

    private int? NextSequential()
    {
        int n = _index + 1;
        if (n < _items.Count) return n;
        return Repeat == RepeatMode.Off ? null : 0;        // after the last: stop (play once) or start over (repeat all / repeat one pressed manually)
    }

    private int? NextShuffled()
    {
        var left = Enumerable.Range(0, _items.Count).Where(i => i != _index && !_played.Contains(i)).ToList();
        if (left.Count == 0)
        {
            if (Repeat == RepeatMode.Off) return null;      // everything played once
            _played.Clear(); _played.Add(_index);           // new round
            left = Enumerable.Range(0, _items.Count).Where(i => i != _index).ToList();
            if (left.Count == 0) return _index;             // a single video repeats itself
        }
        return left[_rng.Next(left.Count)];
    }
}
