using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;

namespace FeatherView.Controls;

/// <summary>Surface that shows one image with wheel zoom (at the cursor), drag pan, fit and actual-size modes.</summary>
public sealed class ZoomPanImage : FrameworkElement, FeatherView.ViewModels.IViewport
{
    public static readonly DependencyProperty SourceProperty = DependencyProperty.Register(nameof(Source), typeof(ImageSource), typeof(ZoomPanImage),
        new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender, (d, _) => ((ZoomPanImage)d).OnSourceChanged()));
    public static readonly DependencyProperty ImageIdProperty = DependencyProperty.Register(nameof(ImageId), typeof(object), typeof(ZoomPanImage),
        new PropertyMetadata(null, (d, _) => ((ZoomPanImage)d).OnNewImage()));
    public static readonly DependencyProperty FitOnOpenProperty = DependencyProperty.Register(nameof(FitOnOpen), typeof(bool), typeof(ZoomPanImage), new PropertyMetadata(true));

    public ImageSource? Source { get => (ImageSource?)GetValue(SourceProperty); set => SetValue(SourceProperty, value); }
    /// <summary>Changes when a different file is shown (not when the same file is rotated or a GIF frame advances).</summary>
    public object? ImageId { get => GetValue(ImageIdProperty); set => SetValue(ImageIdProperty, value); }
    public bool FitOnOpen { get => (bool)GetValue(FitOnOpenProperty); set => SetValue(FitOnOpenProperty, value); }

    public event Action<double>? ZoomChanged;

    private double _zoom = 1;          // 1.0 = one image pixel per screen pixel
    private Vector _pan;               // offset of the image centre from the control centre, in DIPs
    private bool _fit = true;
    private bool _dragging;
    private Point _dragStart;
    private Vector _panStart;
    private int _lastW, _lastH;

    public ZoomPanImage() { ClipToBounds = true; Focusable = false; }

    private double PxToDip => 1.0 / VisualTreeHelper.GetDpi(this).DpiScaleX;
    private (double w, double h) PixelSize => Source is System.Windows.Media.Imaging.BitmapSource b ? (b.PixelWidth, b.PixelHeight) : (0, 0);

    private void OnSourceChanged()
    {
        var (w, h) = PixelSize;
        if ((int)w != _lastW || (int)h != _lastH) { _lastW = (int)w; _lastH = (int)h; if (_fit) FitInternal(); }
        InvalidateVisual();
    }

    private void OnNewImage()
    {
        if (FitOnOpen) FitInternal(); else { _fit = false; SetZoom(1, null); _pan = default; }
        BeginAnimation(OpacityProperty, new DoubleAnimation(0.25, 1, TimeSpan.FromMilliseconds(160)) { EasingFunction = new QuadraticEase() });
    }

    protected override void OnRenderSizeChanged(SizeChangedInfo info) { base.OnRenderSizeChanged(info); if (_fit) FitInternal(); else ClampPan(); }

    protected override void OnRender(DrawingContext dc)
    {
        dc.DrawRectangle(Brushes.Transparent, null, new Rect(RenderSize));   // makes the whole surface hit-testable
        if (Source is not System.Windows.Media.Imaging.BitmapSource bs || bs.PixelWidth == 0) return;
        double k = _zoom * PxToDip;
        double dw = bs.PixelWidth * k, dh = bs.PixelHeight * k;
        var r = new Rect((ActualWidth - dw) / 2 + _pan.X, (ActualHeight - dh) / 2 + _pan.Y, dw, dh);
        dc.DrawImage(bs, r);
    }

    // ---- public view commands ----
    public void ZoomIn() => ZoomBy(1.25, null);
    public void ZoomOut() => ZoomBy(0.8, null);
    public void Fit() => FitInternal();
    public void ActualSize() { _fit = false; SetZoom(1, null); _pan = default; Redraw(); }
    public bool IsFit => _fit;

    private void FitInternal()
    {
        var (w, h) = PixelSize;
        if (w <= 0 || h <= 0 || ActualWidth <= 0 || ActualHeight <= 0) { _fit = true; return; }
        double k = PxToDip;
        _fit = true;
        _pan = default;
        SetZoom(Math.Min(1.0, Math.Min(ActualWidth / (w * k), ActualHeight / (h * k))), null);
        Redraw();
    }

    private void ZoomBy(double factor, Point? anchor)
    {
        if (Source == null) return;
        _fit = false;
        SetZoom(Math.Clamp(_zoom * factor, 0.02, 32), anchor);
        ClampPan(); Redraw();
    }

    private void SetZoom(double z, Point? anchor)
    {
        double old = _zoom;
        _zoom = z;
        if (anchor is Point a && old > 0)
        {
            // keep the image point under the cursor fixed
            var c = new Point(ActualWidth / 2, ActualHeight / 2);
            var p = new Vector(a.X - c.X, a.Y - c.Y);
            _pan = p - (p - _pan) * (z / old);
        }
        RenderOptions.SetBitmapScalingMode(this, z < 1 ? BitmapScalingMode.HighQuality : z >= 2.5 ? BitmapScalingMode.NearestNeighbor : BitmapScalingMode.Linear);
        ZoomChanged?.Invoke(z);
    }

    private void ClampPan()
    {
        var (w, h) = PixelSize;
        double k = _zoom * PxToDip;
        double mx = Math.Max(0, (w * k - ActualWidth) / 2), my = Math.Max(0, (h * k - ActualHeight) / 2);
        _pan = new Vector(Math.Clamp(_pan.X, -mx, mx), Math.Clamp(_pan.Y, -my, my));
    }

    private void Redraw() => InvalidateVisual();

    // ---- mouse ----
    protected override void OnMouseWheel(MouseWheelEventArgs e)
    {
        ZoomBy(Math.Pow(1.15, e.Delta / 120.0), e.GetPosition(this));
        e.Handled = true;
    }

    protected override void OnMouseLeftButtonDown(MouseButtonEventArgs e)
    {
        if (e.ClickCount == 2) { if (_fit) ActualSize(); else FitInternal(); e.Handled = true; return; }
        _dragging = true; _dragStart = e.GetPosition(this); _panStart = _pan;
        CaptureMouse(); Cursor = Cursors.SizeAll;
    }

    protected override void OnMouseMove(MouseEventArgs e)
    {
        if (!_dragging) return;
        _pan = _panStart + (e.GetPosition(this) - _dragStart);
        ClampPan(); Redraw();
    }

    protected override void OnMouseLeftButtonUp(MouseButtonEventArgs e)
    {
        if (!_dragging) return;
        _dragging = false; ReleaseMouseCapture(); Cursor = Cursors.Arrow;
    }
}
