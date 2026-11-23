using System.Runtime.InteropServices;
using System.Windows.Interop;

namespace FeatherView.Video;

/// <summary>
/// A plain child window that libmpv renders into (`wid`). libmpv's own child window is disabled so that mouse input falls through
/// to this host (and from there to the app), and keyboard focus stays with the WPF window.
/// </summary>
public sealed class MpvHost : HwndHost
{
    private const int WS_CHILD = 0x40000000, WS_VISIBLE = 0x10000000, WS_CLIPCHILDREN = 0x02000000, WS_CLIPSIBLINGS = 0x04000000;
    private const int WM_NCHITTEST = 0x0084, WM_LBUTTONDOWN = 0x0201, WM_MOUSEWHEEL = 0x020A, WM_SIZE = 0x0005, HTCLIENT = 1;

    private delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateWindowEx(int exStyle, string cls, string name, int style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr inst, IntPtr param);
    [DllImport("user32.dll")] private static extern bool DestroyWindow(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll")] private static extern bool EnableWindow(IntPtr hwnd, bool enable);
    [DllImport("user32.dll")] private static extern bool MoveWindow(IntPtr hwnd, int x, int y, int w, int h, bool repaint);
    [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr hwnd, out RECT r);
    [DllImport("user32.dll")] private static extern uint GetDoubleClickTime();
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }

    private IntPtr _hwnd;
    private int _lastClick;
    public IntPtr Handle => _hwnd;

    public event Action<IntPtr>? HandleReady;
    public event Action? Clicked, DoubleClicked;
    public event Action<int>? Wheel;

    protected override HandleRef BuildWindowCore(HandleRef hwndParent)
    {
        _hwnd = CreateWindowEx(0, "static", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 16, 16, hwndParent.Handle, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
        HandleReady?.Invoke(_hwnd);
        return new HandleRef(this, _hwnd);
    }

    protected override void DestroyWindowCore(HandleRef hwnd) { DestroyWindow(hwnd.Handle); _hwnd = IntPtr.Zero; }

    /// <summary>Call after libmpv has created its window: disables it (so input reaches us) and fits it to the host.</summary>
    public void PrepareChildren()
    {
        if (_hwnd == IntPtr.Zero) return;
        GetClientRect(_hwnd, out var r);
        EnumChildWindows(_hwnd, (child, _) => { EnableWindow(child, false); MoveWindow(child, 0, 0, r.Right, r.Bottom, true); return true; }, IntPtr.Zero);
    }

    protected override IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        switch (msg)
        {
            case WM_NCHITTEST: handled = true; return (IntPtr)HTCLIENT;        // static controls are otherwise click-through
            case WM_LBUTTONDOWN:
            {
                int now = Environment.TickCount;
                if (unchecked(now - _lastClick) <= (int)GetDoubleClickTime()) { _lastClick = 0; DoubleClicked?.Invoke(); }
                else { _lastClick = now; Clicked?.Invoke(); }
                handled = true; return IntPtr.Zero;
            }
            case WM_MOUSEWHEEL: Wheel?.Invoke((short)((wParam.ToInt64() >> 16) & 0xFFFF)); handled = true; return IntPtr.Zero;
            case WM_SIZE: PrepareChildren(); break;
        }
        return base.WndProc(hwnd, msg, wParam, lParam, ref handled);
    }
}
