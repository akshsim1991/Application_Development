using System.IO;
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
}
