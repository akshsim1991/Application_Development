using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using FeatherView.Core.Settings;
using Microsoft.Win32;

namespace FeatherView.Services;

/// <summary>Swaps the Dark/Light resource dictionary at runtime and follows the Windows setting in System mode.</summary>
public sealed class ThemeService
{
    private AppTheme _mode = AppTheme.System;
    public bool IsDark { get; private set; } = true;

    public ThemeService()
    {
        SystemEvents.UserPreferenceChanged += (_, e) =>
        {
            if (_mode == AppTheme.System && e.Category == UserPreferenceCategory.General)
                Application.Current?.Dispatcher.BeginInvoke(() => Apply(_mode));
        };
    }

    public void Apply(AppTheme mode)
    {
        _mode = mode;
        IsDark = mode switch { AppTheme.Light or AppTheme.Sepia => false, AppTheme.System => !SystemUsesLightTheme(), _ => true };
        var dict = new ResourceDictionary { Source = new Uri($"Themes/{ThemeFile(mode)}.xaml", UriKind.Relative) };
        var merged = Application.Current.Resources.MergedDictionaries;
        if (merged.Count > 0) merged[0] = dict; else merged.Insert(0, dict);
        foreach (Window w in Application.Current.Windows) ApplyToWindow(w);
    }

    private string ThemeFile(AppTheme mode) => mode switch
    {
        AppTheme.Glass => "Glass", AppTheme.Amoled => "Amoled", AppTheme.Midnight => "Midnight", AppTheme.Forest => "Forest", AppTheme.Sepia => "Sepia",
        _ => IsDark ? "Dark" : "Light",
    };

    public void ApplyToWindow(Window w)
    {
        var hwnd = new WindowInteropHelper(w).Handle;
        if (hwnd == IntPtr.Zero) { w.SourceInitialized += (_, _) => ApplyToWindow(w); return; }
        int on = IsDark ? 1 : 0;
        try { DwmSetWindowAttribute(hwnd, 20, ref on, sizeof(int)); DwmSetWindowAttribute(hwnd, 19, ref on, sizeof(int)); }
        catch { /* DWM unavailable (e.g. Safe Mode): keep the default title bar */ }
        ApplyGlass(w, hwnd, _mode == AppTheme.Glass);
    }

    /// <summary>Glass theme: translucent window over a blurred (acrylic) backdrop on Windows 11; other themes switch it off again.</summary>
    private static void ApplyGlass(Window w, IntPtr hwnd, bool on)
    {
        try
        {
            if (HwndSource.FromHwnd(hwnd)?.CompositionTarget is { } target)
                target.BackgroundColor = on ? System.Windows.Media.Colors.Transparent : System.Windows.Media.Colors.Black;
            var m = new MARGINS { Left = on ? -1 : 0, Right = on ? -1 : 0, Top = on ? -1 : 0, Bottom = on ? -1 : 0 };
            DwmExtendFrameIntoClientArea(hwnd, ref m);
            int backdrop = on ? 3 /* acrylic */ : 1 /* none */;
            DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE, Windows 11 22H2+ */, ref backdrop, sizeof(int));
        }
        catch { /* older Windows or no DWM: the theme is then just translucent colours */ }
    }

    [StructLayout(LayoutKind.Sequential)] private struct MARGINS { public int Left, Right, Top, Bottom; }
    [DllImport("dwmapi.dll")] private static extern int DwmExtendFrameIntoClientArea(IntPtr hwnd, ref MARGINS m);

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
