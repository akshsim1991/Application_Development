using System.Text.Json.Serialization;

namespace FeatherView.Core.Settings;

[JsonConverter(typeof(JsonStringEnumConverter<AppTheme>))]
public enum AppTheme { System, Light, Dark }

[JsonConverter(typeof(JsonStringEnumConverter<SortMode>))]
public enum SortMode { Name, Date, Size, Type }

/// <summary>User settings persisted as JSON. Plain data only; validated by <see cref="Normalize"/>.</summary>
public sealed class AppSettings
{
    public int SchemaVersion { get; set; } = 1;

    // Appearance
    public AppTheme Theme { get; set; } = AppTheme.System;

    // General
    public bool RememberWindow { get; set; } = true;
    public double WindowLeft { get; set; } = double.NaN;
    public double WindowTop { get; set; } = double.NaN;
    public double WindowWidth { get; set; } = 1100;
    public double WindowHeight { get; set; } = 720;
    public bool WindowMaximized { get; set; }
    public int MaxRecentFiles { get; set; } = 12;
    public List<string> RecentFiles { get; set; } = new();

    // Browsing
    public SortMode Sort { get; set; } = SortMode.Name;
    public bool SortDescending { get; set; }
    public int ThumbnailSize { get; set; } = 160;

    // Photo viewing
    public bool FitOnOpen { get; set; } = true;
    public bool ShowInfoPanelOnOpen { get; set; }

    // Slideshow
    public int SlideshowSeconds { get; set; } = 4;
    public bool SlideshowLoop { get; set; } = true;
    public bool SlideshowFullscreen { get; set; } = true;

    // Performance
    public int ThumbnailCacheItems { get; set; } = 600;
    public int ThumbnailParallelism { get; set; } = 2;

    // Updates (same model as FeatherPDF / FileGlade: daily check, ask first, verify before install)
    public bool AutoUpdateCheck { get; set; } = true;
    public DateTime? LastUpdateCheckUtc { get; set; }
    public string? SkippedVersion { get; set; }

    public const int MinThumb = 96, MaxThumb = 320;

    /// <summary>Clamps every value into its supported range so a hand-edited file cannot break the app.</summary>
    public void Normalize()
    {
        ThumbnailSize = Math.Clamp(ThumbnailSize, MinThumb, MaxThumb);
        SlideshowSeconds = Math.Clamp(SlideshowSeconds, 1, 120);
        ThumbnailCacheItems = Math.Clamp(ThumbnailCacheItems, 50, 5000);
        ThumbnailParallelism = Math.Clamp(ThumbnailParallelism, 1, 8);
        MaxRecentFiles = Math.Clamp(MaxRecentFiles, 0, 50);
        if (!Enum.IsDefined(Theme)) Theme = AppTheme.System;
        if (!Enum.IsDefined(Sort)) Sort = SortMode.Name;
        if (!(WindowWidth >= 400)) WindowWidth = 1100;
        if (!(WindowHeight >= 300)) WindowHeight = 720;
        RecentFiles ??= new();
        if (RecentFiles.Count > MaxRecentFiles) RecentFiles.RemoveRange(MaxRecentFiles, RecentFiles.Count - MaxRecentFiles);
    }

    public void AddRecent(string path)
    {
        RecentFiles.RemoveAll(p => string.Equals(p, path, StringComparison.OrdinalIgnoreCase));
        RecentFiles.Insert(0, path);
        Normalize();
    }

    public AppSettings Clone() => SettingsJson.Deserialize(SettingsJson.Serialize(this)) ?? new AppSettings();
}

[JsonSourceGenerationOptions(WriteIndented = true, PropertyNameCaseInsensitive = true,
    DefaultIgnoreCondition = JsonIgnoreCondition.Never, NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals)]
[JsonSerializable(typeof(AppSettings))]
internal sealed partial class SettingsJsonContext : JsonSerializerContext { }

public static class SettingsJson
{
    public static string Serialize(AppSettings s) => System.Text.Json.JsonSerializer.Serialize(s, SettingsJsonContext.Default.AppSettings);
    public static AppSettings? Deserialize(string json) => System.Text.Json.JsonSerializer.Deserialize(json, SettingsJsonContext.Default.AppSettings);
}
