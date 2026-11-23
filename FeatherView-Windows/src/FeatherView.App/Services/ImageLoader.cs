using System.IO;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace FeatherView.Services;

public sealed record LoadedImage(BitmapSource Image, int PixelWidth, int PixelHeight, string Format,
                                 IReadOnlyList<InfoLine> Exif, GifBitmapDecoder? Gif);

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
        var gif = decoder is GifBitmapDecoder g && g.Frames.Count > 1 ? g : null;
        return new LoadedImage(src, frame.PixelWidth, frame.PixelHeight, format, exif, gif);
    }

    public static BitmapSource ApplyOrientation(BitmapSource s, ushort o)
    {
        if (o == 1) return s;
        var t = new TransformGroup();
        switch (o)
        {
            case 2: t.Children.Add(new ScaleTransform(-1, 1)); break;
            case 3: t.Children.Add(new RotateTransform(180)); break;
            case 4: t.Children.Add(new ScaleTransform(1, -1)); break;
            case 5: t.Children.Add(new RotateTransform(90)); t.Children.Add(new ScaleTransform(-1, 1)); break;
            case 6: t.Children.Add(new RotateTransform(90)); break;
            case 7: t.Children.Add(new RotateTransform(270)); t.Children.Add(new ScaleTransform(-1, 1)); break;
            case 8: t.Children.Add(new RotateTransform(270)); break;
        }
        var r = new TransformedBitmap(s, t); r.Freeze(); return r;
    }

    /// <summary>User rotation (quarter turns clockwise) and horizontal flip applied on top of the decoded image.</summary>
    public static BitmapSource Transform(BitmapSource s, int quarterTurns, bool flipH)
    {
        quarterTurns = ((quarterTurns % 4) + 4) % 4;
        if (quarterTurns == 0 && !flipH) return s;
        var t = new TransformGroup();
        if (flipH) t.Children.Add(new ScaleTransform(-1, 1));
        if (quarterTurns != 0) t.Children.Add(new RotateTransform(quarterTurns * 90));
        var r = new TransformedBitmap(s, t); r.Freeze(); return r;
    }
}
