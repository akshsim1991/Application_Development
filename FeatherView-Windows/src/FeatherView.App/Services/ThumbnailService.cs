using System.IO;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using FeatherView.Core.Infrastructure;
using FeatherView.Core.Media;

namespace FeatherView.Services;

/// <summary>
/// Decodes thumbnails on background threads (bounded parallelism) and keeps them in a bounded LRU cache,
/// so memory stays flat however large the folder is.
/// </summary>
public sealed class ThumbnailService
{
    private LruCache<string, BitmapSource> _cache;
    private SemaphoreSlim _gate;
    private readonly ILog _log;
    private int _size;
    // Videos are slow to preview (Windows has to open and decode each file): they get their own small gate so pictures are never held up,
    // a quick "already cached" attempt first, and a small on-disk cache (JPEG previews only, in this app's own folder) so each video is slow only once.
    private readonly SemaphoreSlim _videoGate = new(2);
    private static readonly string DiskDir = System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "FeatherView", "thumbs");

    public ThumbnailService(int size, int cacheItems, int parallelism, ILog log)
    {
        _size = size; _log = log;
        _cache = new LruCache<string, BitmapSource>(cacheItems);
        _gate = new SemaphoreSlim(parallelism);
    }

    public int Size => _size;

    public void Reconfigure(int size, int cacheItems, int parallelism)
    {
        _size = size;
        _cache = new LruCache<string, BitmapSource>(cacheItems);
        _gate = new SemaphoreSlim(parallelism);
    }

    private string Key(MediaFile f) => $"{f.Path}|{_size}|{f.LastWriteUtc.Ticks}";

    public bool TryGet(MediaFile f, out BitmapSource? thumb)
    {
        var ok = _cache.TryGet(Key(f), out var t); thumb = t; return ok;
    }

    public async Task<BitmapSource?> LoadAsync(MediaFile f, CancellationToken ct = default)
    {
        var key = Key(f); var size = _size; var gate = _gate; var cache = _cache;
        if (cache.TryGet(key, out var hit)) return hit;
        if (f.Kind == MediaKind.Video) return await LoadVideoAsync(f, key, size, cache, ct).ConfigureAwait(false);
        await gate.WaitAsync(ct).ConfigureAwait(false);
        try
        {
            ct.ThrowIfCancellationRequested();
            var bmp = await Task.Run(() => Decode(f.Path, size), ct).ConfigureAwait(false);
            if (bmp != null) cache.Set(key, bmp);
            return bmp;
        }
        catch (OperationCanceledException) { throw; }
        catch (Exception ex) { _log.Warn($"Thumbnail failed ({ex.GetType().Name})."); return null; }
        finally { gate.Release(); }
    }

    private async Task<BitmapSource?> LoadVideoAsync(MediaFile f, string key, int size, LruCache<string, BitmapSource> cache, CancellationToken ct)
    {
        var vsize = Math.Min(size, 256);
        try
        {
            // 1) our own disk cache, 2) Windows' thumbnail cache (both quick and run in parallel), 3) let Windows generate it (limited to 2 at a time)
            var quick = await Task.Run(() => ReadDisk(f, vsize) ?? ShellThumbnail.Get(f.Path, vsize, cacheOnly: true), ct).ConfigureAwait(false);
            if (quick != null) { cache.Set(key, quick); return quick; }
            await _videoGate.WaitAsync(ct).ConfigureAwait(false);
            try
            {
                ct.ThrowIfCancellationRequested();
                var bmp = await Task.Run(() => ShellThumbnail.Get(f.Path, vsize), ct).ConfigureAwait(false);
                if (bmp != null) { cache.Set(key, bmp); WriteDisk(f, vsize, bmp); }
                return bmp;
            }
            finally { _videoGate.Release(); }
        }
        catch (OperationCanceledException) { throw; }
        catch (Exception ex) { _log.Warn($"Video thumbnail failed ({ex.GetType().Name})."); return null; }
    }

    private static string DiskPath(MediaFile f, int size)
    {
        var bytes = System.Security.Cryptography.SHA1.HashData(System.Text.Encoding.UTF8.GetBytes($"{f.Path.ToLowerInvariant()}|{f.Length}|{f.LastWriteUtc.Ticks}|{size}"));
        return System.IO.Path.Combine(DiskDir, Convert.ToHexString(bytes) + ".jpg");
    }

    private static BitmapSource? ReadDisk(MediaFile f, int size)
    {
        try
        {
            var p = DiskPath(f, size);
            if (!File.Exists(p)) return null;
            using var fs = new FileStream(p, FileMode.Open, FileAccess.Read, FileShare.Read);
            var bi = new BitmapImage(); bi.BeginInit(); bi.StreamSource = fs; bi.CacheOption = BitmapCacheOption.OnLoad; bi.EndInit(); bi.Freeze();
            return bi;
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or NotSupportedException or InvalidOperationException) { return null; }
    }

    private static void WriteDisk(MediaFile f, int size, BitmapSource bmp)
    {
        try
        {
            Directory.CreateDirectory(DiskDir);
            var enc = new JpegBitmapEncoder { QualityLevel = 85 };
            enc.Frames.Add(BitmapFrame.Create(new FormatConvertedBitmap(bmp, PixelFormats.Bgr24, null, 0)));
            var tmp = DiskPath(f, size) + ".tmp";
            using (var fs = new FileStream(tmp, FileMode.Create, FileAccess.Write)) enc.Save(fs);
            File.Move(tmp, DiskPath(f, size), true);
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or NotSupportedException or InvalidOperationException) { }
    }

    private static BitmapSource? Decode(string path, int size)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete, 65536, FileOptions.SequentialScan);
        // First pass reads only the header to learn the aspect ratio; DecodePixel* then lets WIC decode at reduced size.
        var probe = BitmapDecoder.Create(fs, BitmapCreateOptions.DelayCreation | BitmapCreateOptions.IgnoreColorProfile, BitmapCacheOption.None);
        var f0 = probe.Frames[0];
        int w = f0.PixelWidth, h = f0.PixelHeight;
        var orientation = ExifReader.ReadOrientation(f0);
        fs.Position = 0;
        var bi = new BitmapImage();
        bi.BeginInit();
        bi.StreamSource = fs;
        bi.CacheOption = BitmapCacheOption.OnLoad;
        bi.CreateOptions = BitmapCreateOptions.IgnoreColorProfile;
        if (w >= h) bi.DecodePixelWidth = Math.Min(size, w); else bi.DecodePixelHeight = Math.Min(size, h);
        bi.EndInit();
        BitmapSource result = ImageLoader.ApplyOrientation(bi, orientation);
        result.Freeze();
        return result;
    }
}
