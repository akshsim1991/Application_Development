using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;

namespace PneuSim;

/// <summary>
/// DC motor (12 V or 24 V). Two terminals: +24 V on M1 and 0 V on M2 turns it one way, the reverse turns it the other way.
/// Model: I = (V - kE*w) / R, T = kT*I, J*dw/dt = T - load - friction - damping*w.
/// </summary>
public class DCMotor : CircuitElement
{
	private double _ratedVoltage = 24.0;

	private double _resistance = 2.0;

	private double _kE = 0.06;

	private double _inertia = 0.0004;

	private double _loadTorque = 0.01;

	private double _friction = 0.002;

	private double _damping = 1E-05;

	private double _omega;

	private double _angle;

	private double _voltage;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Motor")]
	[DisplayName("Rated voltage (V)")]
	[Description("12 V or 24 V motor. A 12 V motor on the 24 V supply runs twice as fast and is shown as over-voltage.")]
	public double RatedVoltage
	{
		get
		{
			return _ratedVoltage;
		}
		set
		{
			_ratedVoltage = Math.Max(6.0, Math.Min(48.0, value));
		}
	}

	[Category("Motor")]
	[DisplayName("Armature resistance (ohm)")]
	[Description("Winding resistance R. Lower values give more current and torque.")]
	public double ArmatureResistance
	{
		get
		{
			return _resistance;
		}
		set
		{
			_resistance = Math.Max(0.05, Math.Min(100.0, value));
		}
	}

	[Category("Motor")]
	[DisplayName("Voltage constant kE (V.s/rad)")]
	[Description("Back-EMF per rad/s. The torque constant kT (N.m per A) has the same value in SI units. No-load speed is about the rated voltage divided by kE.")]
	public double VoltageConstant
	{
		get
		{
			return _kE;
		}
		set
		{
			_kE = Math.Max(0.001, Math.Min(5.0, value));
		}
	}

	[Category("Mechanics")]
	[DisplayName("Rotor and load inertia (kg.m2)")]
	[Description("Moment of inertia J. Larger values make the motor accelerate more slowly.")]
	public double Inertia
	{
		get
		{
			return _inertia;
		}
		set
		{
			_inertia = Math.Max(1E-06, Math.Min(10.0, value));
		}
	}

	[Category("Mechanics")]
	[DisplayName("Load torque (N.m)")]
	[Description("Constant opposing torque from the driven machine.")]
	public double LoadTorque
	{
		get
		{
			return _loadTorque;
		}
		set
		{
			_loadTorque = Math.Max(0.0, Math.Min(1000.0, value));
		}
	}

	[Category("Mechanics")]
	[DisplayName("Friction torque (N.m)")]
	[Description("Bearing friction.")]
	public double FrictionTorque
	{
		get
		{
			return _friction;
		}
		set
		{
			_friction = Math.Max(0.0, Math.Min(100.0, value));
		}
	}

	[Category("Mechanics")]
	[DisplayName("Viscous damping (N.m.s/rad)")]
	[Description("Speed-dependent drag (for example from a fan or pump).")]
	public double Damping
	{
		get
		{
			return _damping;
		}
		set
		{
			_damping = Math.Max(0.0, Math.Min(10.0, value));
		}
	}

	[Browsable(false)]
	public double Rpm => _omega * 60.0 / (Math.PI * 2.0);

