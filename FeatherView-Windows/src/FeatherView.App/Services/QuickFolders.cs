using System.IO;
using System.Runtime.InteropServices;

namespace FeatherView.Services;

public sealed record QuickFolder(string Name, string Hint, string Path, string IconKey);

/// <summary>The folders people look for first: Pictures, Videos, Downloads, Desktop, Documents from the Windows user profile (follows folders that were moved), and the drives.</summary>
public static class QuickFolders
{
    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern int SHGetKnownFolderPath([MarshalAs(UnmanagedType.LPStruct)] Guid id, uint flags, IntPtr token, [MarshalAs(UnmanagedType.LPWStr)] out string path);

    private static readonly (string Name, Guid Id, string Icon)[] Known =
    {
        ("Pictures", new Guid("33E28130-4E1E-4676-835A-98395C3BC3BB"), "IconImage"),
        ("Videos", new Guid("18989B1D-99B5-455B-841C-AB7C74E4DDFC"), "IconVideo"),
        ("Camera Roll", new Guid("AB5FB87B-7CE2-4F83-915D-550846C9537B"), "IconCamera"),
        ("Screenshots", new Guid("B7BEDE81-DF94-4682-A7D8-57A09BF7D3A9"), "IconCamera"),
        ("Downloads", new Guid("374DE290-123F-4565-9164-39C4925E467B"), "IconFolder"),
        ("Desktop", new Guid("B4BFCC3A-DB2C-424C-B029-7FE99A87C641"), "IconMonitor"),
        ("Documents", new Guid("FDD39AD0-238F-46AF-ADB4-6C85480369C7"), "IconFile"),
    };

    public static List<QuickFolder> Build()
    {
        var list = new List<QuickFolder>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        string profile = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        string who = string.IsNullOrEmpty(profile) ? "profile" : System.IO.Path.GetFileName(profile.TrimEnd('\\', '/'));
        foreach (var (name, id, icon) in Known)
        {
            string? path = null;
            try { if (SHGetKnownFolderPath(id, 0, IntPtr.Zero, out var p) == 0) path = p; } catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException or COMException) { }
            if (string.IsNullOrEmpty(path)) path = Fallback(name, profile);
            if (!string.IsNullOrEmpty(path) && Directory.Exists(path) && seen.Add(path)) list.Add(new QuickFolder(name, $"({who})", path, icon));
        }
        try
        {
            foreach (var d in DriveInfo.GetDrives())
            {
                if (!d.IsReady || d.DriveType is not (DriveType.Fixed or DriveType.Removable or DriveType.Network)) continue;
                string label = string.IsNullOrWhiteSpace(d.VolumeLabel) ? (d.DriveType == DriveType.Removable ? "Removable Disk" : "Local Disk") : d.VolumeLabel;
                list.Add(new QuickFolder(label, $"({d.Name.TrimEnd('\\', '/')})", d.RootDirectory.FullName, "IconMonitor"));
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        return list;
    }

    private static string? Fallback(string name, string profile) => name switch
    {
        "Pictures" => Environment.GetFolderPath(Environment.SpecialFolder.MyPictures),
        "Videos" => Environment.GetFolderPath(Environment.SpecialFolder.MyVideos),
        "Desktop" => Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
        "Documents" => Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
        "Downloads" => string.IsNullOrEmpty(profile) ? null : System.IO.Path.Combine(profile, "Downloads"),
        _ => null,
    };
}
