using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class Junction : CircuitElement
{
	private bool _electric;

	public override string TypeName { get; }

	public override string DisplayName => Medium switch
	{
		PortKind.Electric => "Wire junction / bend point", 
		PortKind.Hydraulic => "Hydraulic junction / bend point", 
		_ => "Tube junction / bend point", 
	};

	[Browsable(false)]
	public bool IsElectric
	{
		get
		{
			return _electric;
		}
		set
		{
			Medium = (value ? PortKind.Electric : PortKind.Pneumatic);
		}
	}

	[Category("Junction")]
	[DisplayName("Joins")]
	[Description("Tubes (pneumatic), wires (electric) or hydraulic lines.")]
	public PortKind Medium
	{
		get
		{
			return base.Ports[0].Kind;
		}
		set
		{
			_electric = value == PortKind.Electric;
			base.Ports[0].Kind = value;
		}
	}

	public override RectangleF LocalBounds => new RectangleF(-5f, -5f, 10f, 10f);

	public override PointF LabelAnchor => new PointF(6f, 0f);

	public Junction()
	{
		TypeName = "Junction";
		AddPort("1", 0f, 0f, 0f, 0f);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		Port port = base.Ports[0];
		Color color = ((r.Simulating && port.IsPressurized) ? RenderContext.ActiveColor(port.Kind) : Color.Black);
		if (port.ConnectionCount >= 3)
		{
			using (SolidBrush brush = new SolidBrush(color))
			{
				g.FillEllipse(brush, -3.5f, -3.5f, 7f, 7f);
				return;
			}
		}
		if (r.Interactive && !r.Simulating)
		{
			using (Pen pen = new Pen(Color.FromArgb(150, 150, 150)))
			{
				g.DrawEllipse(pen, -2.5f, -2.5f, 5f, 5f);
			}
		}
	}
}
