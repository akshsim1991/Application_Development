namespace FeatherView.Core.Media;

public enum QuickFilter { All, Jpg, Png, Gif, Video, Other }

/// <summary>Sidebar quick filters, their counts and the filename search, all over an in-memory list (no disk access).</summary>
public static class MediaFilter
{
    public static QuickFilter Classify(MediaFile f)
    {
        if (f.Kind == MediaKind.Video) return QuickFilter.Video;
        var ext = Path.GetExtension(f.Name).ToLowerInvariant();
        return ext switch
        {
            ".jpg" or ".jpeg" or ".jpe" or ".jfif" => QuickFilter.Jpg,
            ".png" => QuickFilter.Png,
            ".gif" => QuickFilter.Gif,
            _ => QuickFilter.Other,
        };
    }

    public static bool Matches(MediaFile f, QuickFilter filter, string? search)
    {
        if (filter != QuickFilter.All && Classify(f) != filter) return false;
        return string.IsNullOrWhiteSpace(search) || f.Name.Contains(search.Trim(), StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Counts per filter for the whole list (ignores the current filter so every row always shows its own total).</summary>
    public static Dictionary<QuickFilter, int> Count(IEnumerable<MediaFile> files, string? search = null)
    {
        var d = Enum.GetValues<QuickFilter>().ToDictionary(k => k, _ => 0);
        foreach (var f in files)
        {
            if (!string.IsNullOrWhiteSpace(search) && !f.Name.Contains(search.Trim(), StringComparison.OrdinalIgnoreCase)) continue;
            d[QuickFilter.All]++; d[Classify(f)]++;
        }
        return d;
    }
}
