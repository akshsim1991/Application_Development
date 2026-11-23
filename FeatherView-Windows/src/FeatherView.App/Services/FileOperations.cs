using System.IO;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using Microsoft.VisualBasic.FileIO;

namespace FeatherView.Services;

/// <summary>Destructive operations always go through the Recycle Bin so they can be undone from Explorer.</summary>
public static class FileOperations
{
    public static void Recycle(string path) =>
        FileSystem.DeleteFile(path, UIOption.OnlyErrorDialogs, RecycleOption.SendToRecycleBin, UICancelOption.ThrowException);

    public static void ShowInExplorer(string path)
    {
        if (!File.Exists(path)) return;
        System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("explorer.exe", $"/select,\"{path}\"") { UseShellExecute = true });
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool SystemParametersInfo(uint action, uint param, string value, uint flags);

    /// <summary>Saves the displayed picture as a PNG in the app's data folder (the original is never touched) and makes it the desktop wallpaper.</summary>
    public static bool SetWallpaper(BitmapSource image, string appDataFolder)
    {
        try
        {
            Directory.CreateDirectory(appDataFolder);
            var path = Path.Combine(appDataFolder, "wallpaper.png");
            var enc = new PngBitmapEncoder(); enc.Frames.Add(BitmapFrame.Create(image));
            using (var fs = File.Create(path)) enc.Save(fs);
            return SystemParametersInfo(0x0014 /*SPI_SETDESKWALLPAPER*/, 0, path, 0x01 | 0x02 /*update ini + broadcast*/);
        }
        catch { return false; }
    }

    /// <summary>Prints the picture scaled to fit the page, keeping its aspect ratio.</summary>
    public static void Print(BitmapSource image, string title)
    {
        var pd = new PrintDialog();
        if (pd.ShowDialog() != true) return;
        double pw = pd.PrintableAreaWidth, ph = pd.PrintableAreaHeight;
        double scale = Math.Min(pw / image.PixelWidth, ph / image.PixelHeight);
        double w = image.PixelWidth * scale, h = image.PixelHeight * scale;
        var dv = new DrawingVisual();
        using (var dc = dv.RenderOpen()) dc.DrawImage(image, new Rect((pw - w) / 2, (ph - h) / 2, w, h));
        pd.PrintVisual(dv, title);
    }
}