	[Browsable(false)]
	public double Current => (_voltage - _kE * _omega) / _resistance;

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 40f, 60f);

	public override PointF LabelAnchor => new PointF(42f, 30f);

	public DCMotor()
	{
		TypeName = "DCMotor";
		DisplayName = "DC motor";
		AddPort("M1", 20f, 0f, 0f, -1f, vents: false, PortKind.Electric);
		AddPort("M2", 20f, 60f, 0f, 1f, vents: false, PortKind.Electric);
		base.Label = "M1";
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[2]
		{
			FaultKind.BurntCoil,
			FaultKind.Jammed
		};
	}

	public override string FaultDescription(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.BurntCoil => "Motor winding burnt out (open circuit)", 
			FaultKind.Jammed => "Shaft jammed (stalled)", 
			_ => base.FaultDescription(kind), 
		};
	}

	private double SupplyVoltage()
	{
		if (base.Fault == FaultKind.BurntCoil)
		{
			return 0.0;
		}
		Port port = base.Ports[0];
		Port port2 = base.Ports[1];
		if (port.State == PortState.Pressurized && port2.State == PortState.Exhausted)
		{
			return 24.0;
		}
		if (port2.State == PortState.Pressurized && port.State == PortState.Exhausted)
		{
			return -24.0;
		}
		return 0.0;
	}

	public override bool UpdateLogic(Simulator sim)
	{
		_voltage = SupplyVoltage();
		return false;
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		_voltage = SupplyVoltage();
		if (base.Fault == FaultKind.Jammed)
		{
			_omega = 0.0;
			return;
		}
		double num = _kE * _voltage / _resistance;
		double num2 = _loadTorque + _friction;
		if (Math.Abs(_omega) < 1E-06 && Math.Abs(num) <= num2)
		{
			_omega = 0.0;
			return;
		}
		double num3 = ((Math.Abs(_omega) > 1E-06) ? Math.Sign(_omega) : Math.Sign(num));
		double num4 = (_kE * _kE / _resistance + _damping) / _inertia;
		double num5 = (num - num3 * num2) / _inertia;
		double num6 = num5 / num4;
		double num7 = num6 + (_omega - num6) * Math.Exp((0.0 - num4) * dt);
		if (_omega != 0.0 && num7 * _omega < 0.0)
		{
			num7 = 0.0;
		}
		_omega = num7;
		_angle += _omega * dt;
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_omega = 0.0;
		_angle = 0.0;
		_voltage = 0.0;
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string, string)> list = base.InspectValues();
		list.Add(("Voltage", $"{_voltage:0.#} V" + ((Math.Abs(_voltage) > _ratedVoltage * 1.2) ? $"  (OVER-VOLTAGE: rated {_ratedVoltage:0} V)" : "")));
		list.Add(("Speed", $"{Rpm:0} rpm" + ((Math.Abs(_omega) < 0.01) ? "" : ((_omega > 0.0) ? "  (forward)" : "  (reverse)"))));
		list.Add(("Current", $"{Current:0.00} A"));
		list.Add(("Torque", $"{_kE * Current:0.000} N.m"));
		list.Add(("Power", $"{Math.Abs(_voltage * Current):0.0} W"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 20f, 0f, 20f, 10f);
		g.DrawLine(r.PenFor(base.Ports[1]), 20f, 50f, 20f, 60f);
		bool flag = r.Simulating && Math.Abs(_voltage) > _ratedVoltage * 1.2;
		g.FillEllipse(r.BodyBrush, 2f, 10f, 36f, 36f);
		g.DrawEllipse(flag ? Pens.Red : r.Line, 2f, 10f, 36f, 36f);
		g.DrawString("M", r.Font, r.TextBrush, 13f, 12f);
		g.DrawString("DC", r.SmallFont, r.TextBrush, 12f, 28f);
		if (r.Simulating)
		{
			double a = _angle;
			g.DrawLine(r.Thin, 20f, 28f, 20f + (float)(Math.Cos(a) * 13.0), 28f + (float)(Math.Sin(a) * 13.0));
			g.DrawString($"{Rpm:0} rpm", r.SmallFont, r.TextBrush, 0f, 48f);
		}
	}
}

/// <summary>
/// Base of the positioning drives (servo motor, electric cylinder). 24 V inputs: Enable and Select P1-P3 choose one of three stored positions
/// (none selected = home, 0); the output In position is 24 V when the target is reached. Motion has a trapezoidal speed profile.
/// </summary>
public abstract class ElectricAxisBase : CircuitElement, ISignalSource
{
	protected double _pos;

	protected double _vel;

	protected double _target;

	protected bool _powered;

	protected bool _enabled;

	protected int _select;

	protected bool _inPosition;

	private double[] _stored = new double[3];

	private string[] _marks = new string[3] { "", "", "" };

	private double _maxSpeed;

	private double _accel;

	private double _tolerance;

	protected const int PEn = 2;

