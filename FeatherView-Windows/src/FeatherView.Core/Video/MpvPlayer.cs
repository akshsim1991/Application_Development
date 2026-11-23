using System.Globalization;
using System.Reflection;
using System.Runtime.InteropServices;

namespace FeatherView.Core.Video;

public sealed class MpvException : Exception { public MpvException(string m) : base(m) { } }

public sealed record MpvTrack(int Id, string Type, string? Lang, string? Title, string? Codec, bool Selected, bool External, bool Default);
public sealed record MpvChapter(string Title, double Time);
public enum MpvEventKind { None, FileLoaded, EndOfFile, Error, Shutdown, PlaybackRestart, Other }
public readonly record struct MpvEvent(MpvEventKind Kind, string? Detail = null);

/// <summary>
/// Thin, UI-free wrapper over libmpv (loaded at run time, never at start-up, so the app works without it).
/// All calls are thread-safe in libmpv; this wrapper is intended for use from one thread (the UI thread).
/// </summary>
public sealed class MpvPlayer : IDisposable
{
    private const string Lib = "mpv";
    private static string? s_path;
    private static bool s_resolverSet;

    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern IntPtr mpv_create();
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern int mpv_initialize(IntPtr h);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern void mpv_terminate_destroy(IntPtr h);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern int mpv_set_option_string(IntPtr h, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern int mpv_set_property_string(IntPtr h, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern IntPtr mpv_get_property_string(IntPtr h, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern void mpv_free(IntPtr data);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern int mpv_command(IntPtr h, IntPtr[] args);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern IntPtr mpv_wait_event(IntPtr h, double timeout);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern IntPtr mpv_error_string(int error);
    [DllImport(Lib, CallingConvention = CallingConvention.Cdecl)] private static extern uint mpv_client_api_version();

    [StructLayout(LayoutKind.Sequential)]
    private struct RawEvent { public int Id; public int Error; public ulong ReplyUserdata; public IntPtr Data; }

    private IntPtr _h;

    /// <summary>Points the loader at a specific libmpv file (or null to use the platform default name). Must precede first use.</summary>
    public static void Configure(string? libraryPath)
    {
        s_path = libraryPath;
        if (!s_resolverSet)
        {
            NativeLibrary.SetDllImportResolver(typeof(MpvPlayer).Assembly, Resolve);
            s_resolverSet = true;
        }
    }

    private static IntPtr Resolve(string name, Assembly asm, DllImportSearchPath? path)
    {
        if (name != Lib) return IntPtr.Zero;
        if (s_path != null && NativeLibrary.TryLoad(s_path, out var h)) return h;
        foreach (var n in new[] { "libmpv.so.2", "libmpv.so", "libmpv-2.dll", "mpv-2.dll", "libmpv.dll", "libmpv.2.dylib" })
            if (NativeLibrary.TryLoad(n, asm, path, out h)) return h;
        return IntPtr.Zero;
    }

    /// <summary>True if the library loads and answers a version query. Never throws.</summary>
    public static bool IsAvailable(out string? error)
    {
        try { error = null; return mpv_client_api_version() != 0; }
        catch (Exception ex) when (ex is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException) { error = ex.Message; return false; }
    }

    public MpvPlayer(IReadOnlyDictionary<string, string>? options = null)
    {
        _h = mpv_create();
        if (_h == IntPtr.Zero) throw new MpvException("libmpv could not create a player.");
        try
        {
            if (options != null) foreach (var kv in options) Check(mpv_set_option_string(_h, kv.Key, kv.Value), $"option {kv.Key}", ignoreUnknown: true);
            Check(mpv_initialize(_h), "initialize");
        }
        catch { mpv_terminate_destroy(_h); _h = IntPtr.Zero; throw; }
    }

    private static string ErrorText(int code) => Marshal.PtrToStringUTF8(mpv_error_string(code)) ?? $"error {code}";

    private void Check(int code, string what, bool ignoreUnknown = false)
    {
        if (code >= 0) return;
        if (ignoreUnknown && code == -5) return;      // MPV_ERROR_OPTION_NOT_FOUND: keep working on older libmpv builds
        throw new MpvException($"mpv {what}: {ErrorText(code)}");
    }

    private void EnsureOpen() { if (_h == IntPtr.Zero) throw new ObjectDisposedException(nameof(MpvPlayer)); }

    public void Command(params string[] args)
    {
        EnsureOpen();
        var ptrs = new IntPtr[args.Length + 1];
        try
        {
            for (int i = 0; i < args.Length; i++) ptrs[i] = Marshal.StringToCoTaskMemUTF8(args[i]);
            Check(mpv_command(_h, ptrs), args[0]);
        }
        finally { for (int i = 0; i < args.Length; i++) if (ptrs[i] != IntPtr.Zero) Marshal.FreeCoTaskMem(ptrs[i]); }
    }

    public bool TryCommand(params string[] args) { try { Command(args); return true; } catch (MpvException) { return false; } }

    public void LoadFile(string path) => Command("loadfile", path, "replace");
    public void Stop() => TryCommand("stop");

    public string? GetString(string name)
    {
        if (_h == IntPtr.Zero) return null;
        var p = mpv_get_property_string(_h, name);
        if (p == IntPtr.Zero) return null;
        try { return Marshal.PtrToStringUTF8(p); } finally { mpv_free(p); }
    }

    public double? GetDouble(string name) =>
        double.TryParse(GetString(name), NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : null;

    public int? GetInt(string name) => int.TryParse(GetString(name), NumberStyles.Integer, CultureInfo.InvariantCulture, out var v) ? v : null;
    public bool GetBool(string name) => GetString(name) == "yes";

    public bool SetString(string name, string value)
    {
        if (_h == IntPtr.Zero) return false;
        return mpv_set_property_string(_h, name, value) >= 0;
    }
    public bool SetDouble(string name, double v) => SetString(name, v.ToString("0.######", CultureInfo.InvariantCulture));
    public bool SetBool(string name, bool v) => SetString(name, v ? "yes" : "no");

    public IReadOnlyList<MpvTrack> GetTracks()
    {
        var list = new List<MpvTrack>();
        int n = GetInt("track-list/count") ?? 0;
        for (int i = 0; i < n; i++)
        {
            var p = $"track-list/{i}/";
            list.Add(new MpvTrack(GetInt(p + "id") ?? 0, GetString(p + "type") ?? "", GetString(p + "lang"), GetString(p + "title"),
                GetString(p + "codec"), GetBool(p + "selected"), GetBool(p + "external"), GetBool(p + "default")));
        }
        return list;
    }

    public IReadOnlyList<MpvChapter> GetChapters()
    {
        var list = new List<MpvChapter>();
        int n = GetInt("chapter-list/count") ?? 0;
        for (int i = 0; i < n; i++)
            list.Add(new MpvChapter(GetString($"chapter-list/{i}/title") ?? $"Chapter {i + 1}", GetDouble($"chapter-list/{i}/time") ?? 0));
        return list;
    }

    /// <summary>Returns queued events without blocking. Call regularly from a timer.</summary>
    public IReadOnlyList<MpvEvent> DrainEvents()
    {
        var list = new List<MpvEvent>();
        if (_h == IntPtr.Zero) return list;
        for (int guard = 0; guard < 64; guard++)
        {
            var p = mpv_wait_event(_h, 0);
            if (p == IntPtr.Zero) break;
            var e = Marshal.PtrToStructure<RawEvent>(p);
            if (e.Id == 0) break;                                   // MPV_EVENT_NONE: queue empty
            switch (e.Id)
            {
                case 1: list.Add(new MpvEvent(MpvEventKind.Shutdown)); break;
                case 7:                                             // END_FILE: data -> { int reason; int error }
                {
                    int reason = e.Data != IntPtr.Zero ? Marshal.ReadInt32(e.Data) : 0;
                    int err = e.Data != IntPtr.Zero ? Marshal.ReadInt32(e.Data, 4) : 0;
                    list.Add(reason == 4 ? new MpvEvent(MpvEventKind.Error, ErrorText(err)) : reason == 0 ? new MpvEvent(MpvEventKind.EndOfFile) : new MpvEvent(MpvEventKind.Other));
                    break;
                }
                case 8: list.Add(new MpvEvent(MpvEventKind.FileLoaded)); break;
                case 21: list.Add(new MpvEvent(MpvEventKind.PlaybackRestart)); break;
            }
        }
        return list;
    }

    public void Dispose()
    {
        if (_h == IntPtr.Zero) return;
        var h = _h; _h = IntPtr.Zero;
        mpv_terminate_destroy(h);
    }
}
