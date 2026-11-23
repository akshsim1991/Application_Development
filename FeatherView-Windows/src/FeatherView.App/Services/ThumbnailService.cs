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
        await gate.WaitAsync(ct).ConfigureAwait(false);
        try
        {
            ct.ThrowIfCancellationRequested();
            var bmp = await Task.Run(() => f.Kind == MediaKind.Video ? ShellThumbnail.Get(f.Path, size) : Decode(f.Path, size), ct).ConfigureAwait(false);
            if (bmp != null) cache.Set(key, bmp);
            return bmp;
        }
        catch (OperationCanceledException) { throw; }
        catch (Exception ex) { _log.Warn($"Thumbnail failed ({ex.GetType().Name})."); return null; }
        finally { gate.Release(); }
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
