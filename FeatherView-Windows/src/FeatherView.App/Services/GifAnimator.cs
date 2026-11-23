using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace FeatherView.Services;

/// <summary>Plays an animated GIF by compositing frames (WPF returns raw partial frames) at the file's own timing.</summary>
public sealed class GifAnimator : IDisposable
{
    private readonly GifBitmapDecoder _decoder;
    private readonly DispatcherTimer _timer = new();
    private readonly int _w, _h;
    private int _index = -1;
    private BitmapSource? _canvas, _beforePrev;
    private int _prevDisposal;
    private Int32Rect _prevRect;

    public event Action<BitmapSource>? FrameReady;
    public bool IsPaused { get; private set; }

    public GifAnimator(GifBitmapDecoder decoder)
    {
        _decoder = decoder;
        int w = 0, h = 0;
        try { if (decoder.Metadata is BitmapMetadata m) { w = Convert.ToInt32(m.GetQuery("/logscrdesc/Width") ?? 0); h = Convert.ToInt32(m.GetQuery("/logscrdesc/Height") ?? 0); } }
        catch { }
        _w = w > 0 ? w : decoder.Frames[0].PixelWidth;
        _h = h > 0 ? h : decoder.Frames[0].PixelHeight;
        _timer.Tick += (_, _) => Advance();
    }

    public void Start() { IsPaused = false; Advance(); }
    public void Pause() { IsPaused = true; _timer.Stop(); }
    public void Resume() { if (IsPaused) { IsPaused = false; Advance(); } }
    public void Dispose() { _timer.Stop(); FrameReady = null; }

    private void Advance()
    {
        _timer.Stop();
        _index = (_index + 1) % _decoder.Frames.Count;
        var frame = _decoder.Frames[_index];
        var (delay, disposal, rect) = ReadInfo(frame);
        var full = new Rect(0, 0, _w, _h);

        var dv = new DrawingVisual();
        using (var dc = dv.RenderOpen())
        {
            if (_index > 0 && _canvas != null)
            {
                BitmapSource? baseImg = _prevDisposal == 3 ? _beforePrev : _canvas;
                if (baseImg != null)
                {
                    if (_prevDisposal == 2)
                        dc.PushClip(new CombinedGeometry(GeometryCombineMode.Exclude, new RectangleGeometry(full),
                                                         new RectangleGeometry(new Rect(_prevRect.X, _prevRect.Y, _prevRect.Width, _prevRect.Height))));
                    dc.DrawImage(baseImg, full);
                    if (_prevDisposal == 2) dc.Pop();
                }
            }
            dc.DrawImage(frame, new Rect(rect.X, rect.Y, rect.Width, rect.Height));
        }
        var rtb = new RenderTargetBitmap(_w, _h, 96, 96, PixelFormats.Pbgra32);
        rtb.Render(dv); rtb.Freeze();

        _beforePrev = _index == 0 ? null : _canvas;   // state before this frame, needed if the next frame restores "previous"
        _canvas = rtb; _prevDisposal = disposal; _prevRect = rect;
        FrameReady?.Invoke(rtb);
        if (!IsPaused) { _timer.Interval = TimeSpan.FromMilliseconds(delay); _timer.Start(); }
    }

    private (int delayMs, int disposal, Int32Rect rect) ReadInfo(BitmapFrame f)
    {
        int delay = 100, disposal = 0, left = 0, top = 0;
        try
        {
            if (f.Metadata is BitmapMetadata m)
            {
                if (m.GetQuery("/grctlext/Delay") is ushort d) delay = d <= 1 ? 100 : d * 10;
                if (m.GetQuery("/grctlext/Disposal") is byte dp) disposal = dp;
                if (m.GetQuery("/imgdesc/Left") is ushort l) left = l;
                if (m.GetQuery("/imgdesc/Top") is ushort t) top = t;
            }
        }
        catch { }
        return (delay, disposal, new Int32Rect(left, top, Math.Min(f.PixelWidth, _w - left), Math.Min(f.PixelHeight, _h - top)));
    }
}
