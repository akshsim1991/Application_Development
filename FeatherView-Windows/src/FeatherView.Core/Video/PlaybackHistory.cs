using System.Text.Json.Serialization;
using FeatherView.Core.Infrastructure;

namespace FeatherView.Core.Video;

public sealed class PlaybackEntry
{
    public string Key { get; set; } = "";
    public string Name { get; set; } = "";
    public string Path { get; set; } = "";
    public double Position { get; set; }
    public double Duration { get; set; }
    public DateTime LastPlayedUtc { get; set; }
}

public sealed class PlaybackHistoryData { public List<PlaybackEntry> Entries { get; set; } = new(); }

[JsonSourceGenerationOptions(WriteIndented = true, PropertyNameCaseInsensitive = true)]
[JsonSerializable(typeof(PlaybackHistoryData))]
internal sealed partial class HistoryJsonContext : JsonSerializerContext { }

/// <summary>Resume positions and a short play history, stored locally as JSON (never uploaded).</summary>
public sealed class PlaybackHistory
{
    public const int Capacity = 300;
    public const double MinResumeSeconds = 5, FinishedMarginSeconds = 10;

    private readonly string _path;
    private readonly ILog _log;
    private readonly object _gate = new();
    private List<PlaybackEntry> _entries = new();

    public PlaybackHistory(string path, ILog? log = null) { _path = path; _log = log ?? NullLog.Instance; Load(); }

    public static string MakeKey(string path, long length, DateTime lastWriteUtc) => $"{path.ToLowerInvariant()}|{length}|{lastWriteUtc.Ticks}";

    public IReadOnlyList<PlaybackEntry> Recent { get { lock (_gate) return _entries.ToList(); } }

    public double? ResumePosition(string key)
    {
        lock (_gate)
        {
            var e = _entries.FirstOrDefault(x => x.Key == key);
            return e != null && e.Position >= MinResumeSeconds ? e.Position : null;
        }
    }

    /// <summary>Stores the position. Nearly-finished or just-started videos are not offered for resume but stay in the history.</summary>
    public void Record(string key, string name, string path, double position, double duration)
    {
        lock (_gate)
        {
            _entries.RemoveAll(x => x.Key == key);
            bool finished = duration > 0 && position >= duration - FinishedMarginSeconds;
            _entries.Insert(0, new PlaybackEntry { Key = key, Name = name, Path = path, Duration = duration, LastPlayedUtc = DateTime.UtcNow,
                Position = finished || position < MinResumeSeconds ? 0 : position });
            if (_entries.Count > Capacity) _entries.RemoveRange(Capacity, _entries.Count - Capacity);
        }
    }

    public void Clear() { lock (_gate) _entries.Clear(); Save(); }

    public void Save()
    {
        try
        {
            string json;
            lock (_gate) json = System.Text.Json.JsonSerializer.Serialize(new PlaybackHistoryData { Entries = _entries }, HistoryJsonContext.Default.PlaybackHistoryData);
            Directory.CreateDirectory(System.IO.Path.GetDirectoryName(_path)!);
            var tmp = _path + ".tmp";
            File.WriteAllText(tmp, json);
            if (File.Exists(_path)) File.Replace(tmp, _path, null); else File.Move(tmp, _path);
        }
        catch (Exception ex) { _log.Warn($"Playback history not saved ({ex.GetType().Name})."); }
    }

    private void Load()
    {
        try
        {
            if (!File.Exists(_path)) return;
            var d = System.Text.Json.JsonSerializer.Deserialize(File.ReadAllText(_path), HistoryJsonContext.Default.PlaybackHistoryData);
            if (d != null) _entries = d.Entries.Take(Capacity).ToList();
        }
        catch (Exception ex) { _log.Warn($"Playback history unreadable ({ex.GetType().Name})."); }
    }
}
