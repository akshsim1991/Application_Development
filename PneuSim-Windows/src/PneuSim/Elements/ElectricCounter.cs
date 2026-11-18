using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

public class ElectricCounter : CircuitElement
{
	private int _preset;

	private int _count;

	private bool _countInput;

	private bool _resetInput;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Counter")]
	[DisplayName("Preset")]
	[Description("Number of pulses after which the counter's contacts switch (1 to 9999).")]
	public int Preset
	{
		get
		{
			return _preset;
		}
		set
		{
			_preset = Math.Max(1, Math.Min(9999, value));
		}
	}

	[Browsable(false)]
	public int Count => _count;

	[Browsable(false)]
	public bool Active => _count >= _preset;

	public override RectangleF LocalBounds => new RectangleF(4f, 0f, 72f, 60f);

	public override PointF LabelAnchor => new PointF(76f, 30f);

	[Browsable(false)]
	public override bool IsManuallyOperated => true;

	public ElectricCounter()
	{
		_preset = 3;
		TypeName = "ElectricCounter";
		DisplayName = "Preset counter";
		AddPort("A1", 20f, 0f, 0f, -1f, vents: false, PortKind.Electric);
		AddPort("A2", 20f, 60f, 0f, 1f, vents: false, PortKind.Electric);
		AddPort("R1", 60f, 0f, 0f, -1f, vents: false, PortKind.Electric);
		AddPort("R2", 60f, 60f, 0f, 1f, vents: false, PortKind.Electric);
		base.Label = "C1";
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
		return "Counting coil burnt out (does not count)";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string Name, string Value)> list = base.InspectValues();
		list.Add(("Count", $"{_count} of {_preset}"));
		list.Add(("Contacts", Active ? "switched" : "at rest"));
		return list;
	}

	private static bool Energized(Port a, Port b)
	{
		if (a.State != PortState.Pressurized || b.State != PortState.Exhausted)
		{
			if (b.State == PortState.Pressurized)
			{
				return a.State == PortState.Exhausted;
			}
			return false;
		}
		return true;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 20f, 0f, 20f, 18f);
		g.DrawLine(r.PenFor(base.Ports[1]), 20f, 42f, 20f, 60f);
		g.DrawLine(r.PenFor(base.Ports[2]), 60f, 0f, 60f, 18f);
		g.DrawLine(r.PenFor(base.Ports[3]), 60f, 42f, 60f, 60f);
		using (SolidBrush brush = new SolidBrush((r.Simulating && Active) ? Color.FromArgb(255, 210, 210) : Color.White))
		{
			g.FillRectangle(brush, 8f, 18f, 64f, 24f);
		}
		g.DrawRectangle((r.Simulating && _countInput) ? r.Energized : r.Line, 8f, 18f, 64f, 24f);
		g.DrawLine(r.Thin, 40f, 18f, 40f, 42f);
		g.DrawString("+1", r.SmallFont, r.TextBrush, 13f, 23f);
		g.DrawString("R", r.SmallFont, r.TextBrush, 53f, 23f);
		g.DrawString(r.Simulating ? $"{_count}/{_preset}" : $"n = {_preset}", r.SmallFont, r.TextBrush, 76f, 40f);
		if (!string.IsNullOrEmpty(base.CrossReference))
		{
			g.DrawString(base.CrossReference.Replace("  ", "\n"), r.SmallFont, r.MarkBrush, 76f, 50f);
		}
	}

	public override void OnSimMouseDown(PointF local)
	{
		_count = 0;
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_count = 0;
		_countInput = false;
		_resetInput = false;
	}

	public override bool UpdateLogic(Simulator sim)
	{
		bool active = Active;
		bool flag = Energized(base.Ports[0], base.Ports[1]) && base.Fault != FaultKind.BurntCoil;
		bool flag2 = Energized(base.Ports[2], base.Ports[3]);
		bool flag3 = false;
		if (flag2)
		{
			if (_count != 0)
			{
				_count = 0;
				flag3 = true;
			}
		}
		else if (flag && !_countInput)
		{
			_count = Math.Min(checked(_count + 1), 99999);
			flag3 = true;
		}
		_countInput = flag;
		_resetInput = flag2;
		if (flag3)
		{
			return active != Active;
		}
		return false;
	}
}
