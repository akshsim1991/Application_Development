using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class PowerTerminal : CircuitElement
{
	private Polarity _polarity;

	public override string TypeName { get; }

	public override string DisplayName
	{
		get
		{
			if (_polarity != 0)
			{
				return "Electrical connection 0 V";
			}
			return "Electrical connection +24 V";
		}
	}

	[Category("Supply")]
	[DisplayName("Polarity")]
	public Polarity Polarity
	{
		get
		{
			return _polarity;
		}
		set
		{
			_polarity = value;
			Port port = base.Ports[0];
			if (value == Polarity.Plus24V)
			{
				port.Local = new PointF(20f, 30f);
				port.Direction = new PointF(0f, 1f);
			}
			else
			{
				port.Local = new PointF(20f, 0f);
				port.Direction = new PointF(0f, -1f);
			}
		}
	}

	public override RectangleF LocalBounds => new RectangleF(8f, 0f, 34f, 30f);

	public override PointF LabelAnchor => new PointF(44f, 0f);

	public PowerTerminal()
	{
		_polarity = Polarity.Plus24V;
		TypeName = "PowerTerminal";
		AddPort("1", 20f, 30f, 0f, 1f, vents: false, PortKind.Electric);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		bool flag = _polarity == Polarity.Plus24V;
		float num = (flag ? 10f : 20f);
		g.DrawLine(r.PenFor(base.Ports[0]), 20f, flag ? (num + 5f) : 0f, 20f, flag ? 30f : (num - 5f));
		using (SolidBrush brush = new SolidBrush(flag ? Color.FromArgb(215, 30, 30) : Color.FromArgb(30, 60, 200)))
		{
			g.FillEllipse(brush, 15f, num - 5f, 10f, 10f);
		}
		g.DrawString(flag ? "+24V" : "0V", r.Font, r.TextBrush, 26f, num - 7f);
	}

	public override void AddTerminals(Simulator sim)
	{
		if (_polarity == Polarity.Plus24V)
		{
			sim.AddSupply(base.Ports[0], 24.0);
		}
		else
		{
			sim.AddExhaust(base.Ports[0]);
		}
	}
}
