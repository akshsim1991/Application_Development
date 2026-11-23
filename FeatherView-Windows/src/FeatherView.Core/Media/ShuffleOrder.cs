namespace FeatherView.Core.Media;

/// <summary>Random order without repeats: every picture is shown once before any is shown again.</summary>
public sealed class ShuffleOrder
{
    private readonly Random _rng;
    private readonly HashSet<int> _seen = new();
    private int _count;

    public ShuffleOrder(int? seed = null) { _rng = seed is int s ? new Random(s) : new Random(); }

    public void Reset() => _seen.Clear();

    /// <summary>Picks the next index (never <paramref name="current"/>), or null when nothing else is allowed.</summary>
    public int? Next(int count, int current, Func<int, bool>? allowed = null)
    {
        if (count <= 0) return null;
        if (count != _count) { _seen.Clear(); _count = count; }
        _seen.Add(current);
        var candidates = Enumerable.Range(0, count).Where(i => i != current && !_seen.Contains(i) && (allowed?.Invoke(i) ?? true)).ToList();
        if (candidates.Count == 0)
        {
            _seen.Clear(); _seen.Add(current);          // everything was shown: start a new round
            candidates = Enumerable.Range(0, count).Where(i => i != current && (allowed?.Invoke(i) ?? true)).ToList();
            if (candidates.Count == 0) return null;
        }
        var pick = candidates[_rng.Next(candidates.Count)];
        _seen.Add(pick);
        return pick;
    }
}
