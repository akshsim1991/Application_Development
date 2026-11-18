using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Drawing2D;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class ElectricContact : CircuitElement
{
	private bool _closed;

	private bool _manualPressed;

	private bool _detentOn;

	[Browsable(false)]
	public bool IsClosed => _closed;

	public override string TypeName { get; }

	[Browsable(false)]
	public bool IsSensor
	{
		get
		{
			if (Operator != ContactOperator.ProximitySensor && Operator != ContactOperator.LimitSwitch)
			{
				return Operator == ContactOperator.PressureSwitch;
			}
			return true;
		}
	}

	public override string DisplayName => string.Format("{0} ({1})", Operator switch
	{
		ContactOperator.PushButton => "Push button", 
		ContactOperator.Selector => "Selector switch", 
		ContactOperator.Relay => "Relay contact", 
		ContactOperator.ProximitySensor => "Proximity sensor", 
		ContactOperator.PressureSwitch => "Pressure switch contact", 
		ContactOperator.EmergencyStop => "Emergency stop", 
		_ => "Limit switch", 
	}, NormallyClosed ? "NC" : "NO");

	[Category("Contact")]
	[DisplayName("Normally closed")]
	[Description("False: normally open (NO, make contact). True: normally closed (NC, break contact).")]
	public bool NormallyClosed { get; set; }

	[Category("Contact")]
	[DisplayName("Operated by")]
	public ContactOperator Operator { get; set; }

	[Category("Contact")]
	[DisplayName("Reference")]
	[Description("Relay contacts: the relay label (e.g. K1). Sensors and limit switches: the cylinder position mark (e.g. 1B2).")]
	public string Reference { get; set; }

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 40f, 60f);

	public override PointF LabelAnchor => new PointF(26f, 22f);

	[Browsable(false)]
	public override bool IsManuallyOperated
	{
		get
		{
			if (Operator != 0 && Operator != ContactOperator.Selector)
			{
				return Operator == ContactOperator.EmergencyStop;
			}
			return true;
		}
	}

	public ElectricContact()
	{
		TypeName = "ElectricContact";
		Operator = ContactOperator.PushButton;
		Reference = "";
		AddPort("1", 20f, 0f, 0f, -1f, vents: false, PortKind.Electric);
		AddPort("2", 20f, 60f, 0f, 1f, vents: false, PortKind.Electric);
	}

	public override bool ShowProperty(string name)
	{
		if (Operators.CompareString(name, "Reference", TextCompare: false) == 0)
		{
			return Operator == ContactOperator.Relay || Operator == ContactOperator.ProximitySensor || Operator == ContactOperator.LimitSwitch || Operator == ContactOperator.PressureSwitch;
		}
		return true;
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[2]
		{
			FaultKind.ContactOpen,
			FaultKind.ContactWelded
		};
	}

	public override string FaultDescription(FaultKind kind)
	{
		bool flag = kind == FaultKind.ContactWelded;
		switch (Operator)
		{
		case ContactOperator.ProximitySensor:
		case ContactOperator.LimitSwitch:
			return flag ? "Sensor always gives a signal (faulty or set too close)" : "Sensor never gives a signal (misaligned, loose or broken)";
		case ContactOperator.PressureSwitch:
			return flag ? "Pressure switch contact stuck closed" : "Pressure switch contact never closes";
		case ContactOperator.Relay:
			return flag ? "Relay contact welded closed" : "Relay contact burnt (never closes)";
		default:
			return flag ? "Switch contact welded closed" : "Switch contact dirty or broken (never closes)";
		}
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Contact", _closed ? "closed (conducts)" : "open"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		bool flag = (r.Simulating ? _closed : NormallyClosed);
		Port p = base.Ports[0];
		Port port = base.Ports[1];
		Pen pen = r.PenFor(p);
		Pen pen2 = r.PenFor(port);
		g.DrawLine(pen, 20f, 0f, 20f, 18f);
		g.DrawLine(pen2, 20f, 42f, 20f, 60f);
		Pen pen3 = ((flag && r.Simulating && port.IsPressurized) ? r.Energized : r.Line);
		PointF b;
		if (NormallyClosed)
		{
			g.DrawLine(pen, 20f, 18f, 12f, 18f);
			b = (flag ? new PointF(10f, 16f) : new PointF(6f, 26f));
		}
		else
		{
			b = (flag ? new PointF(20f, 18f) : new PointF(9f, 20f));
		}
		g.DrawLine(pen3, new PointF(20f, 42f), b);
		PointF pointF = Symbols.Lerp(new PointF(20f, 42f), b, 0.5f);
		ContactOperator @operator = Operator;
		if ((uint)@operator <= 1u || (uint)(@operator - 3) <= 3u)
		{
			using Pen pen4 = new Pen(Color.Black, 1f)
			{
				DashStyle = DashStyle.Dash
			};
			g.DrawLine(pen4, pointF.X, pointF.Y, 4f, pointF.Y);
		}
		float y = pointF.Y;
		switch (Operator)
		{
		case ContactOperator.PushButton:
			g.DrawLine(r.Line, 3f, y - 6f, 3f, y + 6f);
			g.DrawLine(r.Line, 3f, y - 6f, 6f, y - 6f);
			g.DrawLine(r.Line, 3f, y + 6f, 6f, y + 6f);
			break;
		case ContactOperator.Selector:
			g.DrawLine(r.Line, 3f, y - 6f, 3f, y + 6f);
			g.DrawLine(r.Line, 3f, y - 6f, 6f, y - 6f);
			g.DrawLine(r.Line, 3f, y + 6f, 0f, y + 6f);
			break;
		case ContactOperator.LimitSwitch:
			g.DrawEllipse(r.Line, -2f, y - 4f, 8f, 8f);
			break;
		case ContactOperator.ProximitySensor:
			g.DrawPolygon(r.Line, new PointF[4]
			{
				new PointF(3f, y - 7f),
				new PointF(9f, y),
				new PointF(3f, y + 7f),
				new PointF(-3f, y)
			});
			break;
		case ContactOperator.PressureSwitch:
			g.DrawRectangle(r.Line, -6f, y - 6f, 10f, 12f);
			Symbols.EnergyTriangle(g, r, new PointF[3]
			{
				new PointF(4f, y),
				new PointF(-2f, y - 4f),
				new PointF(-2f, y + 4f)
			}, hydraulic: false, r.Simulating && _closed != NormallyClosed);
			break;
		case ContactOperator.EmergencyStop:
		{
			List<PointF> list = new List<PointF>();
			int num = 90;
			do
			{
				list.Add(new PointF((float)(6.0 * Math.Cos((double)num * Math.PI / 180.0)), (float)((double)y - 8.0 * Math.Sin((double)num * Math.PI / 180.0))));
				num = checked(num + 15);
			}
			while (num <= 270);
			using (SolidBrush brush = new SolidBrush(Color.FromArgb(215, 30, 30)))
			{
				g.FillPolygon(brush, list.ToArray());
			}
			g.DrawArc(r.Line, -6f, y - 8f, 12f, 16f, 90f, 180f);
			g.DrawLine(r.Line, 0f, y - 8f, 0f, y + 8f);
			if (r.Simulating && _detentOn)
			{
				g.DrawString("pressed", r.SmallFont, r.MarkBrush, -8f, y + 9f);
			}
			break;
		}
		}
		if (string.IsNullOrEmpty(base.Label) && !string.IsNullOrWhiteSpace(Reference))
		{
			using Font font = new Font("Segoe UI", 8.5f, FontStyle.Bold);
			g.DrawString(Reference, font, Brushes.Black, 26f, 7f);
		}
		else if (!string.IsNullOrWhiteSpace(Reference))
		{
			g.DrawString(Reference, r.SmallFont, r.MarkBrush, 26f, 24f);
		}
		if (!string.IsNullOrEmpty(base.CrossReference))
		{
			g.DrawString("(" + base.CrossReference + ")", r.SmallFont, r.MarkBrush, 26f, string.IsNullOrEmpty(base.Label) ? 21 : 34);
		}
	}

	public override void OnSimMouseDown(PointF local)
	{
		if (Operator == ContactOperator.PushButton)
		{
			_manualPressed = true;
		}
		if (Operator == ContactOperator.Selector || Operator == ContactOperator.EmergencyStop)
		{
			_detentOn = !_detentOn;
		}
	}

	public override void OnSimMouseUp()
	{
		_manualPressed = false;
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_manualPressed = false;
		_detentOn = false;
		_closed = NormallyClosed;
	}

	public override bool UpdateLogic(Simulator sim)
	{
		bool flag;
		switch (Operator)
		{
		case ContactOperator.PushButton:
			flag = _manualPressed;
			break;
		case ContactOperator.Selector:
		case ContactOperator.EmergencyStop:
			flag = _detentOn;
			break;
		case ContactOperator.Relay:
			flag = sim.IsRelayActive(Reference);
			break;
		default:
			flag = sim.IsMarkActive(Reference);
			break;
		}
		bool flag2 = flag ^ NormallyClosed;
		if (base.Fault == FaultKind.ContactOpen)
		{
			flag2 = false;
		}
		if (base.Fault == FaultKind.ContactWelded)
		{
			flag2 = true;
		}
		if (flag2 == _closed)
		{
			return false;
		}
		_closed = flag2;
		return true;
	}

	public override void AddEdges(Simulator sim)
	{
		if (_closed)
		{
			sim.AddEdge(base.Ports[0], base.Ports[1]);
		}
	}
}
