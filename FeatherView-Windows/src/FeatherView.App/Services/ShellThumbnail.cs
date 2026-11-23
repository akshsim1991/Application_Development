using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media.Imaging;

namespace FeatherView.Services;

/// <summary>Asks Windows for the same preview picture Explorer shows for a video (works for the formats Windows has a thumbnail handler for). Read-only.</summary>
public static class ShellThumbnail
{
    [ComImport, Guid("bcc18b79-ba16-442f-80c4-8a59c30c463b"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IShellItemImageFactory
    {
        [PreserveSig] int GetImage([In, MarshalAs(UnmanagedType.Struct)] SIZE size, [In] int flags, out IntPtr phbm);
    }

    [StructLayout(LayoutKind.Sequential)] private struct SIZE { public int cx, cy; }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode, PreserveSig = false)]
    private static extern void SHCreateItemFromParsingName([MarshalAs(UnmanagedType.LPWStr)] string path, IntPtr pbc, [In] ref Guid riid, [MarshalAs(UnmanagedType.Interface)] out IShellItemImageFactory factory);

    [DllImport("gdi32.dll")] private static extern bool DeleteObject(IntPtr h);

    private const int ResizeToFit = 0x0, BiggerSizeOk = 0x1, ThumbnailOnly = 0x8, InCacheOnly = 0x10;

    /// <param name="cacheOnly">true: only return a picture Windows already has cached (Explorer's thumbnail cache) - returns at once; false: let Windows generate it (can take a second or more per video).</param>
    public static BitmapSource? Get(string path, int size, bool cacheOnly = false)
    {
        IntPtr hbm = IntPtr.Zero;
        try
        {
            var iid = typeof(IShellItemImageFactory).GUID;
            SHCreateItemFromParsingName(path, IntPtr.Zero, ref iid, out var factory);
            try
            {
                // THUMBNAILONLY: no generic file icon when Windows cannot make a preview (we then show our own VIDEO tile)
                if (factory.GetImage(new SIZE { cx = size, cy = size }, ResizeToFit | ThumbnailOnly | BiggerSizeOk | (cacheOnly ? InCacheOnly : 0), out hbm) != 0 || hbm == IntPtr.Zero) return null;
                var src = Imaging.CreateBitmapSourceFromHBitmap(hbm, IntPtr.Zero, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
                src.Freeze();
                return src;
            }
            finally { Marshal.ReleaseComObject(factory); }
        }
        catch (Exception ex) when (ex is COMException or InvalidCastException or ArgumentException or System.IO.IOException or UnauthorizedAccessException) { return null; }
        finally { if (hbm != IntPtr.Zero) DeleteObject(hbm); }
    }
}
