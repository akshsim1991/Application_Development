namespace FeatherView.Core.Media;

public enum MediaKind { Unknown, Image, Video }

public static class MediaTypes
{
    // Decoded by Windows Imaging Component. WebP/HEIC/AVIF decode only when the matching Microsoft codec
    // extension is installed; the viewer reports a clear message when WIC cannot open a file.
    public static readonly IReadOnlySet<string> ImageExtensions = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
    { ".jpg", ".jpeg", ".jpe", ".jfif", ".png", ".bmp", ".dib", ".gif", ".tif", ".tiff", ".ico", ".webp", ".heic", ".heif", ".avif", ".jxr", ".wdp" };

    public static readonly IReadOnlySet<string> VideoExtensions = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
    { ".mp4", ".m4v", ".mov", ".avi", ".mkv", ".wmv", ".webm", ".mpg", ".mpeg", ".3gp", ".ts", ".m2ts" };

    public static MediaKind Classify(string path)
    {
        var ext = Path.GetExtension(path);
        if (ImageExtensions.Contains(ext)) return MediaKind.Image;
        if (VideoExtensions.Contains(ext)) return MediaKind.Video;
        return MediaKind.Unknown;
    }
}
