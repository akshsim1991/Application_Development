using System.Drawing;

namespace PneuSim;

public class VectorShape
{
	public PointF[] Points;

	public bool Closed;

	public Color Stroke;

	public float StrokeWidth;

	public bool Dashed;

	public Color Fill;

	public string Text;

	public PointF TextPos;

	public float FontSize;

	public bool Bold;

	public float AngleDeg;

	public bool IsText => Text != null;

	public VectorShape()
	{
		Stroke = Color.Empty;
		StrokeWidth = 1f;
		Fill = Color.Empty;
	}
}
