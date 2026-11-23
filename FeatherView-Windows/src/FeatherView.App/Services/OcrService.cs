using System.IO;
using System.Windows.Media.Imaging;
using Windows.Graphics.Imaging;
using Windows.Media.Ocr;
using Windows.Storage.Streams;

namespace FeatherView.Services;

public sealed record OcrOutcome(string? Text, string? Error);

/// <summary>Text recognition with Windows' own offline engine (Windows.Media.Ocr). Nothing is downloaded or uploaded; it runs only when asked.</summary>
public static class OcrService
{
    public static async Task<OcrOutcome> RecognizeAsync(BitmapSource image)
    {
        try
        {
            var engine = OcrEngine.TryCreateFromUserProfileLanguages();
            if (engine == null)
                return new OcrOutcome(null, "Windows has no text-recognition language installed. Add one under Settings > Time & language > Language & region.");

            BitmapSource source = image;
            int max = (int)OcrEngine.MaxImageDimension;
            if (image.PixelWidth > max || image.PixelHeight > max)
            {
                double scale = (double)max / Math.Max(image.PixelWidth, image.PixelHeight);
                source = new TransformedBitmap(image, new System.Windows.Media.ScaleTransform(scale, scale));
            }

            var encoder = new PngBitmapEncoder();
            encoder.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(source));
            using var stream = new InMemoryRandomAccessStream();
            using (var write = stream.AsStreamForWrite()) { encoder.Save(write); await write.FlushAsync(); }
            stream.Seek(0);

            var decoder = await Windows.Graphics.Imaging.BitmapDecoder.CreateAsync(stream);
            using var bitmap = await decoder.GetSoftwareBitmapAsync(BitmapPixelFormat.Bgra8, BitmapAlphaMode.Premultiplied);
            var result = await engine.RecognizeAsync(bitmap);
            var text = result.Text?.Trim();
            return string.IsNullOrEmpty(text) ? new OcrOutcome(null, "No text was found in this picture.") : new OcrOutcome(text, null);
        }
        catch (Exception)
        {
            return new OcrOutcome(null, "Text recognition is not available on this PC.");
        }
    }
}
