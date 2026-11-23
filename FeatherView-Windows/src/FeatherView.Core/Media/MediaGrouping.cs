using FeatherView.Core.Settings;

namespace FeatherView.Core.Media;

/// <summary>The files re-ordered so each group is contiguous, plus the group label of every file (null when not grouped).</summary>
public sealed record GroupedFiles(List<MediaFile> Files, List<string>? Labels);

/// <summary>Arranges a list of files into groups (folder, file type, date, size). Read-only; only the order of the list changes.</summary>
public static class MediaGrouping
{
    public static GroupedFiles Apply(IReadOnlyList<MediaFile> files, GroupMode mode, bool descending)
    {
        if (mode == GroupMode.None || files.Count == 0) return new GroupedFiles(files.ToList(), null);

        var keyed = files.Select((f, i) => (f, i, k: Key(f, mode))).ToList();
        // groups ordered by their sort key (dates follow the user's ascending/descending choice); files inside a group keep their sorted order
        bool flip = descending && mode is GroupMode.Year or GroupMode.Month or GroupMode.Day;
        keyed.Sort((a, b) =>
        {
            int c = mode == GroupMode.Folder ? NaturalComparer.Instance.Compare(a.k.Order, b.k.Order) : string.CompareOrdinal(a.k.Order, b.k.Order);
            if (c != 0) return flip ? -c : c;
            return a.i.CompareTo(b.i);
        });

        var list = new List<MediaFile>(keyed.Count);
        var labels = new List<string>(keyed.Count);
        foreach (var x in keyed) { list.Add(x.f); labels.Add(x.k.Label); }
        return new GroupedFiles(list, labels);
    }

    public static (string Order, string Label) Key(MediaFile f, GroupMode mode)
    {
        switch (mode)
        {
            case GroupMode.Folder:
            {
                var dir = System.IO.Path.GetDirectoryName(f.Path) ?? "";
                var name = System.IO.Path.GetFileName(dir);
                return (dir, string.IsNullOrEmpty(name) ? (dir.Length > 0 ? dir : "(root)") : name);
            }
            case GroupMode.Type:
            {
                var ext = System.IO.Path.GetExtension(f.Name).TrimStart('.').ToUpperInvariant();
                if (ext.Length == 0) ext = "No extension";
                var kind = f.Kind == MediaKind.Video ? "Video" : f.Kind == MediaKind.Image ? "Picture" : "Other";
                return ($"{(f.Kind == MediaKind.Video ? 1 : 0)}{ext}", $"{ext}  ({kind})");
            }
            case GroupMode.Year:
            {
                var d = f.LastWriteUtc.ToLocalTime();
                return (d.ToString("yyyy"), d.ToString("yyyy"));
            }
            case GroupMode.Month:
            {
                var d = f.LastWriteUtc.ToLocalTime();
                return (d.ToString("yyyyMM"), d.ToString("MMMM yyyy", System.Globalization.CultureInfo.CurrentCulture));
            }
            case GroupMode.Day:
            {
                var d = f.LastWriteUtc.ToLocalTime();
                return (d.ToString("yyyyMMdd"), d.ToString("D", System.Globalization.CultureInfo.CurrentCulture));
            }
            case GroupMode.Size:
            {
                long b = f.Length;
                if (b < 100 * 1024) return ("0", "Tiny  (under 100 KB)");
                if (b < 1024 * 1024) return ("1", "Small  (100 KB to 1 MB)");
                if (b < 10L * 1024 * 1024) return ("2", "Medium  (1 to 10 MB)");
                if (b < 100L * 1024 * 1024) return ("3", "Large  (10 to 100 MB)");
                return ("4", "Huge  (over 100 MB)");
            }
            default: return ("", "");
        }
    }
}
