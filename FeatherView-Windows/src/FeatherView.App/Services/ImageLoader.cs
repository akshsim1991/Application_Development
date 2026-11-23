using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace FeatherView.Services;

public sealed record LoadedImage(BitmapSource Image, int PixelWidth, int PixelHeight, string Format,
                                 IReadOnlyList<InfoLine> Exif, GifData? Gif, double DpiX, double DpiY, int BitsPerPixel, ImageMeta Meta);

/// <summary>Decodes images with the Windows Imaging Component. The file is released as soon as decoding ends.</summary>
public static class ImageLoader
{
    public static LoadedImage Load(string path)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete, 81920, FileOptions.SequentialScan);
        var decoder = BitmapDecoder.Create(fs, BitmapCreateOptions.PreservePixelFormat, BitmapCacheOption.OnLoad);
        if (decoder.Frames.Count == 0) throw new InvalidDataException("No image data.");
        var frame = decoder.Frames[0];
        var exif = ExifReader.Read(frame);
        var src = ApplyOrientation(frame, ExifReader.ReadOrientation(frame));
        src.Freeze();
        var format = decoder.CodecInfo?.FriendlyName ?? Path.GetExtension(path).TrimStart('.').ToUpperInvariant();
        GifData? gif = null;
        if (decoder is GifBitmapDecoder g && g.Frames.Count > 1)
        {
            try { gif = GifData.From(g); } catch { gif = null; }   // still show the first frame if the animation cannot be prepared
        }
        return new LoadedImage(src, frame.PixelWidth, frame.PixelHeight, format, exif, gif, frame.DpiX, frame.DpiY, frame.Format.BitsPerPixel, ExifReader.ReadMeta(frame));
    }

    // WIC transforms are applied one at a time (a single rotation or flip each); each transform is frozen so the result can be frozen too.
    private static BitmapSource Step(BitmapSource s, System.Windows.Media.Transform t)
    {
        t.Freeze();
        var r = new TransformedBitmap(s, t); r.Freeze(); return r;
    }

    public static BitmapSource ApplyOrientation(BitmapSource s, ushort o)
    {
        switch (o)
        {
            case 2: return Step(s, new ScaleTransform(-1, 1));
            case 3: return Step(s, new RotateTransform(180));
            case 4: return Step(s, new ScaleTransform(1, -1));
            case 5: return Step(Step(s, new RotateTransform(90)), new ScaleTransform(-1, 1));
            case 6: return Step(s, new RotateTransform(90));
            case 7: return Step(Step(s, new RotateTransform(270)), new ScaleTransform(-1, 1));
            case 8: return Step(s, new RotateTransform(270));
            default: return s;
        }
    }

    /// <summary>User rotation (quarter turns clockwise) and horizontal flip applied on top of the decoded image.</summary>
    public static BitmapSource Transform(BitmapSource s, int quarterTurns, bool flipH)
    {
        quarterTurns = ((quarterTurns % 4) + 4) % 4;
        var r = s;
        if (flipH) r = Step(r, new ScaleTransform(-1, 1));
        if (quarterTurns != 0) r = Step(r, new RotateTransform(quarterTurns * 90));
        return r;
    }
}
