using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;

namespace PneuSim;

/// <summary>
/// Automatic operation: when the simulation starts, PneuSim presses the start button(s) of the circuit for you, so you do not have to click them.
/// Once = one cycle. Continuous = the cycle is repeated with a rest in between.
/// Circuits with a memory (relays, double-solenoid or double-pilot valves) get a short pulse; circuits that need the button held
/// (direct control) are held until the cylinders have reached their end positions.
/// </summary>
public sealed class AutoOperator
{
	private enum Phase
	{
		Waiting,
		Pressing,
		Settling,
		Resting,
		Done
	}

	private readonly Circuit _circuit;

	private readonly string _mode;

	private readonly List<CircuitElement> _starts = new List<CircuitElement>();

	private readonly List<CircuitElement> _switches = new List<CircuitElement>();

	private readonly List<CylinderBase> _cylinders = new List<CylinderBase>();

	private readonly bool _pulse;

	private Phase _phase;

	private double _phaseStart;

	private double _lastMotion;

	private double _signature;

	private bool _moved;

	private readonly double _holdLimit;

	private bool _switchesDone;

	/// <summary>Latching start switches (selector switches or selector valves), used when the circuit has no push button.</summary>
	private readonly List<CircuitElement> _latch = new List<CircuitElement>();

	/// <summary>Text for the status bar describing what automatic operation is doing, or null.</summary>
	public string Status { get; private set; }

	/// <summary>True if the circuit has something that can be operated automatically.</summary>
	public bool HasWork => _starts.Count > 0 || _latch.Count > 0;

	public AutoOperator(Circuit circuit, string mode)
	{
		_circuit = circuit;
		_mode = mode;
		foreach (CircuitElement element in circuit.Elements)
		{
			if (element is ElectricContact electricContact)
			{
				if (electricContact.Operator == ContactOperator.PushButton && !electricContact.NormallyClosed)
				{
					_starts.Add(electricContact);
				}
				else if (electricContact.Operator == ContactOperator.Selector && !electricContact.NormallyClosed)
				{
					_switches.Add(electricContact);
				}
			}
			else if (element is DirectionalValve directionalValve && directionalValve.Actuator == ValveActuator.PushButton && !(directionalValve is Valve32 valve && valve.NormallyOpen))
			{
				_starts.Add(directionalValve);
			}
			else if (element is DirectionalValve directionalValve3 && directionalValve3.Actuator == ValveActuator.Selector)
			{
				_latch.Add(directionalValve3);
			}
			else if (element is CylinderBase cylinderBase && !(element is Gripper) && !(element is SemiRotaryActuator))
			{
				_cylinders.Add(cylinderBase);
			}
		}
		if (_starts.Count > 0)
		{
			_latch.Clear();
		}
		else if (_switches.Count > 0)
		{
			_latch.AddRange(_switches);
			_switches.Clear();
		}
		_pulse = circuit.Elements.Any((CircuitElement e) => (e is ElectricCoil electricCoil && electricCoil.Kind != CoilKind.Solenoid && electricCoil.Kind != CoilKind.Lamp) || (e is DirectionalValve directionalValve2 && (directionalValve2.Positions == 3 || directionalValve2.ReturnType != ValveReturn.Spring)));
		_holdLimit = ((AppSettings.AutoHoldSeconds > 0.0) ? AppSettings.AutoHoldSeconds : (1.4 * (_cylinders.Count == 0 ? 1.0 : _cylinders.Max((CylinderBase c) => c.StrokeTime)) * (_cylinders.Count + 1) + 1.0));
		_phase = Phase.Waiting;
		_phaseStart = 0.0;
		Status = (HasWork ? ("Automatic operation: the start will be operated in " + AppSettings.AutoStartDelay.ToString("0.#") + " s.") : null);
	}

