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
    private const int WM_NCHITTEST = 0x0084, WM_LBUTTONDOWN = 0x0201, WM_LBUTTONDBLCLK = 0x0203, WM_MOUSEWHEEL = 0x020A, WM_SIZE = 0x0005, WM_MOUSEMOVE = 0x0200, WM_SETCURSOR = 0x0020, HTCLIENT = 1;

    private delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr CreateWindowEx(int exStyle, string cls, string name, int style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr inst, IntPtr param);
    [DllImport("user32.dll")] private static extern bool DestroyWindow(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool EnumChildWindows(IntPtr parent, EnumProc proc, IntPtr lParam);
    [DllImport("user32.dll")] private static extern bool EnableWindow(IntPtr hwnd, bool enable);
    [DllImport("user32.dll")] private static extern bool MoveWindow(IntPtr hwnd, int x, int y, int w, int h, bool repaint);
    [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr hwnd, out RECT r);
    [DllImport("user32.dll")] private static extern uint GetDoubleClickTime();
    [DllImport("user32.dll")] private static extern IntPtr SetCursor(IntPtr cursor);
    [DllImport("user32.dll")] private static extern IntPtr GetAncestor(IntPtr hwnd, uint flags);
    [DllImport("user32.dll")] private static extern IntPtr SetFocus(IntPtr hwnd);
    [StructLayout(LayoutKind.Sequential)] private struct RECT { public int Left, Top, Right, Bottom; }

    private IntPtr _hwnd;
    private int _lastClick, _lastX = -1, _lastY = -1;
    public new IntPtr Handle => _hwnd;

    public event Action<IntPtr>? HandleReady;
    public event Action? Clicked, DoubleClicked;
    public event Action<int>? Wheel;
    public event Action? Moved;
    /// <summary>True while full-screen controls are hidden: the mouse pointer disappears too, until the mouse moves.</summary>
    public bool CursorHidden { get; set; }

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
                var root = GetAncestor(hwnd, 2 /* GA_ROOT */); if (root != IntPtr.Zero) SetFocus(root);   // keep keyboard shortcuts working after a click on the video
                if (unchecked(now - _lastClick) <= (int)GetDoubleClickTime()) { _lastClick = 0; DoubleClicked?.Invoke(); }
                else { _lastClick = now; Clicked?.Invoke(); }
                handled = true; return IntPtr.Zero;
            }
            case WM_LBUTTONDBLCLK:          // windows whose class has CS_DBLCLKS deliver the second click of a double click as this message
                _lastClick = 0; DoubleClicked?.Invoke(); handled = true; return IntPtr.Zero;
            case WM_MOUSEWHEEL: Wheel?.Invoke((short)((wParam.ToInt64() >> 16) & 0xFFFF)); handled = true; return IntPtr.Zero;
            case WM_SIZE: PrepareChildren(); break;
            case WM_MOUSEMOVE:
            {
                // Windows also sends a move when the layout changes under a still mouse; only real movement counts.
                long p = lParam.ToInt64(); int x = (short)(p & 0xFFFF), y = (short)((p >> 16) & 0xFFFF);
                if (x != _lastX || y != _lastY) { _lastX = x; _lastY = y; Moved?.Invoke(); }
                break;
            }
            case WM_SETCURSOR when CursorHidden: SetCursor(IntPtr.Zero); handled = true; return (IntPtr)1;
        }
        return base.WndProc(hwnd, msg, wParam, lParam, ref handled);
    }
}
