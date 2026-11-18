using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class ElectricCoil : CircuitElement
{
	private CoilKind _kind;

	private double _delay;

	private bool _energized;

	private bool _active;

	private double _onTime;

	private double _offTime;

	public override string TypeName { get; }

	public override string DisplayName => _kind switch
	{
		CoilKind.OnDelayTimer => "Timer relay (on-delay)", 
		CoilKind.OffDelayTimer => "Timer relay (off-delay)", 
		CoilKind.Solenoid => "Valve solenoid", 
		CoilKind.Lamp => "Indicator lamp", 
		_ => "Relay coil", 
	};

	[Category("Coil")]
	[DisplayName("Type")]
	[Description("Relays and timers operate contacts with the same label. Solenoids operate valves whose solenoid label matches.")]
	public CoilKind Kind
	{
		get
		{
			return _kind;
		}
		set
		{
			_kind = value;
		}
	}

	[Category("Coil")]
	[DisplayName("Delay (s)")]
	[Description("Timer relays: switching delay.")]
	public double DelaySeconds
	{
		get
		{
			return _delay;
		}
		set
		{
			_delay = Math.Max(0.0, Math.Min(600.0, value));
		}
	}

	[Browsable(false)]
	public bool Active => _active;

	public override RectangleF LocalBounds => new RectangleF(4f, 0f, 32f, 60f);

	public override PointF LabelAnchor => new PointF(36f, 30f);

	public ElectricCoil()
	{
		_kind = CoilKind.Relay;
		_delay = 2.0;
		_offTime = double.MaxValue;
		TypeName = "ElectricCoil";
		AddPort("A1", 20f, 0f, 0f, -1f, vents: false, PortKind.Electric);
		AddPort("A2", 20f, 60f, 0f, 1f, vents: false, PortKind.Electric);
		base.Label = "K1";
	}

	public override bool ShowProperty(string name)
	{
		if (Operators.CompareString(name, "DelaySeconds", TextCompare: false) == 0)
		{
			return _kind == CoilKind.OnDelayTimer || _kind == CoilKind.OffDelayTimer;
		}
		return true;
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[1] { FaultKind.BurntCoil };
	}

	public override string FaultDescription(FaultKind kind)
	{
		if (kind != FaultKind.BurntCoil)
		{
			return base.FaultDescription(kind);
		}
		return _kind switch
		{
			CoilKind.Lamp => "Lamp blown", 
			CoilKind.Solenoid => "Solenoid coil burnt out (open circuit)", 
			_ => "Relay coil burnt out (open circuit)", 
		};
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string, string)> list = base.InspectValues();
		list.Add(("Voltage across coil", _energized ? "24 V" : ((base.Ports[0].State == PortState.Pressurized || base.Ports[1].State == PortState.Pressurized) ? "24 V on one side only" : "0 V")));
		list.Add(((_kind == CoilKind.Lamp) ? "Lamp" : "Coil", (!_active) ? "off" : ((_kind == CoilKind.Lamp) ? "lit" : "active")));
		if (_kind == CoilKind.OnDelayTimer && _energized && !_active)
		{
			list.Add(("Timer", $"{_onTime:0.00} of {_delay:0.0#} s"));
		}
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 20f, 0f, 20f, 18f);
		g.DrawLine(r.PenFor(base.Ports[1]), 20f, 42f, 20f, 60f);
		if (!string.IsNullOrEmpty(base.CrossReference))
		{
			g.DrawString(base.CrossReference.Replace("  ", "\n"), r.SmallFont, r.MarkBrush, 36f, 44f);
		}
		bool flag = r.Simulating && _active;
		Pen pen = ((r.Simulating && _energized) ? r.Energized : r.Line);
		if (_kind == CoilKind.Lamp)
		{
			using (SolidBrush brush = new SolidBrush(flag ? Color.FromArgb(255, 220, 40) : Color.White))
			{
				g.FillEllipse(brush, 9f, 19f, 22f, 22f);
			}
			g.DrawEllipse(pen, 9f, 19f, 22f, 22f);
			g.DrawLine(r.Line, 12f, 22f, 28f, 38f);
			g.DrawLine(r.Line, 28f, 22f, 12f, 38f);
			g.DrawLine(r.PenFor(base.Ports[0]), 20f, 18f, 20f, 19f);
			return;
		}
		using (SolidBrush brush2 = new SolidBrush(flag ? Color.FromArgb(255, 210, 210) : Color.White))
		{
			g.FillRectangle(brush2, 8f, 18f, 24f, 24f);
		}
		g.DrawRectangle(pen, 8f, 18f, 24f, 24f);
		switch (_kind)
		{
		case CoilKind.Solenoid:
			g.DrawLine(r.Line, 8f, 42f, 32f, 18f);
			break;
		case CoilKind.OnDelayTimer:
		case CoilKind.OffDelayTimer:
		{
			g.FillRectangle(r.BodyBrush, 0f, 22f, 8f, 16f);
			g.DrawRectangle(r.Line, 0f, 22f, 8f, 16f);
			if (_kind == CoilKind.OnDelayTimer)
			{
				g.FillRectangle(Brushes.Black, 1f, 23f, 6f, 14f);
			}
			else
			{
				g.DrawLine(r.Thin, 0f, 22f, 8f, 38f);
				g.DrawLine(r.Thin, 8f, 22f, 0f, 38f);
			}
			string text = ((r.Simulating && _kind == CoilKind.OnDelayTimer && _energized && !_active) ? $"{_onTime:0.0}/{_delay:0.0}s" : $"{_delay:0.#}s");
			g.DrawString(text, r.SmallFont, r.TextBrush, 36f, 32f);
			break;
		}
		}
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_energized = false;
		_active = false;
		_onTime = 0.0;
		_offTime = double.MaxValue;
	}

	public override bool UpdateLogic(Simulator sim)
	{
		Port port = base.Ports[0];
		Port port2 = base.Ports[1];
		bool flag = (port.State == PortState.Pressurized && port2.State == PortState.Exhausted) || (port2.State == PortState.Pressurized && port.State == PortState.Exhausted);
		_energized = flag && base.Fault != FaultKind.BurntCoil;
		bool flag2 = _kind switch
		{
			CoilKind.OnDelayTimer => _energized && _onTime >= _delay - 1E-06, 
			CoilKind.OffDelayTimer => _energized || _offTime < _delay, 
			_ => _energized, 
		};
		if (flag2 == _active)
		{
			return false;
		}
		_active = flag2;
		return true;
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		if (_energized)
		{
			_onTime += dt;
			_offTime = 0.0;
			return;
		}
		_onTime = 0.0;
		if (_offTime < 8.988465674311579E+307)
		{
			_offTime += dt;
		}
	}
}
