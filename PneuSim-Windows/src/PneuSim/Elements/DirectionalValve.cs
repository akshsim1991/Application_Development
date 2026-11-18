using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public abstract class DirectionalValve : CircuitElement
{
	protected struct WorkingPort
	{
		public string Name;

		public float Offset;

		public bool Top;

		public bool Vents;

		public WorkingPort(string name, float offset, bool top, bool vents = false)
		{
			this = default(WorkingPort);
			Name = name;
			Offset = offset;
			Top = top;
			Vents = vents;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__104_002D0
	{
		public float _0024VB_0024Local_edge;

		public int _0024VB_0024Local_dir;

		public _Closure_0024__104_002D0(_Closure_0024__104_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_edge = arg0._0024VB_0024Local_edge;
				_0024VB_0024Local_dir = arg0._0024VB_0024Local_dir;
			}
		}

		[SpecialName]
		internal float _Lambda_0024__0(float t)
		{
			return _0024VB_0024Local_edge + (float)_0024VB_0024Local_dir * t;
		}
	}

	protected const float BoxTop = 10f;

	protected const float BoxBottom = 50f;

	private const float SpringW = 14f;

	private const int RestIndex = 1;

	private ValveActuator _actuator;

	private ValveReturn _return;

	private double _delay;

	private int _state;

	private bool _manualPressed;

	private bool _manualRight;

	private bool _detentOn;

	private double _timer;

	private double _pulse;

	private bool _markBefore;

	public const double IdleRollerPulse = 0.15;

	private double _switchPressure;

	private bool _rightActive;

	protected abstract float BoxWidth { get; }

	protected abstract string LeftPilotName { get; }

	protected abstract string RightPilotName { get; }

	protected virtual int PositionCount => 2;

	protected float ActW
	{
		get
		{
			if (PositionCount != 3)
			{
				return 20f;
			}
			return 34f;
		}
	}

	[Category("Actuation")]
	[DisplayName("Operated by")]
	[Description("How the valve is switched into position a (left box).")]
	public ValveActuator Actuator
	{
		get
		{
			return _actuator;
		}
		set
		{
			_actuator = value;
			RebuildPorts();
		}
	}

	[Category("Actuation")]
	[DisplayName("Return / right actuator")]
	[Description("2-position valves: spring return, or a second pilot / solenoid (memory valve). 3-position valves: actuator for position b; the valve is spring centred.")]
	public ValveReturn ReturnType
	{
		get
		{
			return _return;
		}
		set
		{
			if (PositionCount == 3 && value == ValveReturn.Spring)
			{
				value = ((_actuator != ValveActuator.Solenoid) ? ValveReturn.Pilot : ValveReturn.Solenoid);
			}
			_return = value;
			RebuildPorts();
		}
	}

	[Category("Actuation")]
	[DisplayName("Solenoid (left)")]
	[Description("Label of the solenoid coil that operates position a, e.g. 1M1.")]
	public string SolenoidLabel { get; set; }

	[Category("Actuation")]
	[DisplayName("Solenoid (right)")]
	[Description("Label of the solenoid coil on the right side, e.g. 1M2.")]
	public string ReturnSolenoidLabel { get; set; }

	[Category("Actuation")]
	[DisplayName("Roller mark")]
	[Description("For roller lever valves: the cylinder position mark that operates the roller, e.g. 1S2.")]
	public string TriggerMark { get; set; }

	[Category("Actuation")]
	[DisplayName("Switching pressure (bar)")]
	[Description("Pilot pressure at which the valve switches. Raise it to make a pressure sequence valve (most meaningful in realistic mode).")]
	public double SwitchingPressure
	{
		get
		{
			return _switchPressure;
		}
		set
		{
			_switchPressure = Math.Max(0.2, Math.Min(16.0, value));
		}
	}

	[Category("Actuation")]
	[DisplayName("Delay (s)")]
	[Description("For time delay valves: how long the pilot signal must be present.")]
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
	public bool IsRollerOperated
	{
		get
		{
			if (_actuator != ValveActuator.RollerLever)
			{
				return _actuator == ValveActuator.IdleReturnRoller;
			}
			return true;
		}
	}

	[Browsable(false)]
	public bool HasSolenoid
	{
		get
		{
			if (_actuator != ValveActuator.Solenoid)
			{
				return _return == ValveReturn.Solenoid;
			}
			return true;
		}
	}

	[Browsable(false)]
	public int Positions => PositionCount;

	[Browsable(false)]
	public bool IsMemoryValve
	{
		get
		{
			if (PositionCount == 2)
			{
				return _return != ValveReturn.Spring;
			}
			return false;
		}
	}

	[Browsable(false)]
	public float BoxSize => BoxWidth;

	[Browsable(false)]
	public int State => _state;

	[Browsable(false)]
	public double DiagramValue
	{
		get
		{
			if (PositionCount == 2)
			{
				return _state;
			}
			return (_state == 1) ? 1.0 : ((_state == 2) ? 0.0 : 0.5);
		}
	}

	private float Shift => (float)checked(1 - BoxIndex(_state)) * BoxWidth;

	private float TotalWidth => 2f * ActW + (float)PositionCount * BoxWidth;

	protected virtual PortKind Medium => PortKind.Pneumatic;

	private bool HasLeftPilot
	{
		get
		{
			if (_actuator != ValveActuator.Pilot)
			{
				return _actuator == ValveActuator.DelayedPilot;
			}
			return true;
		}
	}

	public override RectangleF LocalBounds
	{
		get
		{
			float shift = Shift;
			return RectangleF.FromLTRB(shift, 0f, TotalWidth + shift, 60f);
		}
	}

	public override PointF LabelAnchor => new PointF(ActW, 78f);

	private bool RightActive => _rightActive;

	[Browsable(false)]
	public override bool IsManuallyOperated
	{
		get
		{
			if (_actuator != 0 && _actuator != ValveActuator.Selector && _actuator != ValveActuator.Solenoid)
			{
				return _return == ValveReturn.Solenoid;
			}
			return true;
		}
	}

	protected DirectionalValve()
	{
		_actuator = ValveActuator.PushButton;
		_return = ValveReturn.Spring;
		_delay = 2.0;
		_markBefore = true;
		SolenoidLabel = "1M1";
		ReturnSolenoidLabel = "1M2";
		TriggerMark = "";
		_switchPressure = 1.5;
	}

	protected abstract WorkingPort[] WorkingPorts();

	protected abstract string[][] Flows(int position);

	protected abstract string[] ClosedPorts(int position);

	public override bool ShowProperty(string name)
	{
		return name switch
		{
			"SolenoidLabel" => _actuator == ValveActuator.Solenoid, 
			"ReturnSolenoidLabel" => _return == ValveReturn.Solenoid, 
			"TriggerMark" => IsRollerOperated, 
			"DelaySeconds" => _actuator == ValveActuator.DelayedPilot, 
			"SwitchingPressure" => _actuator == ValveActuator.Pilot || _actuator == ValveActuator.DelayedPilot || _return == ValveReturn.Pilot, 
			_ => true, 
		};
	}

	public override FaultKind[] PossibleFaults()
	{
		List<FaultKind> list = new List<FaultKind>
		{
			FaultKind.StuckNormal,
			FaultKind.StuckOperated
		};
		if (HasSolenoid)
		{
			list.Add(FaultKind.BurntCoil);
		}
		return list.ToArray();
	}

	public override string FaultDescription(FaultKind kind)
	{
		return kind switch
		{
			FaultKind.StuckNormal => "Spool stuck in the normal position (dirt or varnish)", 
			FaultKind.StuckOperated => (PositionCount == 3) ? "Spool stuck in position a" : "Spool stuck in the switched position", 
			FaultKind.BurntCoil => "Solenoid coil on the valve burnt out (the manual override still works)", 
			_ => base.FaultDescription(kind), 
		};
	}

	public override List<(string Name, string Value)> InspectValues()
	{
		List<(string, string)> list = base.InspectValues();
		list.Add(("Switching position", (_state != 0) ? ((_state == 1) ? "a (left box)" : "b (right box)") : ((PositionCount == 3) ? "centre (normal)" : "normal (rest)")));
		if (_actuator == ValveActuator.DelayedPilot)
		{
			list.Add(("Delay timer", $"{Math.Min(_timer, _delay):0.00} of {_delay:0.0#} s"));
		}
		return list;
	}

	public override void AfterStateRestored()
	{
		RebuildPorts();
	}

	public string[][] PassagesIn(int position)
	{
		return Flows(position);
	}

	public string PilotPortName(bool leftSide)
	{
		if (!leftSide)
		{
			return RightPilotName;
		}
		return LeftPilotName;
	}

	public (string Name, float Offset, bool Top, bool Vents)[] WorkingPortLayout()
	{
		return (from w in WorkingPorts()
			select (w.Name, w.Offset, w.Top, w.Vents)).ToArray();
	}

	public string[] WorkingPortNames()
	{
		return (from w in WorkingPorts()
			select w.Name).ToArray();
	}

	public bool UsesSolenoid(string coilLabel)
	{
		VB_0024AnonymousDelegate_7<string, bool> vB_0024AnonymousDelegate_ = [SpecialName] (string a) => string.Equals(a?.Trim(), coilLabel?.Trim(), StringComparison.OrdinalIgnoreCase);
		if (_actuator != ValveActuator.Solenoid || !vB_0024AnonymousDelegate_(SolenoidLabel))
		{
			if (_return == ValveReturn.Solenoid)
			{
				return vB_0024AnonymousDelegate_(ReturnSolenoidLabel);
			}
			return false;
		}
		return true;
	}

	private int BoxIndex(int position)
	{
		return position switch
		{
			0 => 1, 
			1 => 0, 
			_ => 2, 
		};
	}

	private int PositionAt(int index)
	{
		return index switch
		{
			1 => 0, 
			0 => 1, 
			_ => 2, 
		};
	}

	protected void RebuildPorts()
	{
		float shift = Shift;
		List<Port> list = new List<Port>();
		WorkingPort[] array = WorkingPorts();
		for (int i = 0; i < array.Length; i = checked(i + 1))
		{
			WorkingPort workingPort = array[i];
			list.Add(MakePort(workingPort.Name, ActW + 1f * BoxWidth + workingPort.Offset, workingPort.Top ? 0f : 60f, 0f, workingPort.Top ? (-1f) : 1f, workingPort.Vents));
		}
		if (HasLeftPilot)
		{
			list.Add(MakePort(LeftPilotName, shift, 30f, -1f, 0f, vents: false));
		}
		if (_return == ValveReturn.Pilot)
		{
			list.Add(MakePort(RightPilotName, TotalWidth + shift, 30f, 1f, 0f, vents: false));
		}
		base.Ports.Clear();
		base.Ports.AddRange(list);
	}

	private Port MakePort(string name, float x, float y, float dx, float dy, bool vents)
	{
		Port obj = GetPort(name) ?? new Port(this, name);
		obj.Local = new PointF(x, y);
		obj.Direction = new PointF(dx, dy);
		obj.VentsWhenOpen = vents;
		obj.Kind = Medium;
		return obj;
	}

	private void SetState(int value)
	{
		if (value != _state)
		{
			_state = value;
			RebuildPorts();
		}
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		float shift = Shift;
		WorkingPort[] array = WorkingPorts();
		WorkingPort[] array2 = array;
		checked
		{
			for (int i = 0; i < array2.Length; i++)
			{
				WorkingPort workingPort = array2[i];
				Port port = GetPort(workingPort.Name);
				float num = ActW + 1f * BoxWidth + workingPort.Offset;
				g.DrawLine(r.PenFor(port), num, (!workingPort.Top) ? 60 : 0, num, workingPort.Top ? 10f : 50f);
				if (workingPort.Vents && port.ConnectionCount == 0)
				{
					Symbols.Exhaust(g, r.Line, new PointF(num, 60f), new PointF(0f, 1f));
				}
			}
			int num2 = PositionCount - 1;
			for (int j = 0; j <= num2; j++)
			{
				int num3 = PositionAt(j);
				float num4 = ActW + shift + (float)j * BoxWidth;
				g.FillRectangle(r.BodyBrush, num4, 10f, BoxWidth, 40f);
				g.DrawRectangle(r.Line, num4, 10f, BoxWidth, 40f);
				bool flag = num3 == _state;
				string[][] array3 = Flows(num3);
				foreach (string[] array4 in array3)
				{
					WorkingPort wp = FindWorking(array, array4[0]);
					WorkingPort wp2 = FindWorking(array, array4[1]);
					Pen pen = (flag ? r.PenFor(GetPort(array4[0])) : r.Line);
					Symbols.Arrow(g, pen, BoxPoint(num4, wp, 2f), BoxPoint(num4, wp2, 2f));
				}
				string[] array5 = ClosedPorts(num3);
				foreach (string name in array5)
				{
					WorkingPort wp3 = FindWorking(array, name);
					Symbols.Blocked(g, r.Line, BoxPoint(num4, wp3, 0f), new PointF(0f, wp3.Top ? 1f : (-1f)));
				}
			}
			float num5 = ActW + shift;
			float num6 = ActW + (float)PositionCount * BoxWidth + shift;
			float num7 = 30f;
			if (PositionCount == 3)
			{
				Symbols.Spring(g, r.Line, num5 - 14f, num5, num7 + 10f, 5f);
				Symbols.Spring(g, r.Line, num6, num6 + 14f, num7 + 10f, 5f);
			}
			float num8 = ((PositionCount == 3) ? 14f : 0f);
			DrawActuator(g, r, LeftKind(), num5 - num8, -1, _state == 1, LeftPilotName, SolenoidLabel);
			string text = RightKindForDrawing();
			if (text != null)
			{
				DrawActuator(g, r, text, num6 + num8, 1, _state == ((PositionCount == 3) ? 2 : 0) && r.Simulating && RightActive, RightPilotName, ReturnSolenoidLabel);
			}
			if (PositionCount == 3)
			{
				g.DrawLine(r.Thin, num5 - 14f, num7, num5, num7);
				g.DrawLine(r.Thin, num6, num7, num6 + 14f, num7);
			}
		}
	}

	private string LeftKind()
	{
		return _actuator.ToString();
	}

	private string RightKindForDrawing()
	{
		if (_return == ValveReturn.Spring)
		{
			return (PositionCount == 3) ? null : "Spring";
		}
		return _return.ToString();
	}

	private static WorkingPort FindWorking(WorkingPort[] wps, string name)
	{
		return wps.First([SpecialName] (WorkingPort w) => Operators.CompareString(w.Name, name, TextCompare: false) == 0);
	}

	private static PointF BoxPoint(float boxX, WorkingPort wp, float inset)
	{
		return new PointF(boxX + wp.Offset, wp.Top ? (10f + inset) : (50f - inset));
	}

	private void DrawActuator(DrawSurface g, RenderContext r, string kind, float edge, int dir, bool operated, string pilotName, string solenoid)
	{
		_Closure_0024__104_002D0 arg = default(_Closure_0024__104_002D0);
		_Closure_0024__104_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__104_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_edge = edge;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_dir = dir;
		float num = 30f;
		VB_0024AnonymousDelegate_8<float, float> vB_0024AnonymousDelegate_ = [SpecialName] (float t) => CS_0024_003C_003E8__locals0._0024VB_0024Local_edge + (float)CS_0024_003C_003E8__locals0._0024VB_0024Local_dir * t;
		Pen pen = ((r.Simulating && operated) ? r.Pressure : r.Line);
		switch (kind)
		{
		case "Spring":
			Symbols.Spring(g, r.Line, CS_0024_003C_003E8__locals0._0024VB_0024Local_edge, vB_0024AnonymousDelegate_(18f), num, 7f);
			break;
		case "PushButton":
			g.DrawLine(r.Line, CS_0024_003C_003E8__locals0._0024VB_0024Local_edge, num, vB_0024AnonymousDelegate_(12f), num);
			g.DrawLine(pen, vB_0024AnonymousDelegate_(12f), num - 8f, vB_0024AnonymousDelegate_(12f), num + 8f);
			if (CS_0024_003C_003E8__locals0._0024VB_0024Local_dir < 0)
			{
				g.DrawArc(pen, vB_0024AnonymousDelegate_(18f), num - 8f, 12f, 16f, 90f, 180f);
			}
			else
			{
				g.DrawArc(pen, vB_0024AnonymousDelegate_(6f), num - 8f, 12f, 16f, 270f, 180f);
			}
			break;
		case "Selector":
		{
			g.DrawLine(r.Line, CS_0024_003C_003E8__locals0._0024VB_0024Local_edge, num, vB_0024AnonymousDelegate_(8f), num);
			g.DrawLine(pen, vB_0024AnonymousDelegate_(8f), num, vB_0024AnonymousDelegate_(18f), num - 12f);
			g.DrawLine(r.Thin, vB_0024AnonymousDelegate_(4f), num + 6f, vB_0024AnonymousDelegate_(16f), num + 6f);
			float[] array = new float[2] { 6f, 14f };
			foreach (float num4 in array)
			{
				g.DrawLine(r.Thin, vB_0024AnonymousDelegate_(num4), num + 6f, vB_0024AnonymousDelegate_(num4 + 2f), num + 3f);
				g.DrawLine(r.Thin, vB_0024AnonymousDelegate_(num4 + 2f), num + 3f, vB_0024AnonymousDelegate_(num4 + 4f), num + 6f);
			}
			break;
		}
		case "RollerLever":
		case "IdleReturnRoller":
			g.DrawLine(r.Line, CS_0024_003C_003E8__locals0._0024VB_0024Local_edge, num, vB_0024AnonymousDelegate_(10f), num);
			if (Operators.CompareString(kind, "IdleReturnRoller", TextCompare: false) == 0)
			{
				g.DrawLine(r.Line, vB_0024AnonymousDelegate_(10f), num, vB_0024AnonymousDelegate_(14f), num - 6f);
				g.FillEllipse(Brushes.Black, vB_0024AnonymousDelegate_(10f) - 1.5f, num - 1.5f, 3f, 3f);
				float num6 = vB_0024AnonymousDelegate_(14f);
				g.FillEllipse(r.BodyBrush, num6 - 4f, num - 14f, 9f, 9f);
				g.DrawEllipse(pen, num6 - 4f, num - 14f, 9f, 9f);
				if (!string.IsNullOrWhiteSpace(TriggerMark))
				{
					SizeF sizeF2 = g.MeasureString(TriggerMark, r.SmallFont);
					g.DrawString(TriggerMark, r.SmallFont, r.MarkBrush, num6 - sizeF2.Width / 2f, num - 16f - sizeF2.Height);
				}
			}
			else
			{
				float num7 = vB_0024AnonymousDelegate_(14f);
				g.FillEllipse(r.BodyBrush, num7 - 5f, num - 5f, 10f, 10f);
				g.DrawEllipse(pen, num7 - 5f, num - 5f, 10f, 10f);
				if (!string.IsNullOrWhiteSpace(TriggerMark))
				{
					SizeF sizeF3 = g.MeasureString(TriggerMark, r.SmallFont);
					g.DrawString(TriggerMark, r.SmallFont, r.MarkBrush, num7 - sizeF3.Width / 2f, num - 8f - sizeF3.Height);
				}
			}
			break;
		case "Pilot":
		case "DelayedPilot":
		{
			Port port = GetPort(pilotName);
			if (Math.Abs(_switchPressure - 1.5) > 0.01)
			{
				Symbols.Spring(g, r.Thin, vB_0024AnonymousDelegate_(18f), vB_0024AnonymousDelegate_(6f), num - 9f, 3f);
				g.DrawString(Units.Pressure(_switchPressure, "0.#"), r.SmallFont, r.TextBrush, Math.Min(vB_0024AnonymousDelegate_(22f), vB_0024AnonymousDelegate_(0f)), num - 26f);
			}
			g.DrawLine(r.DashedFor(port), vB_0024AnonymousDelegate_(20f), num, vB_0024AnonymousDelegate_(6f), num);
			Symbols.EnergyTriangle(g, r, new PointF[3]
			{
				new PointF(CS_0024_003C_003E8__locals0._0024VB_0024Local_edge, num),
				new PointF(vB_0024AnonymousDelegate_(7f), num - 4f),
				new PointF(vB_0024AnonymousDelegate_(7f), num + 4f)
			}, Medium == PortKind.Hydraulic, r.Simulating && port != null && port.IsPressurized);
			if (Operators.CompareString(kind, "DelayedPilot", TextCompare: false) == 0)
			{
				float num5 = vB_0024AnonymousDelegate_(11f);
				g.FillEllipse(r.BodyBrush, num5 - 6f, num - 22f, 12f, 12f);
				g.DrawEllipse(r.Thin, num5 - 6f, num - 22f, 12f, 12f);
				g.DrawLine(r.Thin, num5, num - 16f, num5, num - 20f);
				g.DrawLine(r.Thin, num5, num - 16f, num5 + 3f, num - 16f);
				string text = (r.Simulating ? $"{Math.Min(_timer, _delay):0.0}/{_delay:0.0} s" : $"t = {_delay:0.0#} s");
				g.DrawString(text, r.SmallFont, r.TextBrush, CS_0024_003C_003E8__locals0._0024VB_0024Local_edge - 4f, num - 34f);
			}
			break;
		}
		case "Solenoid":
		{
			float num2 = Math.Min(vB_0024AnonymousDelegate_(4f), vB_0024AnonymousDelegate_(14f));
			float num3 = Math.Max(vB_0024AnonymousDelegate_(4f), vB_0024AnonymousDelegate_(14f));
			using (SolidBrush brush = new SolidBrush((r.Simulating && operated) ? Color.FromArgb(255, 205, 205) : Color.White))
			{
				g.FillRectangle(brush, num2, num - 9f, num3 - num2, 18f);
			}
			g.DrawRectangle((r.Simulating && operated) ? r.Energized : r.Line, num2, num - 9f, num3 - num2, 18f);
			g.DrawLine(r.Line, num2, num + 9f, num3, num - 9f);
			g.DrawLine(r.Line, CS_0024_003C_003E8__locals0._0024VB_0024Local_edge, num, vB_0024AnonymousDelegate_(4f), num);
			if (!string.IsNullOrWhiteSpace(solenoid))
			{
				SizeF sizeF = g.MeasureString(solenoid, r.SmallFont);
				float x = ((CS_0024_003C_003E8__locals0._0024VB_0024Local_dir < 0) ? (CS_0024_003C_003E8__locals0._0024VB_0024Local_edge - 2f - sizeF.Width) : (CS_0024_003C_003E8__locals0._0024VB_0024Local_edge + 2f));
				g.DrawString(solenoid, r.SmallFont, r.TextBrush, x, num + 11f);
			}
			break;
		}
		}
	}

	public override void OnSimMouseDown(PointF local)
	{
		if (local.X > Shift + TotalWidth / 2f && _return == ValveReturn.Solenoid)
		{
			_manualRight = true;
			return;
		}
		if (_actuator == ValveActuator.PushButton || _actuator == ValveActuator.Solenoid)
		{
			_manualPressed = true;
		}
		if (_actuator == ValveActuator.Selector)
		{
			_detentOn = !_detentOn;
		}
	}

	public override void OnSimMouseUp()
	{
		_manualPressed = false;
		_manualRight = false;
	}

	public override void ResetSim()
	{
		SetState(0);
		base.ResetSim();
		_manualPressed = false;
		_manualRight = false;
		_detentOn = false;
		_rightActive = false;
		_timer = 0.0;
		_pulse = 0.0;
		_markBefore = true;
	}

	public override void AddEdges(Simulator sim)
	{
		string[][] array = Flows(_state);
		foreach (string[] array2 in array)
		{
			sim.AddEdge(GetPort(array2[0]), GetPort(array2[1]));
		}
	}

	private bool PilotOn(string name)
	{
		Port port = GetPort(name);
		if (port != null)
		{
			return port.Pressure >= _switchPressure - 1E-06;
		}
		return false;
	}

	public bool[] ActiveSignals(Simulator sim)
	{
		bool flag = default(bool);
		switch (_actuator)
		{
		case ValveActuator.RollerLever:
			flag = sim.IsMarkActive(TriggerMark);
			break;
		case ValveActuator.IdleReturnRoller:
			flag = _pulse > 0.0;
			break;
		case ValveActuator.Pilot:
		case ValveActuator.DelayedPilot:
			flag = PilotOn(LeftPilotName);
			break;
		case ValveActuator.Solenoid:
			flag = sim.IsSolenoidActive(SolenoidLabel);
			break;
		}
		bool flag2 = (_return == ValveReturn.Pilot && PilotOn(RightPilotName)) || (_return == ValveReturn.Solenoid && sim.IsSolenoidActive(ReturnSolenoidLabel));
		return new bool[2] { flag, flag2 };
	}

	public override bool UpdateLogic(Simulator sim)
	{
		bool flag = base.Fault != FaultKind.BurntCoil;
		bool flag2 = default(bool);
		switch (_actuator)
		{
		case ValveActuator.PushButton:
			flag2 = _manualPressed;
			break;
		case ValveActuator.Selector:
			flag2 = _detentOn;
			break;
		case ValveActuator.RollerLever:
			flag2 = sim.IsMarkActive(TriggerMark);
			break;
		case ValveActuator.IdleReturnRoller:
			flag2 = _pulse > 0.0;
			break;
		case ValveActuator.Pilot:
			flag2 = PilotOn(LeftPilotName);
			break;
		case ValveActuator.DelayedPilot:
			flag2 = PilotOn(LeftPilotName) && _timer >= _delay - 1E-06;
			break;
		case ValveActuator.Solenoid:
			flag2 = _manualPressed || (flag && sim.IsSolenoidActive(SolenoidLabel));
			break;
		}
		bool flag3 = default(bool);
		switch (_return)
		{
		case ValveReturn.Pilot:
			flag3 = PilotOn(RightPilotName);
			break;
		case ValveReturn.Solenoid:
			flag3 = _manualRight || (flag && sim.IsSolenoidActive(ReturnSolenoidLabel));
			break;
		}
		_rightActive = flag3;
		int num = _state;
		if (PositionCount == 3)
		{
			num = ((flag2 && !flag3) ? 1 : ((flag3 && !flag2) ? 2 : 0));
		}
		else if (_return == ValveReturn.Spring)
		{
			num = (flag2 ? 1 : 0);
		}
		else
		{
			if (flag2 && !flag3)
			{
				num = 1;
			}
			if (flag3 && !flag2)
			{
				num = 0;
			}
		}
		if (base.Fault == FaultKind.StuckNormal)
		{
			num = 0;
		}
		if (base.Fault == FaultKind.StuckOperated)
		{
			num = 1;
		}
		if (num == _state)
		{
			return false;
		}
		SetState(num);
		return true;
	}

	public override void UpdateDynamics(Simulator sim, double dt)
	{
		if (_actuator == ValveActuator.DelayedPilot)
		{
			if (PilotOn(LeftPilotName))
			{
				_timer += dt;
			}
			else
			{
				_timer = 0.0;
			}
		}
		if (_actuator == ValveActuator.IdleReturnRoller)
		{
			bool flag = sim.IsMarkActive(TriggerMark);
			if (flag && !_markBefore)
			{
				_pulse = 0.15;
			}
			else
			{
				_pulse = Math.Max(0.0, _pulse - dt);
			}
			_markBefore = flag;
		}
	}

	protected string ActuationText()
	{
		string text = _actuator switch
		{
			ValveActuator.PushButton => "push button", 
			ValveActuator.Selector => "selector switch", 
			ValveActuator.RollerLever => "roller lever", 
			ValveActuator.IdleReturnRoller => "idle-return roller", 
			ValveActuator.Pilot => "pneumatic pilot", 
			ValveActuator.Solenoid => "solenoid", 
			_ => "time delay", 
		};
		if (PositionCount == 3)
		{
			return text + ((_return == ValveReturn.Spring) ? ", spring centred" : $" / {_return.ToString().ToLowerInvariant()}, spring centred");
		}
		return _return switch
		{
			ValveReturn.Spring => text + ", spring return", 
			ValveReturn.Pilot => text + ", pilot return", 
			_ => text + ", solenoid return", 
		};
	}
}
