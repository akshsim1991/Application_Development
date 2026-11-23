using FeatherView.Core.Settings;

namespace FeatherView.Core.Media;

public sealed record MediaFile(string Path, string Name, long Length, DateTime LastWriteUtc, MediaKind Kind);

/// <summary>Finds viewable media in a folder. Read-only; never touches the files.</summary>
public static class MediaScanner
{
    public static List<MediaFile> Scan(string folder, SortMode sort = SortMode.Name, bool descending = false,
                                       bool includeVideo = true, bool images = true)
    {
        var list = new List<MediaFile>();
        try
        {
            var opts = new EnumerationOptions { IgnoreInaccessible = true, RecurseSubdirectories = false,
                                                AttributesToSkip = FileAttributes.Hidden | FileAttributes.System };
            foreach (var fi in new DirectoryInfo(folder).EnumerateFiles("*", opts))
            {
                var kind = MediaTypes.Classify(fi.Name);
                if (kind == MediaKind.Unknown) continue;
                if (kind == MediaKind.Video && !includeVideo) continue;
                if (kind == MediaKind.Image && !images) continue;
                list.Add(new MediaFile(fi.FullName, fi.Name, fi.Length, fi.LastWriteTimeUtc, kind));
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* unreadable folder: empty list */ }
        Sort(list, sort, descending);
        return list;
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
