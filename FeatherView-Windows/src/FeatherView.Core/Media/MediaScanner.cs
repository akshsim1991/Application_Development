using FeatherView.Core.Settings;

namespace FeatherView.Core.Media;

public sealed record MediaFile(string Path, string Name, long Length, DateTime LastWriteUtc, MediaKind Kind);

public sealed record ScanResult(List<MediaFile> Files, bool Truncated);

/// <summary>Finds viewable media in a folder. Read-only; never touches the files.</summary>
public static class MediaScanner
{
    public const int DefaultMaxFiles = 50_000;

    public static List<MediaFile> Scan(string folder, SortMode sort = SortMode.Name, bool descending = false,
                                       bool includeVideo = true, bool images = true) =>
        ScanDetailed(folder, sort, descending, includeVideo, images, recurse: false).Files;

    /// <summary>
    /// Scans one folder, or (recurse) the whole tree underneath. A tree scan stops at <paramref name="maxFiles"/> media files or after
    /// <paramref name="maxTime"/>, so pointing it at a whole drive cannot freeze the window; <see cref="ScanResult.Truncated"/> says so.
    /// </summary>
    public static ScanResult ScanDetailed(string folder, SortMode sort = SortMode.Name, bool descending = false, bool includeVideo = true, bool images = true,
                                          bool recurse = false, int maxFiles = DefaultMaxFiles, TimeSpan? maxTime = null)
    {
        var list = new List<MediaFile>();
        bool truncated = false;
        var deadline = DateTime.UtcNow + (maxTime ?? TimeSpan.FromSeconds(4));
        try
        {
            var opts = new EnumerationOptions { IgnoreInaccessible = true, RecurseSubdirectories = recurse,
                                                AttributesToSkip = FileAttributes.Hidden | FileAttributes.System };
            int seen = 0;
            foreach (var fi in new DirectoryInfo(folder).EnumerateFiles("*", opts))
            {
                if (recurse && (++seen & 0xFF) == 0 && DateTime.UtcNow > deadline) { truncated = true; break; }
                var kind = MediaTypes.Classify(fi.Name);
                if (kind == MediaKind.Unknown) continue;
                if (kind == MediaKind.Video && !includeVideo) continue;
                if (kind == MediaKind.Image && !images) continue;
                list.Add(new MediaFile(fi.FullName, fi.Name, fi.Length, fi.LastWriteTimeUtc, kind));
                if (list.Count >= maxFiles) { truncated = true; break; }
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* unreadable folder: empty list */ }
        Sort(list, sort, descending);
        return new ScanResult(list, truncated);
    }

    /// <summary>Sub-folders (not hidden or system), in natural name order, for navigating into a drive or folder tree.</summary>
    public static List<string> ListFolders(string folder, int max = 500)
    {
        var names = new List<string>();
        try
        {
            var opts = new EnumerationOptions { IgnoreInaccessible = true, AttributesToSkip = FileAttributes.Hidden | FileAttributes.System };
            foreach (var d in new DirectoryInfo(folder).EnumerateDirectories("*", opts)) { names.Add(d.Name); if (names.Count >= max) break; }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        names.Sort(NaturalComparer.Instance);
        return names;
    }

    public static void Sort(List<MediaFile> list, SortMode sort, bool descending)
    {
        Comparison<MediaFile> cmp = sort switch
        {
            SortMode.Date => (a, b) => a.LastWriteUtc.CompareTo(b.LastWriteUtc),
            SortMode.Size => (a, b) => a.Length.CompareTo(b.Length),
            SortMode.Type => (a, b) => { int c = string.Compare(System.IO.Path.GetExtension(a.Name), System.IO.Path.GetExtension(b.Name), StringComparison.OrdinalIgnoreCase);
                                         return c != 0 ? c : NaturalComparer.Instance.Compare(a.Name, b.Name); },
            _ => (a, b) => NaturalComparer.Instance.Compare(a.Name, b.Name),
        };
        // Stable sort with a name tie-breaker so equal keys keep a predictable order.
        var sorted = list.Select((f, i) => (f, i)).ToList();
        sorted.Sort((p, q) => { int c = cmp(p.f, q.f); if (c == 0) c = NaturalComparer.Instance.Compare(p.f.Name, q.f.Name); if (c == 0) c = p.i.CompareTo(q.i); return descending ? -c : c; });
        for (int k = 0; k < list.Count; k++) list[k] = sorted[k].f;
    }
}