	protected const int PP1 = 3;

	protected const int POut = 6;

	protected abstract double RangeMin { get; }

	protected abstract double RangeMax { get; }

	protected abstract string UnitName { get; }

	protected double SpeedLimit
	{
		get
		{
			return _maxSpeed;
		}
		set
		{
			_maxSpeed = Math.Max(0.001, value);
		}
	}

	protected double AccelLimit
	{
		get
		{
			return _accel;
		}
		set
		{
			_accel = Math.Max(0.001, value);
		}
	}

	protected double Tol
	{
		get
		{
			return _tolerance;
		}
		set
		{
			_tolerance = Math.Max(1E-06, value);
		}
	}

	protected double Stored(int i)
	{
		return _stored[i];
	}

	protected void SetStored(int i, double v)
	{
		_stored[i] = Math.Max(RangeMin, Math.Min(RangeMax, v));
	}

	protected string Mark(int i)
	{
		return _marks[i];
	}

	protected void SetMark(int i, string v)
	{
		_marks[i] = v ?? "";
	}

	[Browsable(false)]
	public double Position => _pos;

	[Browsable(false)]
	public bool InPosition => _inPosition;

	protected void AddAxisPorts(float width)
	{
		AddPort("24V", 22f, 0f, 0f, -1f, vents: false, PortKind.Electric);
		AddPort("0V", 40f, 0f, 0f, -1f, vents: false, PortKind.Electric);
		AddPort("EN", 0f, 26f, -1f, 0f, vents: false, PortKind.Electric);
		AddPort("P1", 0f, 42f, -1f, 0f, vents: false, PortKind.Electric);
		AddPort("P2", 0f, 58f, -1f, 0f, vents: false, PortKind.Electric);
		AddPort("P3", 0f, 74f, -1f, 0f, vents: false, PortKind.Electric);
		AddPort("OK", width, 26f, 1f, 0f, vents: false, PortKind.Electric);
	}

	public override FaultKind[] PossibleFaults()
	{
		return new FaultKind[2]
		{
			FaultKind.BurntCoil,
			FaultKind.Jammed
		};
	}

