using System.Text.Json.Serialization;
using FeatherView.Core.Infrastructure;

namespace FeatherView.Core.Tabs;

/// <summary>Everything needed to bring a tab back: small and cheap, no images or file lists are kept for inactive tabs.</summary>
public sealed class TabState
{
    public Guid Id { get; set; } = Guid.NewGuid();
    public string? CustomTitle { get; set; }
    public string Section { get; set; } = "Gallery";      // Gallery | Favorites | Recent | Search
    public string? FolderPath { get; set; }
    public string? SelectedPath { get; set; }
    public string Filter { get; set; } = "All";
    public string Search { get; set; } = "";
    public string Mode { get; set; } = "Gallery";         // Gallery | Photo | Video

    public TabState Clone() => new()
    {
        Id = Guid.NewGuid(), CustomTitle = CustomTitle, Section = Section, FolderPath = FolderPath,
        SelectedPath = SelectedPath, Filter = Filter, Search = Search, Mode = Mode,
    };
}

public sealed class TabSessionData
{
    public List<TabState> Tabs { get; set; } = new();
    public int Active { get; set; }
}

[JsonSourceGenerationOptions(WriteIndented = true, PropertyNameCaseInsensitive = true)]
[JsonSerializable(typeof(TabSessionData))]
internal sealed partial class TabJsonContext : JsonSerializerContext { }

/// <summary>The list of tabs, which one is active, and a short "reopen closed tab" history. Pure logic, no UI.</summary>
public sealed class TabManager
{
    public const int MaxTabs = 20, MaxClosed = 10;

    private readonly List<TabState> _tabs = new();
    private readonly Stack<(TabState State, int Index)> _closed = new();

    public TabManager() { _tabs.Add(new TabState()); }

    public IReadOnlyList<TabState> Tabs => _tabs;
    public int ActiveIndex { get; private set; }
    public TabState Active => _tabs[ActiveIndex];
    public bool CanReopen => _closed.Count > 0;
    public bool CanAdd => _tabs.Count < MaxTabs;

    public static string TitleOf(TabState t)
    {
        if (!string.IsNullOrWhiteSpace(t.CustomTitle)) return t.CustomTitle!.Trim();
        if (t.Section == "Favorites") return "Favorites";
        if (t.Section == "Recent") return "Recent";
        if (!string.IsNullOrEmpty(t.FolderPath))
        {
            var trimmed = t.FolderPath!.TrimEnd('\\', '/');
            var name = trimmed.Length == 0 ? t.FolderPath : trimmed.Substring(trimmed.LastIndexOfAny(new[] { '\\', '/' }) + 1);
            return string.IsNullOrEmpty(name) ? t.FolderPath! : name;       // drive roots keep their name ("C:")
        }
        return "New tab";
    }

    /// <summary>Adds a tab after the active one and makes it active. Returns null when the limit is reached.</summary>
    public TabState? Add(TabState? template = null)
    {
        if (!CanAdd) return null;
        var t = template ?? new TabState();
        _tabs.Insert(ActiveIndex + 1, t);
        ActiveIndex++;
        return t;
    }

    public TabState? Duplicate(int index)
    {
        if (index < 0 || index >= _tabs.Count || !CanAdd) return null;
        var copy = _tabs[index].Clone();
        _tabs.Insert(index + 1, copy);
        ActiveIndex = index + 1;
        return copy;
    }

    /// <summary>Closes a tab. The last remaining tab is replaced by an empty one, so there is always at least one.</summary>
    public void Close(int index)
    {
        if (index < 0 || index >= _tabs.Count) return;
        var closing = _tabs[index];
        if (closing.FolderPath != null || closing.Section is "Favorites" or "Recent")
        {
            _closed.Push((closing, index));
            while (_closed.Count > MaxClosed) { var keep = _closed.ToArray().Take(MaxClosed).Reverse().ToArray(); _closed.Clear(); foreach (var k in keep) _closed.Push(k); }
        }
        _tabs.RemoveAt(index);
        if (_tabs.Count == 0) { _tabs.Add(new TabState()); ActiveIndex = 0; return; }
        if (index < ActiveIndex) ActiveIndex--;
        else if (index == ActiveIndex) ActiveIndex = Math.Min(index, _tabs.Count - 1);   // the tab that slid into this place (or the new last one)
    }

    public void CloseOthers(int keepIndex)
    {
        if (keepIndex < 0 || keepIndex >= _tabs.Count) return;
        var keep = _tabs[keepIndex];
        for (int i = _tabs.Count - 1; i >= 0; i--) if (i != _tabs.IndexOf(keep)) Close(i);
        ActiveIndex = _tabs.IndexOf(keep);
    }

    /// <summary>Brings back the most recently closed tab at its old position and activates it.</summary>
    public TabState? Reopen()
    {
        if (_closed.Count == 0 || !CanAdd) return null;
        var (state, index) = _closed.Pop();
        index = Math.Clamp(index, 0, _tabs.Count);
        // a lone empty tab (what remains after closing everything) is replaced rather than kept next to the restored one
        if (_tabs.Count == 1 && _tabs[0].FolderPath == null && _tabs[0].Section == "Gallery") { _tabs[0] = state; ActiveIndex = 0; return state; }
        _tabs.Insert(index, state);
        ActiveIndex = index;
        return state;
    }

    public void Rename(int index, string? title)
    {
        if (index < 0 || index >= _tabs.Count) return;
        _tabs[index].CustomTitle = string.IsNullOrWhiteSpace(title) ? null : title.Trim();
    }

    public void Activate(int index) { if (index >= 0 && index < _tabs.Count) ActiveIndex = index; }
    public void Next() => ActiveIndex = (ActiveIndex + 1) % _tabs.Count;
    public void Previous() => ActiveIndex = (ActiveIndex - 1 + _tabs.Count) % _tabs.Count;

    public TabSessionData ToData() => new() { Tabs = _tabs.ToList(), Active = ActiveIndex };

    public static TabManager FromData(TabSessionData? data)
    {
        var m = new TabManager();
        if (data == null || data.Tabs.Count == 0) return m;
        m._tabs.Clear();
        foreach (var t in data.Tabs.Take(MaxTabs)) m._tabs.Add(t ?? new TabState());
        m.ActiveIndex = Math.Clamp(data.Active, 0, m._tabs.Count - 1);
        return m;
    }
}

/// <summary>Reads and writes the tab session as JSON. Never throws; a missing or damaged file just means "no session".</summary>
public sealed class SessionStore
{
    private readonly string _path;
    private readonly ILog _log;
    public SessionStore(string path, ILog? log = null) { _path = path; _log = log ?? NullLog.Instance; }

    public TabSessionData? Load()
    {
        try { return File.Exists(_path) ? System.Text.Json.JsonSerializer.Deserialize(File.ReadAllText(_path), TabJsonContext.Default.TabSessionData) : null; }
        catch (Exception ex) { _log.Warn($"Tab session unreadable ({ex.GetType().Name})."); return null; }
    }

    public void Save(TabSessionData data)
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
            var tmp = _path + ".tmp";
            File.WriteAllText(tmp, System.Text.Json.JsonSerializer.Serialize(data, TabJsonContext.Default.TabSessionData));
            if (File.Exists(_path)) File.Replace(tmp, _path, null); else File.Move(tmp, _path);
        }
        catch (Exception ex) { _log.Warn($"Tab session not saved ({ex.GetType().Name})."); }
    }

    public void Delete() { try { if (File.Exists(_path)) File.Delete(_path); } catch { } }
}
