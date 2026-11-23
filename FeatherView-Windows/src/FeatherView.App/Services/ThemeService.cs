using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using FeatherView.Core.Settings;
using Microsoft.Win32;

namespace FeatherView.Services;

/// <summary>Swaps the Dark/Light resource dictionary at runtime and follows the Windows setting in System mode.</summary>
public sealed class ThemeService
{
    private ThemeMode _mode = ThemeMode.System;
    public bool IsDark { get; private set; } = true;

    public ThemeService()
    {
        SystemEvents.UserPreferenceChanged += (_, e) =>
        {
            if (_mode == ThemeMode.System && e.Category == UserPreferenceCategory.General)
                Application.Current?.Dispatcher.BeginInvoke(() => Apply(_mode));
        };
    }

    public void Apply(ThemeMode mode)
    {
        _mode = mode;
        IsDark = mode switch { ThemeMode.Dark => true, ThemeMode.Light => false, _ => !SystemUsesLightTheme() };
        var dict = new ResourceDictionary { Source = new Uri($"Themes/{(IsDark ? "Dark" : "Light")}.xaml", UriKind.Relative) };
        var merged = Application.Current.Resources.MergedDictionaries;
        if (merged.Count > 0) merged[0] = dict; else merged.Insert(0, dict);
        foreach (Window w in Application.Current.Windows) ApplyToWindow(w);
    }

    public void ApplyToWindow(Window w)
    {
        var hwnd = new WindowInteropHelper(w).Handle;
        if (hwnd == IntPtr.Zero) { w.SourceInitialized += (_, _) => ApplyToWindow(w); return; }
        int on = IsDark ? 1 : 0;
        try { DwmSetWindowAttribute(hwnd, 20, ref on, sizeof(int)); DwmSetWindowAttribute(hwnd, 19, ref on, sizeof(int)); }
        catch { /* DWM unavailable (e.g. Safe Mode): keep the default title bar */ }
    }

    private static bool SystemUsesLightTheme()
    {
        try
        {
            using var k = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
            return k?.GetValue("AppsUseLightTheme") is int v && v == 1;
        }
        catch { return false; }
    }

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);
}
