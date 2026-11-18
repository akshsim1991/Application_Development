using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;

namespace PneuSim;

public abstract class CylinderBase : CircuitElement, ISignalSource
{
	protected const float BodyLeft = 0f;

	protected const float BodyRight = 120f;

	protected const float BodyTop = 0f;

	protected const float BodyBottom = 30f;

	protected const float PistonWidth = 8f;

	protected const float PistonTravel = 92f;

	protected const float RodLength = 112f;

	public const double Atm = 1.013;

	public const double ValveConductance = 0.35;

	private const double CriticalRatio = 0.3;

	private const double PhysicsDt = 0.0002;

	private double _strokeTime;

	private double _position;

	private bool _physicsReady;

	private double _velocity;

	private double _airCap;

	private double _airRod;

	private int _lastDirection;

	private int _strokeLength;

	private double _boreMm;

	private double _rodMm;

	private double _loadMassKg;

	private double _loadForceN;

	private double _frictionN;

	[Category("Cylinder")]
	[DisplayName("Stroke time (s)")]
	[Description("Ideal mode: time for a full stroke when air can flow freely. Flow control valves make it slower. (Realistic mode calculates the speed from bore, load and flow.)")]
	public double StrokeTime
	{
		get
		{
			return _strokeTime;
		}
		set
		{
			_strokeTime = Math.Max(0.05, Math.Min(60.0, value));
		}
	}

	[Category("Cylinder")]
	[DisplayName("Stroke (mm)")]
	[Description("Stroke length (5 to 5000 mm).")]
	public int StrokeLength
	{
		get
		{
			return _strokeLength;
		}
		set
		{
			_strokeLength = Math.Max(5, Math.Min(5000, value));
		}
	}

	[Category("Physical data (realistic mode)")]
	[DisplayName("Bore (mm)")]
	[Description("Piston diameter (4 to 500 mm).")]
	public double BoreMm
	{
		get
		{
			return _boreMm;
		}
		set
		{
			_boreMm = Math.Max(4.0, Math.Min(500.0, value));
		}
	}

	[Category("Physical data (realistic mode)")]
	[DisplayName("Rod diameter (mm)")]
	[Description("Piston rod diameter; at most 90 % of the bore is used.")]
	public double RodMm
	{
		get
		{
			return _rodMm;
		}
		set
		{
			_rodMm = Math.Max(1.0, Math.Min(400.0, value));
		}
	}

	[Category("Physical data (realistic mode)")]
	[DisplayName("Moving load (kg)")]
	[Description("Mass moved by the piston rod.")]
	public double LoadMassKg
	{
		get
		{
			return _loadMassKg;
		}
		set
		{
			_loadMassKg = Math.Max(0.0, Math.Min(100000.0, value));
		}
	}

	[Category("Physical data (realistic mode)")]
	[DisplayName("Load force (N)")]
	[Description("External force against extension (e.g. a pressing force or a weight on a vertical cylinder). Negative values help extension.")]
	public double LoadForceN
	{
		get
		{
			return _loadForceN;
		}
		set
		{
			_loadForceN = Math.Max(-1000000.0, Math.Min(1000000.0, value));
		}
	}

	[Category("Physical data (realistic mode)")]
	[DisplayName("Friction (N)")]
	public double FrictionN
	{
		get
		{
			return _frictionN;
		}
		set
		{
			_frictionN = Math.Max(0.0, Math.Min(100000.0, value));
		}
	}

	[Category("Physical data (realistic mode)")]
	[DisplayName("End-position cushioning")]
	[Description("Slows the piston down over the last few millimetres of the stroke.")]
	public bool Cushioning { get; set; }

	[Category("Position marks")]
	[DisplayName("Retracted mark")]
	[Description("Name of the limit valve or sensor operated when the piston rod is fully retracted, e.g. 1S1.")]
	public string RetractedMark { get; set; }

