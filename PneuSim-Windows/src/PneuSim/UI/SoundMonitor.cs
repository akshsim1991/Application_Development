using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace PneuSim;

/// <summary>
/// Watches a running simulation and turns what happens into sounds and a rated-pressure warning.
/// Call Update after each simulation tick. It never changes the simulation.
/// </summary>
public sealed class SoundMonitor
{
	private readonly Dictionary<CircuitElement, double> _cylinderPos = new Dictionary<CircuitElement, double>();

	private readonly Dictionary<DirectionalValve, int> _valveState = new Dictionary<DirectionalValve, int>();

	private readonly Dictionary<ElectricContact, bool> _estop = new Dictionary<ElectricContact, bool>();

	private readonly Dictionary<CircuitElement, int> _overCount = new Dictionary<CircuitElement, int>();

	private readonly Stopwatch _clock = Stopwatch.StartNew();

	private long _alarmUntilMs;

	/// <summary>Elements currently above their rated pressure, with the pressure in bar.</summary>
	public List<KeyValuePair<CircuitElement, double>> OverRated { get; } = new List<KeyValuePair<CircuitElement, double>>();

	public void Update(Circuit circuit)
	{
		OverRated.Clear();
		bool flag = false;
		bool on = false;
		bool on2 = false;
		foreach (CircuitElement element in circuit.Elements)
		{
			if (element is DirectionalValve directionalValve)
			{
				if (_valveState.TryGetValue(directionalValve, out var value) && value != directionalValve.State)
				{
					SoundEngine.Play("valve");
					if (directionalValve.FluidKind() == PortKind.Pneumatic)
					{
						SoundEngine.Play("hiss");
					}
				}
				_valveState[directionalValve] = directionalValve.State;
			}
			else if (element is CylinderBase cylinderBase)
			{
				double position = cylinderBase.Position;
				if (_cylinderPos.TryGetValue(cylinderBase, out var value2) && ((value2 < 0.995 && position >= 0.995) || (value2 > 0.005 && position <= 0.005)))
				{
					SoundEngine.Play("thud");
				}
				_cylinderPos[cylinderBase] = position;
			}
			else if (element is Compressor compressor)
			{
				on |= compressor.Running;
			}
			else if (element is HydraulicPump hydraulicPump)
			{
				on |= hydraulicPump.Running;
			}
			else if (element is ReliefValve reliefValve)
			{
				on2 |= reliefValve.IsOpen;
			}
			else if (element is ElectricContact electricContact && electricContact.Operator == ContactOperator.EmergencyStop)
			{
				bool flag2 = electricContact.IsClosed != electricContact.NormallyClosed;
				if (_estop.TryGetValue(electricContact, out var value3) && flag2 && !value3)
				{
					_alarmUntilMs = _clock.ElapsedMilliseconds + 1500;
				}
				_estop[electricContact] = flag2;
			}
			double? num = element.OverRatedPressure();
			if (num.HasValue)
			{
				_overCount.TryGetValue(element, out var value4);
				_overCount[element] = ++value4;
				if (value4 >= 3)
				{
					OverRated.Add(new KeyValuePair<CircuitElement, double>(element, num.Value));
					flag = true;
				}
			}
			else
			{
				_overCount.Remove(element);
			}
		}
		SoundEngine.SetLoop("hum", on);
		SoundEngine.SetLoop("relief", on2);
		SoundEngine.SetLoop("alarm", flag || _clock.ElapsedMilliseconds < _alarmUntilMs);
	}

	/// <summary>Plain-English warning for the status bar, or null when nothing is over its rating.</summary>
	public string WarningText()
	{
		if (OverRated.Count == 0)
		{
			return null;
		}
		KeyValuePair<CircuitElement, double> keyValuePair = OverRated.OrderByDescending((KeyValuePair<CircuitElement, double> o) => o.Value).First();
		string text = (string.IsNullOrEmpty(keyValuePair.Key.Label) ? keyValuePair.Key.DisplayName : (keyValuePair.Key.DisplayName + " " + keyValuePair.Key.Label));
		return "⚠ Rated pressure exceeded: " + text + " is at " + Units.Pressure(keyValuePair.Value, "0.0") + " (rated " + Units.Pressure(keyValuePair.Key.EffectiveRatedPressure, "0.#") + ")" + ((OverRated.Count > 1) ? $" and {OverRated.Count - 1} more" : "");
	}

	public void Stop()
	{
		SoundEngine.Shutdown();
		OverRated.Clear();
	}
}