	private double Signature()
	{
		double num = 0.0;
		foreach (CylinderBase cylinder in _cylinders)
		{
			num += cylinder.Position * 7.0 + Math.Abs(cylinder.Velocity);
		}
		foreach (CircuitElement element in _circuit.Elements)
		{
			if (element is ElectricAxisBase electricAxisBase)
			{
				num += electricAxisBase.Position * 0.01;
			}
			else if (element is DCMotor dCMotor)
			{
				num += Math.Abs(dCMotor.Rpm) * 0.001;
			}
		}
		return num;
	}

	private static void Press(CircuitElement e)
	{
		RectangleF localBounds = e.LocalBounds;
		e.OnSimMouseDown(new PointF(localBounds.Left + localBounds.Width / 2f, localBounds.Top + localBounds.Height / 2f));
	}

	private void PressAll()
	{
		foreach (CircuitElement start in _starts)
		{
			Press(start);
		}
	}

	private void ReleaseAll()
	{
		foreach (CircuitElement start in _starts)
		{
			start.OnSimMouseUp();
		}
	}

	/// <summary>Releases anything this operator is holding (call when the simulation stops).</summary>
	public void Stop()
	{
		ReleaseAll();
		_phase = Phase.Done;
	}

	/// <summary>Call once per simulation tick with the simulation time. Returns true if an input changed, so the logic must be run again.</summary>
	public bool Update(double time)
	{
		if (_mode == "Off" || !HasWork)
		{
			return false;
		}
		double num = Signature();
		bool flag = Math.Abs(num - _signature) > 0.0001;
		_signature = num;
		if (flag)
		{
			_lastMotion = time;
			_moved = true;
		}
		switch (_phase)
		{
		case Phase.Waiting:
			if (time - _phaseStart >= AppSettings.AutoStartDelay)
			{
				if (_starts.Count == 0)
				{
					foreach (CircuitElement item2 in _latch)
					{
						Press(item2);
					}
					_phase = Phase.Done;
					Status = "Automatic operation: start switch turned on.";
					return true;
				}
				if (_mode == "Continuous" && !_switchesDone)
				{
					foreach (CircuitElement item in _switches)
					{
						Press(item);
					}
					_switchesDone = true;
				}
				PressAll();
				_moved = false;
				_phase = Phase.Pressing;
				_phaseStart = time;
				Status = "Automatic operation: start button pressed.";
				return true;
			}
			break;
		case Phase.Pressing:
		{
			bool flag2 = false;
			double num2 = time - _phaseStart;
			if (_pulse)
			{
				flag2 = (_moved && num2 > 0.3) || num2 > 1.2;
			}
			else if (_cylinders.Count > 0)
			{
				bool flag3 = _cylinders.All((CylinderBase c) => c.Position > 0.97 || c.Position < 0.03 || c.Fault == FaultKind.Jammed) && _moved && _cylinders.Any((CylinderBase c) => c.Position > 0.97) && time - _lastMotion > 0.35;
				flag2 = flag3 || num2 > _holdLimit;
			}
			else
			{
				flag2 = num2 > 1.2;
			}
			if (flag2)
			{
				ReleaseAll();
				_phase = Phase.Settling;
				_phaseStart = time;
				_lastMotion = time;
				Status = "Automatic operation: start button released.";
				return true;
			}
			break;
		}
		case Phase.Settling:
			if (time - _lastMotion > 0.9 && time - _phaseStart > 0.6)
			{
				if (_mode == "Continuous")
				{
					_phase = Phase.Resting;
					_phaseStart = time;
					Status = "Automatic operation: next cycle in " + AppSettings.AutoCyclePause.ToString("0.#") + " s.";
				}
				else
				{
					_phase = Phase.Done;
					Status = "Automatic operation: cycle finished. Press a button yourself, or use Stop and Start for another run.";
				}
			}
			else if (time - _phaseStart > 60.0)
			{
				_phase = Phase.Done;
			}
			break;
		case Phase.Resting:
			if (time - _phaseStart >= AppSettings.AutoCyclePause)
			{
				PressAll();
				_moved = false;
				_phase = Phase.Pressing;
				_phaseStart = time;
				Status = "Automatic operation: new cycle.";
				return true;
			}
			break;
		}
		return false;
	}
}