	[Category("Position marks")]
	[DisplayName("Extended mark")]
	[Description("Name of the limit valve or sensor operated when the piston rod is fully extended, e.g. 1S2.")]
	public string ExtendedMark { get; set; }

	[Browsable(false)]
	public double Position => _position;

	[Browsable(false)]
	public double Velocity => _velocity;

	[Browsable(false)]
	public double CapPressure
	{
		get
		{
			if (!_physicsReady)
			{
				return base.Ports[0].Pressure;
			}
			return Math.Max(0.0, ChamberAbs(_airCap, CapVolume()) - 1.013);
		}
	}

	[Browsable(false)]
	public double RodPressure
	{
		get
		{
			if (!HasRodPort)
			{
				return 0.0;
			}
			return _physicsReady ? Math.Max(0.0, ChamberAbs(_airRod, RodVolume()) - 1.013) : base.Ports[1].Pressure;
		}
	}

	[Browsable(false)]
	public int CompletedCycles { get; set; }

	protected abstract bool HasRodPort { get; }

	protected virtual double SpringForce(double x) => 0.0; // VB parameterized property SpringForce(x) restored as a method

	protected double EffectiveFrictionN
	{
		get
		{
			if (base.Fault != FaultKind.Sticking)
			{
				return FrictionN;
			}
			return FrictionN * 6.0 + 150.0;
		}
	}

	public override RectangleF LocalBounds => RectangleF.FromLTRB(0f, 0f, 220f, 40f);

	public override PointF LabelAnchor => new PointF(0f, -2f);

	protected float PistonX => (float)(4.0 + _position * 88.0);

	protected CylinderBase()
	{
		_strokeTime = 1.0;
		_strokeLength = 100;
		_boreMm = 32.0;
		_rodMm = 12.0;
		_loadMassKg = 2.0;
		_frictionN = 25.0;
		Cushioning = true;
		RetractedMark = "";
		ExtendedMark = "";
	}

	public override bool ShowProperty(string name)
	{
		if (base.Ports[0].Kind == PortKind.Hydraulic)
		{
			switch (name)
			{
			case "StrokeTime":
			case "LoadMassKg":
			case "Cushioning":
				return false;
			}
		}
		if (this is SemiRotaryActuator)
		{
			switch (name)
			{
			case "StrokeLength":
			case "BoreMm":
			case "RodMm":
			case "LoadMassKg":
			case "LoadForceN":
			case "FrictionN":
			case "Cushioning":
				return false;
			}
		}
		return true;
	}

	public bool IsSignalOn(string name)
	{
		return IsAtMark(name);
	}

	bool ISignalSource.IsSignalOn(string name)
	{
		//ILSpy generated this explicit interface implementation from .override directive in IsSignalOn
		return this.IsSignalOn(name);
	}

	public IEnumerable<string> SignalNames()
	{
		return from m in new string[2] { RetractedMark, ExtendedMark }
			where !string.IsNullOrWhiteSpace(m)
			select m.Trim();
	}

	IEnumerable<string> ISignalSource.SignalNames()
	{
		//ILSpy generated this explicit interface implementation from .override directive in SignalNames
		return this.SignalNames();
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[3]
		{
			FaultKind.Leak,
			FaultKind.Sticking,
			FaultKind.Jammed
		};
	}