	public override string FaultDescription(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.BurntCoil => "Drive electronics or motor burnt out (no power)", 
			FaultKind.Jammed => "Mechanically jammed", 
			_ => base.FaultDescription(kind), 
		};
	}

	public override bool UpdateLogic(Simulator sim)
	{
		bool powered = base.Ports[0].State == PortState.Pressurized && base.Ports[1].State == PortState.Exhausted && base.Fault != FaultKind.BurntCoil;
		bool enabled = powered && base.Ports[2].State == PortState.Pressurized;
		int num = (!powered) ? 0 : ((base.Ports[5].State == PortState.Pressurized) ? 3 : ((base.Ports[4].State == PortState.Pressurized) ? 2 : ((base.Ports[3].State == PortState.Pressurized) ? 1 : 0)));
		double num2 = ((num == 0) ? 0.0 : _stored[num - 1]);
		bool flag = enabled && Math.Abs(_pos - num2) <= _tolerance && Math.Abs(_vel) <= _tolerance * 5.0;
		bool result = powered != _powered || enabled != _enabled || num != _select || flag != _inPosition;
		_powered = powered;
		_enabled = enabled;
		_select = num;
		_target = num2;
		_inPosition = flag;
		return result;
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		if (base.Fault == FaultKind.Jammed)
		{
			_vel = 0.0;
			return;
		}
		double num;
		if (!_enabled)
		{
			num = 0.0;
		}
		else
		{
			double num2 = _target - _pos;
			double val = Math.Sqrt(2.0 * _accel * Math.Abs(num2));
			num = (double)Math.Sign(num2) * Math.Min(_maxSpeed, val);
		}
		double num3 = _accel * dt;
		_vel += Math.Max(0.0 - num3, Math.Min(num3, num - _vel));
		double pos = _pos;
		_pos += _vel * dt;
		if (_enabled && (_target - pos) * (_target - _pos) <= 0.0 && Math.Abs(_vel) < _maxSpeed * 0.2)
		{
			_pos = _target;
			_vel = 0.0;
		}
		if (_pos < RangeMin || _pos > RangeMax)
		{
			_pos = Math.Max(RangeMin, Math.Min(RangeMax, _pos));
			_vel = 0.0;
		}
	}

	public override void AddTerminals(Simulator sim)
	{
		if (_inPosition)
		{
			sim.AddSupply(base.Ports[6], 24.0);
		}
	}

	public override void ResetSim()
	{
		base.ResetSim();
		_pos = 0.0;
		_vel = 0.0;
		_target = 0.0;
		_powered = false;
		_enabled = false;
		_select = 0;
		_inPosition = false;
	}

	public bool IsSignalOn(string name)
	{
		for (int i = 0; i < 3; i++)
		{
			if (!string.IsNullOrWhiteSpace(_marks[i]) && string.Equals(_marks[i].Trim(), name?.Trim(), StringComparison.OrdinalIgnoreCase) && Math.Abs(_pos - _stored[i]) <= _tolerance)
			{
				return true;
			}
		}
		return false;
	}

	public IEnumerable<string> SignalNames()
	{
		for (int i = 0; i < 3; i++)
		{
			if (!string.IsNullOrWhiteSpace(_marks[i]))
			{
				yield return _marks[i].Trim();
			}
		}
	}

	protected string Status()
	{
		if (!_powered)
		{
			return "no power";
		}
		if (!_enabled)
		{
			return "not enabled";
		}
		return _inPosition ? "in position" : "moving";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string, string)> list = base.InspectValues();
		list.Add(("Position", $"{_pos:0.0} {UnitName}"));
		list.Add(("Target", $"{_target:0.0} {UnitName}" + ((_select == 0) ? " (home)" : $" (P{_select})")));
		list.Add(("Speed", $"{_vel:0.0} {UnitName}/s"));
		list.Add(("Status", Status()));
		return list;
	}

	protected void DrawInputLabels(DrawSurface g, RenderContext r, float width)
	{
		g.DrawLine(r.PenFor(base.Ports[0]), 22f, 0f, 22f, 10f);
		g.DrawLine(r.PenFor(base.Ports[1]), 40f, 0f, 40f, 10f);
		for (int i = 2; i <= 5; i++)
		{
			Port port = base.Ports[i];
			g.DrawLine(r.PenFor(port), 0f, port.Local.Y, 12f, port.Local.Y);
		}
		g.DrawLine(r.PenFor(base.Ports[6]), width - 12f, 26f, width, 26f);
		g.DrawString("+24V", r.SmallFont, r.TextBrush, 12f, 11f);
		g.DrawString("0V", r.SmallFont, r.TextBrush, 36f, 11f);
		g.DrawString("EN", r.SmallFont, r.TextBrush, 14f, 20f);
		g.DrawString("P1", r.SmallFont, r.TextBrush, 14f, 36f);
		g.DrawString("P2", r.SmallFont, r.TextBrush, 14f, 52f);
		g.DrawString("P3", r.SmallFont, r.TextBrush, 14f, 68f);
		g.DrawString("OK", r.SmallFont, r.TextBrush, width - 30f, 20f);
	}
}

/// <summary>Servo motor with three stored angles chosen by the 24 V inputs P1 to P3.</summary>
public class ServoMotor : ElectricAxisBase
{
	public override string TypeName { get; }

	public override string DisplayName { get; }

	protected override double RangeMin => -3600.0;

	protected override double RangeMax => 3600.0;

	protected override string UnitName => "deg";

	[Category("Servo motor")]
	[DisplayName("Position P1 (deg)")]
	[Description("Angle reached when input P1 is 24 V.")]
	public double Position1
	{
		get
		{
			return Stored(0);
		}
		set
		{
			SetStored(0, value);
		}
	}

	[Category("Servo motor")]
	[DisplayName("Position P2 (deg)")]
	[Description("Angle reached when input P2 is 24 V (P3 wins over P2, P2 over P1).")]
	public double Position2
	{
		get
		{
			return Stored(1);
		}
		set
		{
			SetStored(1, value);
		}
	}

