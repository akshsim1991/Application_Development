using System.Text;

namespace FeatherView.Core.Infrastructure;

public interface ILog
{
    void Info(string message);
    void Warn(string message);
    void Error(string message, Exception? ex = null);
}

public sealed class NullLog : ILog
{
    public static readonly NullLog Instance = new();
    public void Info(string message) { }
    public void Warn(string message) { }
    public void Error(string message, Exception? ex = null) { }
}

/// <summary>
/// Small rolling file log. Privacy: nothing is ever sent anywhere, and user-profile paths are masked
/// so a log can be shared without revealing the Windows account name.
/// </summary>
public sealed class FileLog : ILog
{
    private readonly string _path;
    private readonly long _maxBytes;
    private readonly object _gate = new();
    private readonly string _profile;

    public FileLog(string path, long maxBytes = 256 * 1024, string? userProfile = null)
    {
        _path = path; _maxBytes = maxBytes;
        _profile = userProfile ?? Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    }

    public void Info(string m) => Write("INFO", m);
    public void Warn(string m) => Write("WARN", m);
    public void Error(string m, Exception? ex = null) => Write("ERROR", ex == null ? m : $"{m} | {ex.GetType().Name}: {ex.Message}\n{ex.StackTrace}");

    public string Scrub(string text) =>
        string.IsNullOrEmpty(_profile) ? text : text.Replace(_profile, "%USERPROFILE%", StringComparison.OrdinalIgnoreCase);

    private void Write(string level, string message)
    {
        try
        {
            lock (_gate)
            {
                Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
                var fi = new FileInfo(_path);
                if (fi.Exists && fi.Length > _maxBytes) { File.Copy(_path, _path + ".1", true); File.WriteAllText(_path, ""); }
                File.AppendAllText(_path, $"{DateTime.Now:yyyy-MM-dd HH:mm:ss} {level,-5} {Scrub(message)}{Environment.NewLine}", Encoding.UTF8);
            }
        }
        catch { /* logging must never crash the app */ }
    }
}
