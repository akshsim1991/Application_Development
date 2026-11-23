using FeatherView.Core.Infrastructure;

namespace FeatherView.Core.Settings;

/// <summary>Loads/saves <see cref="AppSettings"/>. Never throws: a missing or corrupt file yields defaults.</summary>
public sealed class SettingsService
{
    private readonly string _path;
    private readonly ILog _log;

    public SettingsService(string path, ILog? log = null) { _path = path; _log = log ?? NullLog.Instance; }

    public static string DefaultPath(string appDataRoot) => Path.Combine(appDataRoot, "FeatherView", "settings.json");

    public AppSettings Load()
    {
        try
        {
            if (File.Exists(_path))
            {
                var s = SettingsJson.Deserialize(File.ReadAllText(_path));
                if (s != null) { Migrate(s); s.Normalize(); return s; }
            }
        }
        catch (Exception ex)
        {
            _log.Warn($"Settings could not be read, using defaults ({ex.GetType().Name}).");
            TryBackupCorrupt();
        }
        var d = new AppSettings(); d.Normalize(); return d;
    }

    /// <summary>
    /// Settings files written by older versions keep their saved values, so a new default would never reach them.
    /// Schema 2: "show pictures from all folders underneath" became the default and is switched on once for existing files.
    /// </summary>
    internal static void Migrate(AppSettings s)
    {
        if (s.SchemaVersion < 2) s.IncludeSubfolders = true;
        s.SchemaVersion = AppSettings.CurrentSchema;
    }

    public bool Save(AppSettings settings)
    {
        try
        {
            settings.Normalize();
            Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
            var tmp = _path + ".tmp";
            File.WriteAllText(tmp, SettingsJson.Serialize(settings));
            if (File.Exists(_path)) File.Replace(tmp, _path, null); else File.Move(tmp, _path);   // atomic swap
            return true;
        }
        catch (Exception ex) { _log.Warn($"Settings could not be saved ({ex.GetType().Name})."); return false; }
    }

    private void TryBackupCorrupt()
    {
        try { File.Copy(_path, _path + ".corrupt", true); } catch { /* best effort */ }
    }
}