	[Category("Servo motor")]
	[DisplayName("Position P3 (deg)")]
	[Description("Angle reached when input P3 is 24 V. With no input selected the servo goes home (0 deg).")]
	public double Position3
	{
		get
		{
			return Stored(2);
		}
		set
		{
			SetStored(2, value);
		}
	}

	[Category("Servo motor")]
	[DisplayName("Maximum speed (deg/s)")]
	public double MaxSpeed
	{
		get
		{
			return SpeedLimit;
		}
		set
		{
			SpeedLimit = value;
		}
	}

	[Category("Servo motor")]
	[DisplayName("Acceleration (deg/s2)")]
	[Description("Trapezoidal profile: accelerate, cruise at maximum speed, decelerate.")]
	public double Acceleration
	{
		get
		{
			return AccelLimit;
		}
		set
		{
			AccelLimit = value;
		}
	}

	[Category("Servo motor")]
	[DisplayName("In-position tolerance (deg)")]
	public double Tolerance
	{
		get
		{
			return Tol;
		}
		set
		{
			Tol = value;
		}
	}

	[Category("Position marks")]
	[DisplayName("Mark at P1")]
	[Description("Name of the sensor signal that is on while the servo is at P1. A proximity sensor or limit switch with the same name reacts to it.")]
	public string MarkP1
	{
		get
		{
			return Mark(0);
		}
		set
		{
			SetMark(0, value);
		}
	}

	[Category("Position marks")]
	[DisplayName("Mark at P2")]
	public string MarkP2
	{
		get
		{
			return Mark(1);
		}
		set
		{
			SetMark(1, value);
		}
	}

	[Category("Position marks")]
	[DisplayName("Mark at P3")]
	public string MarkP3
	{
		get
		{
			return Mark(2);
		}
		set
		{
			SetMark(2, value);
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 110f, 84f);

	public override PointF LabelAnchor => new PointF(0f, -4f);

	public ServoMotor()
	{
		TypeName = "ServoMotor";
		DisplayName = "Servo motor";
		SetStored(0, 90.0);
		SetStored(1, 180.0);
		SetStored(2, 270.0);
		SpeedLimit = 360.0;
		AccelLimit = 1440.0;
		Tol = 0.5;
		AddAxisPorts(110f);
		base.Label = "M1";
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.FillRectangle(r.BodyBrush, 12f, 10f, 86f, 74f);
		g.DrawRectangle(r.Line, 12f, 10f, 86f, 74f);
		DrawInputLabels(g, r, 110f);
		g.FillEllipse(Brushes.White, 54f, 30f, 36f, 36f);
		g.DrawEllipse(r.Line, 54f, 30f, 36f, 36f);
		g.DrawString("M", r.Font, r.TextBrush, 64f, 32f);
		g.DrawString("servo", r.SmallFont, r.TextBrush, 60f, 50f);
		double num = (_pos - 90.0) * Math.PI / 180.0;
		g.DrawLine(r.Line, 72f, 48f, 72f + (float)(Math.Cos(num) * 16.0), 48f + (float)(Math.Sin(num) * 16.0));
		if (r.Simulating)
		{
			g.DrawString($"{_pos:0.0} deg", r.SmallFont, r.TextBrush, 40f, 68f);
		}
	}
}

/// <summary>Electric cylinder: a servo motor turning a lead screw, with three stored positions and position marks.</summary>
public class ElectricCylinder : ElectricAxisBase
{
	private int _stroke = 200;

	private double _lead = 10.0;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	protected override double RangeMin => 0.0;

	protected override double RangeMax => _stroke;

	protected override string UnitName => "mm";

	[Category("Electric cylinder")]
	[DisplayName("Stroke (mm)")]
	public int StrokeLength
	{
		get
		{
			return _stroke;
		}
		set
		{
			_stroke = Math.Max(10, Math.Min(2000, value));
		}
	}

	[Category("Electric cylinder")]
	[DisplayName("Screw lead (mm/rev)")]
	[Description("Travel per motor revolution. Motor speed (rpm) = speed (mm/s) x 60 / lead.")]
	public double ScrewLead
	{
		get
		{
			return _lead;
		}
		set
		{
			_lead = Math.Max(0.5, Math.Min(100.0, value));
		}
	}

