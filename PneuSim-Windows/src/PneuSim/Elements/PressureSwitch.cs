using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace PneuSim;

public class PressureSwitch : CircuitElement, ISignalSource
{
	private double _setting;

	private bool _on;

	public const double Hysteresis = 0.2;

	public override string TypeName { get; }

	public override string DisplayName
	{
		get
		{
			if (!(_setting < 0.0))
			{
				return "Pressure switch";
			}
			return "Vacuum switch";
		}
	}

	[Category("Pressure switch")]
	[DisplayName("Switching pressure (bar)")]
	[Description("The switch operates when the pressure reaches this value. Use a negative value (e.g. -0.5) for a vacuum switch.")]
	public double Setting
	{
		get
		{
			return _setting;
		}
		set
		{
			_setting = Math.Max(-0.95, Math.Min(400.0, value));
		}
	}

	[Category("Medium")]
	[DisplayName("Hydraulic")]
	[Description("True: fitted in a hydraulic (oil) line. False: compressed air.")]
	public bool Hydraulic
	{
		get
		{
			return base.Ports[0].Kind == PortKind.Hydraulic;
		}
		set
		{
			base.Ports[0].Kind = (value ? PortKind.Hydraulic : PortKind.Pneumatic);
		}
	}

	[Browsable(false)]
	public bool IsOn => _on;

	public override RectangleF LocalBounds => new RectangleF(0f, 4f, 44f, 56f);

	public override PointF LabelAnchor => new PointF(46f, 18f);

	public PressureSwitch()
	{
		_setting = 4.0;
		TypeName = "PressureSwitch";
		AddPort("1", 20f, 60f, 0f, 1f);
	}

	public bool IsSignalOn(string name)
	{
		if (_on)
		{
			return string.Equals(base.Label?.Trim(), name, StringComparison.OrdinalIgnoreCase);
		}
		return false;
	}

	bool ISignalSource.IsSignalOn(string name)
	{
		//ILSpy generated this explicit interface implementation from .override directive in IsSignalOn
		return this.IsSignalOn(name);
	}

	public IEnumerable<string> SignalNames()
	{
		if (!string.IsNullOrWhiteSpace(base.Label))
		{
			return new string[1] { base.Label.Trim() };
		}
		return Array.Empty<string>();
	}

	IEnumerable<string> ISignalSource.SignalNames()
	{
		//ILSpy generated this explicit interface implementation from .override directive in SignalNames
		return this.SignalNames();
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[2]
		{
			FaultKind.StuckNormal,
			FaultKind.StuckOperated
		};
	}

	public override string FaultDescription(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.StuckNormal => "Pressure switch does not switch (sensing port blocked or switch broken)", 
			FaultKind.StuckOperated => "Pressure switch always switched (stuck)", 
			_ => base.FaultDescription(kind), 
		};
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Switching pressure", $"{_setting:0.0#} bar"));
		list.Add(("Switch", _on ? "operated" : "at rest"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		Port port = base.Ports[0];
		g.DrawLine(r.PenFor(port), 20f, 60f, 20f, 44f);
		using (Pen pen = new Pen((r.Simulating && port.IsPressurized) ? RenderContext.ActiveColor(port.Kind) : Color.Black, 1.2f)
		{
			DashStyle = DashStyle.Dash
		})
		{
			g.DrawLine(pen, 20f, 44f, 20f, 36f);
		}
		Symbols.EnergyTriangle(g, r, new PointF[3]
		{
			new PointF(20f, 30f),
			new PointF(16f, 36f),
			new PointF(24f, 36f)
		}, Hydraulic, r.Simulating && port.IsPressurized);
		g.FillRectangle(r.BodyBrush, 4f, 8f, 32f, 22f);
		g.DrawRectangle(r.Line, 4f, 8f, 32f, 22f);
		bool flag = r.Simulating && _on;
		g.DrawLine(r.Thin, 10f, 24f, 16f, 24f);
		g.DrawLine(flag ? r.Energized : r.Line, 16f, 24f, 28f, flag ? 24 : 16);
		g.DrawLine(r.Thin, 28f, 24f, 32f, 24f);
		string text = ((!r.Simulating) ? $"{_setting:0.0#} bar" : (Hydraulic ? $"{port.Pressure:0} bar" : $"{port.Pressure:0.0} bar"));
		g.DrawString(text, r.SmallFont, r.TextBrush, 26f, 42f);
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_on = false;
	}

	public override bool UpdateLogic(Simulator sim)
	{
		double pressure = base.Ports[0].Pressure;
		bool flag = ((!(_setting >= 0.0)) ? (_on ? (pressure <= _setting + 0.2) : (pressure <= _setting + 1E-06)) : (_on ? (pressure >= _setting - 0.2) : (pressure >= _setting - 1E-06)));
		if (base.Fault == FaultKind.StuckNormal)
		{
			flag = false;
		}
		if (base.Fault == FaultKind.StuckOperated)
		{
			flag = true;
		}
		if (flag == _on)
		{
			return false;
		}
		_on = flag;
		return true;
	}
}
