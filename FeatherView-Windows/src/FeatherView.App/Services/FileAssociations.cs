using System.Runtime.InteropServices;
using FeatherView.Core.Media;
using Microsoft.Win32;

namespace FeatherView.Services;

/// <summary>
/// Makes Feather View appear in Windows' "Open with" list (and in Settings > Default apps) for pictures and videos.
/// Written for the current user only (HKCU), so no administrator rights are needed; removing it deletes exactly these keys.
/// </summary>
public static class FileAssociations
{
    private const string ImageProgId = "FeatherView.Image", VideoProgId = "FeatherView.Video", AppKey = "FeatherView.exe", CapKey = @"Software\FeatherView\Capabilities";
    private static readonly string[] ImageExts = { ".jpg", ".jpeg", ".png", ".bmp", ".gif", ".tif", ".tiff", ".webp", ".ico" };

    [DllImport("shell32.dll")] private static extern void SHChangeNotify(int eventId, uint flags, IntPtr a, IntPtr b);

    public static void Register(string exePath)
    {
        var cmd = $"\"{exePath}\" \"%1\"";
        var videoExts = MediaTypes.VideoExtensions.ToArray();
        using var classes = Registry.CurrentUser.CreateSubKey(@"Software\Classes");

        WriteProgId(classes, ImageProgId, "Picture (Feather View)", exePath, cmd);
        WriteProgId(classes, VideoProgId, "Video (Feather View)", exePath, cmd);
        foreach (var e in ImageExts) AddOpenWith(classes, e, ImageProgId);
        foreach (var e in videoExts) AddOpenWith(classes, e, VideoProgId);

        // "Applications\\FeatherView.exe" is what the classic "Open with > Choose another app" list reads
        using (var app = classes.CreateSubKey(@"Applications\" + AppKey))
        {
            app.SetValue("FriendlyAppName", "Feather View");
            using (var c = app.CreateSubKey(@"shell\open\command")) c.SetValue("", cmd);
            using var types = app.CreateSubKey("SupportedTypes");
            foreach (var e in ImageExts.Concat(videoExts)) types.SetValue(e, "");
        }

        // Capabilities + RegisteredApplications: lists the app in Settings > Default apps
        using (var cap = Registry.CurrentUser.CreateSubKey(CapKey))
        {
            cap.SetValue("ApplicationName", "Feather View");
            cap.SetValue("ApplicationDescription", "Lightweight, offline photo and video viewer");
            using var fa = cap.CreateSubKey("FileAssociations");
            foreach (var e in ImageExts) fa.SetValue(e, ImageProgId);
            foreach (var e in videoExts) fa.SetValue(e, VideoProgId);
        }
        using (var reg = Registry.CurrentUser.CreateSubKey(@"Software\RegisteredApplications")) reg.SetValue("FeatherView", CapKey);

        SHChangeNotify(0x08000000 /* SHCNE_ASSOCCHANGED */, 0, IntPtr.Zero, IntPtr.Zero);
    }

    public static void Unregister()
    {
        using (var classes = Registry.CurrentUser.OpenSubKey(@"Software\Classes", writable: true))
        {
            if (classes != null)
            {
                foreach (var e in ImageExts.Concat(MediaTypes.VideoExtensions))
                {
                    using var k = classes.OpenSubKey(e + @"\OpenWithProgids", writable: true);
                    k?.DeleteValue(ImageProgId, false); k?.DeleteValue(VideoProgId, false);
                }
                classes.DeleteSubKeyTree(ImageProgId, false); classes.DeleteSubKeyTree(VideoProgId, false);
                classes.DeleteSubKeyTree(@"Applications\" + AppKey, false);
            }
        }
        Registry.CurrentUser.DeleteSubKeyTree(@"Software\FeatherView", false);
        using (var reg = Registry.CurrentUser.OpenSubKey(@"Software\RegisteredApplications", writable: true)) reg?.DeleteValue("FeatherView", false);
        SHChangeNotify(0x08000000, 0, IntPtr.Zero, IntPtr.Zero);
    }

    private static void WriteProgId(RegistryKey classes, string progId, string title, string exe, string cmd)
    {
        using var k = classes.CreateSubKey(progId);
        k.SetValue("", title);
        using (var i = k.CreateSubKey("DefaultIcon")) i.SetValue("", exe + ",0");
        using (var c = k.CreateSubKey(@"shell\open\command")) c.SetValue("", cmd);
    }

    private static void AddOpenWith(RegistryKey classes, string ext, string progId)
    {
        using var k = classes.CreateSubKey(ext + @"\OpenWithProgids");
        k.SetValue(progId, "", RegistryValueKind.String);
    }
}