	public override string FaultDescription(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.Leak => (base.Ports[0].Kind == PortKind.Hydraulic) ? "Piston seal worn: oil leaks past the piston" : "Piston seal worn: air leaks past the piston", 
			FaultKind.Sticking => "Sticking: worn guide or bent rod, high friction", 
			FaultKind.Jammed => "Jammed: the rod cannot move (mechanical blockage)", 
			_ => base.FaultDescription(kind), 
		};
	}

	public double PistonForce()
	{
		double num = (_physicsReady ? CapPressure : base.Ports[0].Pressure);
		double num2 = ((!HasRodPort) ? 0.0 : (_physicsReady ? RodPressure : base.Ports[1].Pressure));
		return (num * CapArea() - num2 * RodArea()) * 100000.0;
	}

	public double PortFlow(Port p)
	{
		double num;
		if (p == base.Ports[0])
		{
			num = CapArea();
		}
		else
		{
			if (!HasRodPort || p != base.Ports[1])
			{
				return 0.0;
			}
			num = 0.0 - RodArea();
		}
		double num2 = num * _velocity * 1000.0 * 60.0;
		if (p.Kind == PortKind.Hydraulic)
		{
			return num2;
		}
		double num3 = ((!_physicsReady) ? p.Pressure : ((p == base.Ports[0]) ? CapPressure : RodPressure));
		return num2 * (num3 + 1.013) / 1.013;
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string, string)> list = base.InspectValues();
		list.Add(("Position", $"{_position * (double)StrokeLength:0.0} mm of {StrokeLength} mm"));
		list.Add(("Speed", $"{_velocity:0.000} m/s"));
		if (base.Ports[0].Kind == PortKind.Pneumatic)
		{
			list.Add(("Cap-side chamber", $"{(_physicsReady ? CapPressure : base.Ports[0].Pressure):0.00} bar"));
			if (HasRodPort)
			{
				list.Add(("Rod-side chamber", $"{(_physicsReady ? RodPressure : base.Ports[1].Pressure):0.00} bar"));
			}
		}
		list.Add(("Piston force", $"{PistonForce():0} N"));
		list.Add(("Completed cycles", CompletedCycles.ToString()));
		return list;
	}

	public bool IsAtMark(string mark)
	{
		if (string.Equals(mark, RetractedMark, StringComparison.OrdinalIgnoreCase) && _position <= 0.005)
		{
			return true;
		}
		if (string.Equals(mark, ExtendedMark, StringComparison.OrdinalIgnoreCase) && _position >= 0.995)
		{
			return true;
		}
		return false;
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_position = 0.0;
		_velocity = 0.0;
		_physicsReady = false;
		_lastDirection = 0;
		CompletedCycles = 0;
	}

	protected void MoveAtSpeed(double speed, double dt)
	{
		double num = Math.Max(0.001, (double)StrokeLength / 1000.0);
		double position = _position;
		speed *= Faults.SpeedFactor(base.Fault);
		_position = Math.Max(0.0, Math.Min(1.0, _position + speed * dt / num));
		_velocity = (_position - position) * num / Math.Max(dt, 1E-06);
		TrackCycles();
	}

	protected void Move(int direction, double flowFactor, double dt, Simulator sim = null)
	{
		double position = _position;
		flowFactor *= Faults.SpeedFactor(base.Fault);
		_position += (double)direction * flowFactor * dt / _strokeTime;
		_position = Math.Max(0.0, Math.Min(1.0, _position));
		_velocity = (_position - position) * (double)StrokeLength / 1000.0 / Math.Max(dt, 1E-06);
		if (sim != null)
		{
			double num = Math.Abs(_position - position) * (double)StrokeLength / 1000.0;
			double num2 = ((direction > 0) ? CapArea() : RodArea());
			double num3 = ((direction > 0) ? base.Ports[0].Pressure : (HasRodPort ? base.Ports[1].Pressure : 0.0));
			if (direction > 0 || HasRodPort)
			{
				sim.AddAirConsumption(num2 * num * 1000.0 * (num3 + 1.013) / 1.013);
			}
		}
		TrackCycles();
	}

	private void TrackCycles()
	{
		if (_position >= 0.995)
		{
			_lastDirection = 1;
		}
		checked
		{
			if (_position <= 0.005 && _lastDirection == 1)
			{
				_lastDirection = 0;
				CompletedCycles++;
			}
		}
	}

	protected double CapArea()
	{
		return Math.PI / 4.0 * Math.Pow(BoreMm / 1000.0, 2.0);
	}

	protected double RodArea()
	{
		return Math.PI / 4.0 * (Math.Pow(BoreMm / 1000.0, 2.0) - Math.Pow(Math.Min(RodMm, BoreMm * 0.9) / 1000.0, 2.0));
	}

	private double Travel()
	{
		return _position * (double)StrokeLength / 1000.0;
	}

	private double CapVolume()
	{
		return (CapArea() * (0.006 + Travel()) + 1.5E-05) * 1000.0;
	}

	private double RodVolume()
	{
		return (RodArea() * (0.006 + (double)StrokeLength / 1000.0 - Travel()) + 1.5E-05) * 1000.0;
	}

	private static double ChamberAbs(double air, double volume)
	{
		return air * 1.013 / volume;
	}

	private static double Flow(double c, double upAbs, double downAbs)
	{
		if (upAbs <= downAbs || c <= 0.0)
		{
			return 0.0;
		}
		double num = downAbs / upAbs;
		double num2 = ((num <= 0.3) ? 1.0 : Math.Sqrt(Math.Max(0.0, 1.0 - Math.Pow((num - 0.3) / 0.7, 2.0))));
		return c * upAbs * num2;
	}

	private double ChamberFlow(Port port, double chamberAbs, bool outletCushion, Simulator sim)
	{
		double num = 0.35 * Math.Max(port.Factor, 0.0);
		switch (port.State)
		{
		case PortState.Pressurized:
		{
			double num2 = Flow(num, port.SupplyPressure + 1.013, chamberAbs);
			sim.AddAirConsumption(num2 * 0.0002);
			return num2;
		}
		case PortState.Exhausted:
			if (outletCushion)
			{
				num *= 0.08;
			}
			return 0.0 - Flow(num, chamberAbs, 1.013);
		default:
			return 0.0;
		}
	}

	public void InitPhysics()
	{
		_airCap = (base.Ports[0].SupplyPressure * (double)((base.Ports[0].State == PortState.Pressurized) ? 1 : 0) + 1.013) * CapVolume() / 1.013;
		_airRod = (HasRodPort ? (base.Ports[1].SupplyPressure * (double)((base.Ports[1].State == PortState.Pressurized) ? 1 : 0) + 1.013) : 1.013) * RodVolume() / 1.013;
		_physicsReady = true;
	}

	protected void PhysicsStep(Simulator sim, double dt)
	{
		if (!_physicsReady)
		{
			InitPhysics();
		}
		double num = Math.Max(0.005, (double)StrokeLength / 1000.0);
		double num2 = (Cushioning ? Math.Min(0.015, num * 0.2) : 0.0);
		double num3 = 0.3 + Math.Max(0.0, LoadMassKg);
		checked
		{
			int num4 = Math.Max(1, (int)Math.Ceiling(dt / 0.0002));
			double num5 = dt / (double)num4;
			int num6 = num4;
			for (int i = 1; i <= num6; i++)
			{
				double num7 = Travel();
				double num8 = ChamberAbs(_airCap, CapVolume());
				double num9 = (HasRodPort ? ChamberAbs(_airRod, RodVolume()) : 1.013);
				bool outletCushion = num7 > num - num2 && _velocity > 0.0;
				bool outletCushion2 = num7 < num2 && _velocity < 0.0;
				_airCap += ChamberFlow(base.Ports[0], num8, outletCushion2, sim) * num5;
				if (HasRodPort)
				{
					_airRod += ChamberFlow(base.Ports[1], num9, outletCushion, sim) * num5;
				}
				if (base.Fault == FaultKind.Leak)
				{
					if (HasRodPort)
					{
						double num10 = ((num8 > num9) ? Flow(0.25, num8, num9) : (0.0 - Flow(0.25, num9, num8)));
						_airCap -= num10 * num5;
						_airRod += num10 * num5;
					}
					else
					{
						_airCap -= Flow(0.25, num8, 1.013) * num5;
					}
				}
				_airCap = Math.Max(_airCap, 0.0001);
				_airRod = Math.Max(_airRod, 0.0001);
				double num11 = (num8 - 1.013) * 100000.0 * CapArea() - (HasRodPort ? ((num9 - 1.013) * 100000.0 * RodArea()) : 0.0) - LoadForceN - SpringForce(num7) - 60.0 * _velocity;
				double num12 = Math.Max(0.0, FrictionN);
				if (base.Fault == FaultKind.Sticking)
				{
					num12 = num12 * 6.0 + 150.0;
				}
				if (base.Fault == FaultKind.Jammed)
				{
					num12 = 4.4942328371557893E+307;
				}
				double num13;
				if (Math.Abs(_velocity) < 0.0005)
				{
					num13 = ((Math.Abs(num11) <= num12) ? 0.0 : ((num11 - (double)Math.Sign(num11) * num12) / num3));
					if (num13 == 0.0)
					{
						_velocity = 0.0;
					}
				}
				else
				{
					num13 = (num11 - (double)Math.Sign(_velocity) * num12) / num3;
				}
				_velocity += num13 * num5;
				double num14 = num7 + _velocity * num5;
				if (num14 <= 0.0)
				{
					num14 = 0.0;
					if (_velocity < 0.0)
					{
						_velocity = 0.0;
					}
				}
				if (num14 >= num)
				{
					num14 = num;
					if (_velocity > 0.0)
					{
						_velocity = 0.0;
					}
				}
				_position = num14 / num;
				if (double.IsNaN(_position) || double.IsNaN(_velocity))
				{
					_position = 0.0;
					_velocity = 0.0;
					InitPhysics();
					break;
				}
			}
			TrackCycles();
		}
	}

	public double ChamberPressureAt(Port p)
	{
		if (!_physicsReady)
		{
			return -1.0;
		}
		if (p == base.Ports[0])
		{
			return CapPressure;
		}
		if (HasRodPort && p == base.Ports[1])
		{
			return RodPressure;
		}
		return -1.0;
	}

	protected void DrawBody(DrawSurface g, RenderContext r)
	{
		float num = 15f;
		g.FillRectangle(r.BodyBrush, 0f, 0f, 120f, 30f);
		g.DrawRectangle(r.Line, 0f, 0f, 120f, 30f);
		float pistonX = PistonX;
		using (SolidBrush brush = new SolidBrush(Color.FromArgb(70, 70, 70)))
		{
			g.FillRectangle(brush, pistonX, 1.5f, 8f, 27f);
		}
		float num2 = pistonX + 8f;
		float num3 = num2 + 112f;
		g.FillRectangle(r.BodyBrush, num2, num - 3f, num3 - num2, 6f);
		g.DrawRectangle(r.Line, num2, num - 3f, num3 - num2, 6f);
		g.DrawLine(r.Line, 120f, 0f, 120f, 30f);
		DrawMark(g, r, 124f, RetractedMark);
		DrawMark(g, r, 212f, ExtendedMark);
		if (r.Simulating && _physicsReady)
		{
			g.DrawString($"{CapPressure:0.0}", r.SmallFont, r.TextBrush, 2f, 18f);
			if (HasRodPort)
			{
				g.DrawString($"{RodPressure:0.0}", r.SmallFont, r.TextBrush, 100f, 18f);
			}
		}
	}

	private void DrawMark(DrawSurface g, RenderContext r, float x_, string mark)
	{
		if (!string.IsNullOrWhiteSpace(mark))
		{
			using (Pen pen = new Pen(Color.FromArgb(160, 40, 40), 1.4f))
			{
				g.DrawLine(pen, x_, 4f, x_, -6f);
			}
			SizeF sizeF = g.MeasureString(mark, r.SmallFont);
			g.DrawString(mark, r.SmallFont, r.MarkBrush, x_ - sizeF.Width / 2f, -6f - sizeF.Height);
		}
	}

	protected void DrawPortStub(DrawSurface g, RenderContext r, Port p)
	{
		g.DrawLine(r.PenFor(p), p.Local.X, 30f, p.Local.X, p.Local.Y);
	}
}