	[Category("Electric cylinder")]
	[DisplayName("Position P1 (mm)")]
	public double Position1
	{
		get
		{
			return Stored(0);
		}
		set
		{
			SetStored(0, value);
		}
	}

	[Category("Electric cylinder")]
	[DisplayName("Position P2 (mm)")]
	public double Position2
	{
		get
		{
			return Stored(1);
		}
		set
		{
			SetStored(1, value);
		}
	}

	[Category("Electric cylinder")]
	[DisplayName("Position P3 (mm)")]
	[Description("With no input selected the cylinder goes home (0 mm).")]
	public double Position3
	{
		get
		{
			return Stored(2);
		}
		set
		{
			SetStored(2, value);
		}
	}

	[Category("Electric cylinder")]
	[DisplayName("Maximum speed (mm/s)")]
	public double MaxSpeed
	{
		get
		{
			return SpeedLimit;
		}
		set
		{
			SpeedLimit = value;
		}
	}

	[Category("Electric cylinder")]
	[DisplayName("Acceleration (mm/s2)")]
	public double Acceleration
	{
		get
		{
			return AccelLimit;
		}
		set
		{
			AccelLimit = value;
		}
	}

	[Category("Electric cylinder")]
	[DisplayName("In-position tolerance (mm)")]
	public double Tolerance
	{
		get
		{
			return Tol;
		}
		set
		{
			Tol = value;
		}
	}

	[Category("Position marks")]
	[DisplayName("Mark at P1")]
	[Description("Name of the sensor signal that is on while the cylinder is at P1 (use the same name on a proximity sensor or limit switch).")]
	public string MarkP1
	{
		get
		{
			return Mark(0);
		}
		set
		{
			SetMark(0, value);
		}
	}

	[Category("Position marks")]
	[DisplayName("Mark at P2")]
	public string MarkP2
	{
		get
		{
			return Mark(1);
		}
		set
		{
			SetMark(1, value);
		}
	}

	[Category("Position marks")]
	[DisplayName("Mark at P3")]
	public string MarkP3
	{
		get
		{
			return Mark(2);
		}
		set
		{
			SetMark(2, value);
		}
	}

	public override RectangleF LocalBounds => new RectangleF(0f, 0f, 250f, 84f);

	public override PointF LabelAnchor => new PointF(0f, -4f);

	public ElectricCylinder()
	{
		TypeName = "ElectricCylinder";
		DisplayName = "Electric cylinder (lead screw)";
		SetStored(0, 50.0);
		SetStored(1, 100.0);
		SetStored(2, 150.0);
		SpeedLimit = 100.0;
		AccelLimit = 500.0;
		Tol = 0.2;
		AddAxisPorts(250f);
		base.Label = "1M";
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string, string)> list = base.InspectValues();
		list.Add(("Motor speed", $"{_vel * 60.0 / _lead:0} rpm"));
		return list;
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		g.FillRectangle(r.BodyBrush, 12f, 10f, 86f, 74f);
		g.DrawRectangle(r.Line, 12f, 10f, 86f, 74f);
		DrawInputLabels(g, r, 250f);
		g.FillEllipse(Brushes.White, 20f, 38f, 30f, 30f);
		g.DrawEllipse(r.Line, 20f, 38f, 30f, 30f);
		g.DrawString("M", r.Font, r.TextBrush, 28f, 41f);
		g.FillRectangle(r.BodyBrush, 98f, 40f, 90f, 30f);
		g.DrawRectangle(r.Line, 98f, 40f, 90f, 30f);
		float num = (float)(_pos / (double)_stroke * 60.0);
		g.FillRectangle(r.BodyBrush, 188f, 52f, num + 4f, 6f);
		g.DrawRectangle(r.Line, 188f, 52f, num + 4f, 6f);
		g.DrawLine(r.Thin, 100f, 55f, 186f, 55f);
		g.DrawString("electric cylinder", r.SmallFont, r.TextBrush, 104f, 72f);
		if (r.Simulating)
		{
			g.DrawString($"{_pos:0.0} mm", r.SmallFont, r.TextBrush, 104f, 42f);
		}
	}
}
