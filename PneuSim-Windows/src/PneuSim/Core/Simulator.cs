using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;

namespace PneuSim;

public class Simulator
{
	private struct Edge
	{
		public int A;

		public int B;

		public double AB;

		public double BA;

		public double LimitAB;
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__50_002D0
	{
		public int[] _0024VB_0024Local_parent;

		public double[] _0024VB_0024Local_fed;

		public bool[] _0024VB_0024Local_vents;

		public _Closure_0024__50_002D0(_Closure_0024__50_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_parent = arg0._0024VB_0024Local_parent;
				_0024VB_0024Local_fed = arg0._0024VB_0024Local_fed;
				_0024VB_0024Local_vents = arg0._0024VB_0024Local_vents;
			}
		}

		[SpecialName]
		internal int _Lambda_0024__0(int i)
		{
			while (_0024VB_0024Local_parent[i] != i)
			{
				_0024VB_0024Local_parent[i] = _0024VB_0024Local_parent[_0024VB_0024Local_parent[i]];
				i = _0024VB_0024Local_parent[i];
			}
			return i;
		}

		[SpecialName]
		internal bool _Lambda_0024__1(int i)
		{
			if (_0024VB_0024Local_fed[i] <= 0.0)
			{
				return !_0024VB_0024Local_vents[i];
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__52_002D0
	{
		public int[] _0024VB_0024Local_parent;

		public _Closure_0024__52_002D0(_Closure_0024__52_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_parent = arg0._0024VB_0024Local_parent;
			}
		}

		[SpecialName]
		internal int _Lambda_0024__0(int i)
		{
			while (_0024VB_0024Local_parent[i] != i)
			{
				_0024VB_0024Local_parent[i] = _0024VB_0024Local_parent[_0024VB_0024Local_parent[i]];
				i = _0024VB_0024Local_parent[i];
			}
			return i;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__76_002D0
	{
		public double _0024VB_0024Local_t;

		public Predicate<PointF> _0024I0;

		public _Closure_0024__76_002D0(_Closure_0024__76_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_t = arg0._0024VB_0024Local_t;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(PointF pt)
		{
			return (double)pt.X > _0024VB_0024Local_t + 1E-06;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__90_002D0
	{
		public int[] _0024VB_0024Local_parent;

		public _Closure_0024__90_002D0(_Closure_0024__90_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_parent = arg0._0024VB_0024Local_parent;
			}
		}

		[SpecialName]
		internal int _Lambda_0024__1(int i)
		{
			while (_0024VB_0024Local_parent[i] != i)
			{
				_0024VB_0024Local_parent[i] = _0024VB_0024Local_parent[_0024VB_0024Local_parent[i]];
				i = _0024VB_0024Local_parent[i];
			}
			return i;
		}
	}

	public const double PilotThreshold = 1.5;

	public const double ControlVoltage = 24.0;

	private const int MaxLogicPasses = 60;

	private const double HistoryInterval = 0.05;

	private const double HistoryLength = 60.0;

	private readonly Circuit _circuit;

	private List<Port> _ports;

	private readonly List<Edge> _edges;

	private readonly Dictionary<int, double> _supplies;

	private readonly HashSet<int> _exhausts;

	private readonly Dictionary<int, double> _exhaustCapacity;

	private double _nextSample;

	public const double LeakNlPerMin = 40.0;

	private Dictionary<Compressor, List<AirReceiver>> _compressorReceivers;

	private const double ChannelInterval = 0.02;

	private const double ChannelLength = 120.0;

	private double _nextChannelSample;

	private int _lastCycles;

	private double _lastCycleEnd;

	private readonly Dictionary<Accumulator, bool> _accumulatorFed;

	public Circuit Circuit => _circuit;

	public double Time { get; set; }

	public bool RealPhysics { get; set; }

	public double AirConsumed { get; set; }

	public List<string> Warnings { get; }

	public Dictionary<CircuitElement, List<PointF>> History { get; }

	public Dictionary<string, List<PointF>> Channels { get; }

	public double LastCycleTime { get; set; }

	public double HydraulicPumpFlow { get; set; }

	public double HydraulicFlowShare { get; set; }

	public Simulator(Circuit circuit)
	{
		_ports = new List<Port>();
		_edges = new List<Edge>();
		_supplies = new Dictionary<int, double>();
		_exhausts = new HashSet<int>();
		_exhaustCapacity = new Dictionary<int, double>();
		Warnings = new List<string>();
		History = new Dictionary<CircuitElement, List<PointF>>();
		Channels = new Dictionary<string, List<PointF>>();
		_accumulatorFed = new Dictionary<Accumulator, bool>();
		_circuit = circuit;
	}

	public void AddAirConsumption(double normalLitres)
	{
		if (normalLitres > 0.0)
		{
			AirConsumed += normalLitres;
		}
	}

	public int CompletedCycles()
	{
		List<CylinderBase> list = _circuit.Elements.OfType<CylinderBase>().ToList();
		if (list.Count != 0)
		{
			return list.Min([SpecialName] (CylinderBase c) => c.CompletedCycles);
		}
		return 0;
	}

	public void AddEdge(Port a, Port b, double capacityAB = 1.0, double capacityBA = 1.0, double pressureLimitAB = double.PositiveInfinity)
	{
		if (a != null && b != null)
		{
			_edges.Add(new Edge
			{
				A = a.NodeIndex,
				B = b.NodeIndex,
				AB = capacityAB,
				BA = capacityBA,
				LimitAB = pressureLimitAB
			});
		}
	}

	public void AddSupply(Port p, double pressure)
	{
		_supplies.TryGetValue(p.NodeIndex, out var value);
		_supplies[p.NodeIndex] = Math.Max(value, pressure);
	}

	public void AddExhaust(Port p, double capacity = 1.0)
	{
		_exhausts.Add(p.NodeIndex);
		if (capacity < 1.0)
		{
			_exhaustCapacity[p.NodeIndex] = Math.Max(0.001, capacity);
		}
	}

	public bool IsMarkActive(string mark)
	{
		if (string.IsNullOrWhiteSpace(mark))
		{
			return false;
		}
		foreach (ISignalSource item in _circuit.Elements.OfType<ISignalSource>())
		{
			if (item.IsSignalOn(mark.Trim()))
			{
				return true;
			}
		}
		return false;
	}

	public bool IsCoilActive(string label)
	{
		return IsActive(label, [SpecialName] (CoilKind k) => true);
	}

	public bool IsRelayActive(string label)
	{
		if (IsActive(label, [SpecialName] (CoilKind k) => k == CoilKind.Relay || k == CoilKind.OnDelayTimer || k == CoilKind.OffDelayTimer))
		{
			return true;
		}
		return !string.IsNullOrWhiteSpace(label) && _circuit.Elements.OfType<ElectricCounter>().Any([SpecialName] (ElectricCounter k) => k.Active && string.Equals(k.Label?.Trim(), label.Trim(), StringComparison.OrdinalIgnoreCase));
	}

	public bool IsSolenoidActive(string label)
	{
		return IsActive(label, [SpecialName] (CoilKind k) => k == CoilKind.Solenoid);
	}

	private bool IsActive(string label, Func<CoilKind, bool> kindOk)
	{
		if (string.IsNullOrWhiteSpace(label))
		{
			return false;
		}
		foreach (ElectricCoil item in _circuit.Elements.OfType<ElectricCoil>())
		{
			if (item.Active && kindOk(item.Kind) && string.Equals(item.Label?.Trim(), label.Trim(), StringComparison.OrdinalIgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	public void Reset()
	{
		_compressorReceivers = null;
		Time = 0.0;
		AirConsumed = 0.0;
		ResetChannels();
		_nextSample = 0.0;
		History.Clear();
		Warnings.Clear();
		foreach (CircuitElement element in _circuit.Elements)
		{
			element.ResetSim();
		}
		LinkCompressors();
		RunLogic();
		if (RealPhysics)
		{
			foreach (CylinderBase item in _circuit.Elements.OfType<CylinderBase>())
			{
				item.InitPhysics();
			}
			RunLogic();
		}
		RecordHistory();
		RecordChannels();
	}

	public void Step(double dt)
	{
		Time += dt;
		PrepareHydraulics(dt);
		double airConsumed = AirConsumed;
		foreach (CircuitElement element in _circuit.Elements)
		{
			element.UpdateDynamics(this, dt);
		}
		AddTubeLeaks(dt);
		UpdateAirGeneration(dt, AirConsumed - airConsumed);
		RunLogic();
		MeasureFlows();
		RecordHistory();
		RecordChannels();
	}

	private void AddTubeLeaks(double dt)
	{
		foreach (Tube tube in _circuit.Tubes)
		{
			if (tube.Fault == FaultKind.Leak && tube.A.Kind == PortKind.Pneumatic)
			{
				double num = Math.Max(tube.A.Pressure, tube.B.Pressure);
				if (num > 0.2)
				{
					AddAirConsumption(40.0 * (num + 1.013) / 7.013 * dt / 60.0);
				}
			}
		}
	}

	public void RunLogic()
	{
		int num = 1;
		while (true)
		{
			Solve();
			bool flag = false;
			foreach (CircuitElement element in _circuit.Elements)
			{
				if (element.UpdateLogic(this))
				{
					flag = true;
				}
			}
			if (flag)
			{
				num = checked(num + 1);
				if (num > 60)
				{
					Solve();
					Warnings.Add("The circuit does not settle: valves or relays keep switching (oscillation).");
					break;
				}
				continue;
			}
			break;
		}
	}

	private void RecordHistory()
	{
		bool flag = Time + 1E-06 >= _nextSample;
		if (flag)
		{
			_nextSample = Time + 0.05;
		}
		foreach (CircuitElement element in _circuit.Elements)
		{
			double num;
			if (element is CylinderBase)
			{
				num = ((CylinderBase)element).Position;
			}
			else
			{
				if (!(element is DirectionalValve))
				{
					continue;
				}
				num = ((DirectionalValve)element).DiagramValue;
			}
			List<PointF> value = null;
			if (!History.TryGetValue(element, out value))
			{
				value = new List<PointF>();
				History[element] = value;
			}
			bool flag2 = value.Count > 0 && element is DirectionalValve && value[checked(value.Count - 1)].Y != (float)num;
			if (flag || flag2)
			{
				value.Add(new PointF((float)Time, (float)num));
				if (value.Count > 0 && (double)value[0].X < Time - 60.0)
				{
					value.RemoveAt(0);
				}
			}
		}
	}

	public void Solve()
	{
		_Closure_0024__50_002D0 arg = default(_Closure_0024__50_002D0);
		_Closure_0024__50_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__50_002D0(arg);
		_ports = _circuit.AllPorts().ToList();
		checked
		{
			int num = _ports.Count - 1;
			for (int j = 0; j <= num; j++)
			{
				_ports[j].NodeIndex = j;
			}
			_edges.Clear();
			_supplies.Clear();
			_exhausts.Clear();
			_exhaustCapacity.Clear();
			Warnings.Clear();
			foreach (CircuitElement element in _circuit.Elements)
			{
				element.AddEdges(this);
				element.AddTerminals(this);
			}
			AddTubeEdges();
			int count = _ports.Count;
			bool[] array = new bool[count - 1 + 1];
			int num2 = count - 1;
			for (int k = 0; k <= num2; k++)
			{
				Port port = _ports[k];
				array[k] = _exhausts.Contains(k) || (port.VentsWhenOpen && port.ConnectionCount == 0);
			}
			List<int[]>[] array2 = new List<int[]>[count - 1 + 1];
			List<int>[] array3 = new List<int>[count - 1 + 1];
			int num3 = count - 1;
			for (int l = 0; l <= num3; l++)
			{
				array2[l] = new List<int[]>();
				array3[l] = new List<int>();
			}
			int num4 = _edges.Count - 1;
			for (int m = 0; m <= num4; m++)
			{
				Edge edge = _edges[m];
				if (edge.AB > 0.0)
				{
					array2[edge.A].Add(new int[3] { edge.B, m, 0 });
					array3[edge.B].Add(edge.A);
				}
				if (edge.BA > 0.0)
				{
					array2[edge.B].Add(new int[3] { edge.A, m, 1 });
					array3[edge.A].Add(edge.B);
				}
			}
			CS_0024_003C_003E8__locals0._0024VB_0024Local_fed = new double[count - 1 + 1];
			Queue<int> queue = new Queue<int>();
			foreach (KeyValuePair<int, double> supply in _supplies)
			{
				if (supply.Value > CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[supply.Key])
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[supply.Key] = supply.Value;
				}
				queue.Enqueue(supply.Key);
			}
			while (queue.Count > 0)
			{
				int num5 = queue.Dequeue();
				foreach (int[] item in array2[num5])
				{
					double val = ((item[2] == 0) ? _edges[item[1]].LimitAB : double.PositiveInfinity);
					double num6 = Math.Min(CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[num5], val);
					if (num6 > CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[item[0]] + 1E-06)
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[item[0]] = num6;
						queue.Enqueue(item[0]);
					}
				}
			}
			CS_0024_003C_003E8__locals0._0024VB_0024Local_vents = new bool[count - 1 + 1];
			int num7 = count - 1;
			for (int n = 0; n <= num7; n++)
			{
				if (array[n] && CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[n] <= 0.0)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_vents[n] = true;
					queue.Enqueue(n);
				}
			}
			while (queue.Count > 0)
			{
				int num8 = queue.Dequeue();
				foreach (int item2 in array3[num8])
				{
					if (!CS_0024_003C_003E8__locals0._0024VB_0024Local_vents[item2] && CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[item2] <= 0.0)
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_vents[item2] = true;
						queue.Enqueue(item2);
					}
				}
			}
			CS_0024_003C_003E8__locals0._0024VB_0024Local_parent = new int[count - 1 + 1];
			int num9 = count - 1;
			for (int num10 = 0; num10 <= num9; num10++)
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[num10] = num10;
			}
			Func<int, int> func = [SpecialName] (int i) =>
			{
				while (CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i] != i)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i] = CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i]];
					i = CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i];
				}
				return i;
			};
			VB_0024AnonymousDelegate_1<int, bool> vB_0024AnonymousDelegate_ = [SpecialName] (int i) => CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[i] <= 0.0 && !CS_0024_003C_003E8__locals0._0024VB_0024Local_vents[i];
			foreach (Edge edge2 in _edges)
			{
				if ((edge2.AB > 0.0 || edge2.BA > 0.0) && vB_0024AnonymousDelegate_(edge2.A) && vB_0024AnonymousDelegate_(edge2.B))
				{
					int num11 = func(edge2.A);
					int num12 = func(edge2.B);
					if (num11 != num12)
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[num11] = num12;
					}
				}
			}
			double[] array4 = new double[count - 1 + 1];
			int num13 = count - 1;
			for (int num14 = 0; num14 <= num13; num14++)
			{
				if (vB_0024AnonymousDelegate_(num14) && _ports[num14].Kind == PortKind.Pneumatic)
				{
					int num15 = func(num14);
					array4[num15] = Math.Max(array4[num15], _ports[num14].Pressure);
				}
			}
			HashSet<CircuitElement> hashSet = new HashSet<CircuitElement>();
			bool flag = false;
			int num16 = count - 1;
			for (int num17 = 0; num17 <= num16; num17++)
			{
				Port port2 = _ports[num17];
				port2.Factor = 0.0;
				port2.SupplyPressure = Math.Max(0.0, CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[num17]);
				if (CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[num17] > 0.0)
				{
					port2.State = PortState.Pressurized;
					port2.Pressure = CS_0024_003C_003E8__locals0._0024VB_0024Local_fed[num17];
					if (array[num17])
					{
						if (port2.Kind == PortKind.Electric)
						{
							flag = true;
						}
						else if (port2.Kind == PortKind.Pneumatic)
						{
							hashSet.Add(port2.Owner);
						}
					}
				}
				else if (CS_0024_003C_003E8__locals0._0024VB_0024Local_vents[num17])
				{
					port2.State = PortState.Exhausted;
					port2.Pressure = 0.0;
				}
				else
				{
					port2.State = PortState.Floating;
					port2.Pressure = ((port2.Kind == PortKind.Pneumatic) ? array4[func(num17)] : 0.0);
				}
			}
			if (flag)
			{
				Warnings.Add("Short circuit: +24 V is connected directly to 0 V.");
			}
			foreach (CircuitElement item3 in hashSet)
			{
				Warnings.Add($"Compressed air escapes from an open port of {item3}.");
			}
			ComputeFlowFactors(array);
			if (RealPhysics)
			{
				ApplyChamberPressures();
			}
			AfterSolve();
		}
	}

	private void AddTubeEdges()
	{
		double num = 0.0;
		foreach (KeyValuePair<int, double> supply in _supplies)
		{
			if (_ports[supply.Key].Kind != PortKind.Electric)
			{
				num = Math.Max(num, supply.Value);
			}
		}
		foreach (Tube tube in _circuit.Tubes)
		{
			switch (tube.Fault)
			{
			case FaultKind.Leak:
			{
				double pressureLimitAB = Math.Max(0.3, num * 0.65);
				AddEdge(tube.A, tube.B, 0.35, 0.0, pressureLimitAB);
				AddEdge(tube.B, tube.A, 0.35, 0.0, pressureLimitAB);
				break;
			}
			default:
				AddEdge(tube.A, tube.B);
				break;
			case FaultKind.Blocked:
				break;
			}
		}
	}

	private void ApplyChamberPressures()
	{
		_Closure_0024__52_002D0 arg = default(_Closure_0024__52_002D0);
		_Closure_0024__52_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__52_002D0(arg);
		int count = _ports.Count;
		checked
		{
			CS_0024_003C_003E8__locals0._0024VB_0024Local_parent = new int[count - 1 + 1];
			int num = count - 1;
			for (int j = 0; j <= num; j++)
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[j] = j;
			}
			Func<int, int> func = [SpecialName] (int i) =>
			{
				while (CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i] != i)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i] = CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i]];
					i = CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i];
				}
				return i;
			};
			foreach (Tube tube in _circuit.Tubes)
			{
				if (tube.A.Kind == PortKind.Pneumatic)
				{
					int num2 = func(tube.A.NodeIndex);
					int num3 = func(tube.B.NodeIndex);
					if (num2 != num3)
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[num2] = num3;
					}
				}
			}
			Dictionary<int, double> dictionary = new Dictionary<int, double>();
			HashSet<int> hashSet = new HashSet<int>();
			foreach (KeyValuePair<int, double> supply in _supplies)
			{
				hashSet.Add(func(supply.Key));
			}
			foreach (CylinderBase item in _circuit.Elements.OfType<CylinderBase>())
			{
				foreach (Port port in item.Ports)
				{
					if (port.Kind != 0)
					{
						continue;
					}
					double num4 = item.ChamberPressureAt(port);
					if (!(num4 < 0.0))
					{
						int num5 = func(port.NodeIndex);
						if (!hashSet.Contains(num5))
						{
							dictionary[num5] = (dictionary.TryGetValue(num5, out var value) ? Math.Max(value, num4) : num4);
						}
					}
				}
			}
			if (dictionary.Count == 0)
			{
				return;
			}
			int num6 = count - 1;
			for (int k = 0; k <= num6; k++)
			{
				if (dictionary.TryGetValue(func(k), out var value2) && _ports[k].State != PortState.Exhausted)
				{
					_ports[k].Pressure = value2;
				}
			}
		}
	}

	private void ComputeFlowFactors(bool[] isExhaust)
	{
		int count = _ports.Count;
		checked
		{
			double[] array = new double[count - 1 + 1];
			int num = count - 1;
			for (int i = 0; i <= num; i++)
			{
				Port port = _ports[i];
				if (port.State == PortState.Pressurized && _supplies.ContainsKey(i))
				{
					array[i] = 1.0;
				}
				if (port.State == PortState.Exhausted && isExhaust[i])
				{
					array[i] = (_exhaustCapacity.TryGetValue(i, out var value) ? value : 1.0);
				}
			}
			bool flag = true;
			int num2 = 0;
			while (flag && num2 <= count + 1)
			{
				flag = false;
				num2++;
				foreach (Edge edge in _edges)
				{
					PortState state = _ports[edge.A].State;
					PortState state2 = _ports[edge.B].State;
					if (state == state2 && state != 0)
					{
						double val;
						double val2;
						if (state == PortState.Pressurized)
						{
							val = edge.AB;
							val2 = edge.BA;
						}
						else
						{
							val = edge.BA;
							val2 = edge.AB;
						}
						double num3 = Math.Min(array[edge.A], val);
						if (num3 > array[edge.B] + 1E-06)
						{
							array[edge.B] = num3;
							flag = true;
						}
						double num4 = Math.Min(array[edge.B], val2);
						if (num4 > array[edge.A] + 1E-06)
						{
							array[edge.A] = num4;
							flag = true;
						}
					}
				}
			}
			int num5 = count - 1;
			for (int j = 0; j <= num5; j++)
			{
				_ports[j].Factor = array[j];
			}
		}
	}

	private void LinkCompressors()
	{
		_compressorReceivers = new Dictionary<Compressor, List<AirReceiver>>();
		Dictionary<Port, List<Port>> dictionary = new Dictionary<Port, List<Port>>();
		foreach (Tube tube in _circuit.Tubes)
		{
			if (!dictionary.ContainsKey(tube.A))
			{
				dictionary[tube.A] = new List<Port>();
			}
			if (!dictionary.ContainsKey(tube.B))
			{
				dictionary[tube.B] = new List<Port>();
			}
			dictionary[tube.A].Add(tube.B);
			dictionary[tube.B].Add(tube.A);
		}
		foreach (Compressor item in _circuit.Elements.OfType<Compressor>())
		{
			List<AirReceiver> list = new List<AirReceiver>();
			HashSet<Port> hashSet = new HashSet<Port> { item.Ports[0] };
			Queue<Port> queue = new Queue<Port>();
			queue.Enqueue(item.Ports[0]);
			while (queue.Count > 0)
			{
				Port port = queue.Dequeue();
				List<Port> list2 = new List<Port>();
				List<Port> value = null;
				if (dictionary.TryGetValue(port, out value))
				{
					list2.AddRange(value);
				}
				CircuitElement owner = port.Owner;
				if (owner is AirReceiver || owner is FlowMeter || owner is ShutOffValve || owner is CheckValve)
				{
					list2.AddRange(owner.Ports);
				}
				if (owner is AirReceiver && !list.Contains((AirReceiver)owner))
				{
					list.Add((AirReceiver)owner);
				}
				foreach (Port item2 in list2)
				{
					if (hashSet.Add(item2))
					{
						queue.Enqueue(item2);
					}
				}
			}
			_compressorReceivers[item] = list;
			item.ChargesReceiver = list.Count > 0;
		}
	}

	private void UpdateAirGeneration(double dt, double consumed)
	{
		List<AirReceiver> list = _circuit.Elements.OfType<AirReceiver>().ToList();
		if (list.Count == 0)
		{
			return;
		}
		if (_compressorReceivers == null)
		{
			LinkCompressors();
		}
		List<AirReceiver> list2 = list.Where([SpecialName] (AirReceiver r) => r.Pressure > 0.05).ToList();
		checked
		{
			int num = _circuit.Elements.OfType<AirSupply>().Count() + _circuit.Elements.OfType<Compressor>().Count([SpecialName] (Compressor c) => !c.ChargesReceiver);
			if (consumed > 0.0 && list2.Count > 0)
			{
				double num2 = consumed / (double)(list2.Count + num);
				foreach (AirReceiver item in list2)
				{
					item.Exchange(0.0 - num2);
				}
			}
			foreach (AirReceiver item2 in list.Where([SpecialName] (AirReceiver x) => x.Fault == FaultKind.Leak && x.Pressure > 0.0))
			{
				item2.Exchange(-60.0 * (item2.Pressure + 1.013) / 7.013 * dt / 60.0);
			}
			foreach (KeyValuePair<Compressor, List<AirReceiver>> compressorReceiver in _compressorReceivers)
			{
				if (compressorReceiver.Value.Count == 0)
				{
					continue;
				}
				compressorReceiver.Key.UpdatePressureSwitch(compressorReceiver.Value.Min([SpecialName] (AirReceiver r) => r.Pressure), compressorReceiver.Value.Max([SpecialName] (AirReceiver r) => r.Pressure));
				if (!compressorReceiver.Key.Running)
				{
					continue;
				}
				double normalLitres = compressorReceiver.Key.EffectiveDelivery / 60.0 * dt / (double)compressorReceiver.Value.Count;
				foreach (AirReceiver item3 in compressorReceiver.Value)
				{
					item3.Exchange(normalLitres);
				}
			}
		}
	}

	private void ApplyVacuum()
	{
		List<VacuumGenerator> list = (from v in _circuit.Elements.OfType<VacuumGenerator>()
			where v.Working
			select v).ToList();
		if (list.Count == 0)
		{
			return;
		}
		List<int>[] array = BuildAdjacency(null);
		foreach (VacuumGenerator item in list)
		{
			int nodeIndex = item.Ports[1].NodeIndex;
			HashSet<int> hashSet = new HashSet<int> { nodeIndex };
			Queue<int> queue = new Queue<int>();
			queue.Enqueue(nodeIndex);
			bool flag = false;
			while (queue.Count > 0)
			{
				int num = queue.Dequeue();
				Port port = _ports[num];
				if (port.State == PortState.Exhausted || _exhausts.Contains(num) || (port.VentsWhenOpen && port.ConnectionCount == 0))
				{
					flag = true;
				}
				if (port.Owner is SuctionCup { Sealed: false })
				{
					flag = true;
				}
				foreach (int item2 in array[num])
				{
					if (_ports[item2].State != PortState.Pressurized && hashSet.Add(item2))
					{
						queue.Enqueue(item2);
					}
				}
			}
			double num2 = (flag ? (-0.05) : item.VacuumLevel);
			foreach (int item3 in hashSet)
			{
				if (_ports[item3].Pressure > num2)
				{
					_ports[item3].Pressure = num2;
				}
			}
		}
	}

	private List<int>[] BuildAdjacency(FlowMeter skipMeter)
	{
		int count = _ports.Count;
		checked
		{
			List<int>[] array = new List<int>[count - 1 + 1];
			int num = count - 1;
			for (int i = 0; i <= num; i++)
			{
				array[i] = new List<int>();
			}
			foreach (Edge edge in _edges)
			{
				if ((!(edge.AB <= 0.0) || !(edge.BA <= 0.0)) && (skipMeter == null || _ports[edge.A].Owner != skipMeter || _ports[edge.B].Owner != skipMeter))
				{
					array[edge.A].Add(edge.B);
					array[edge.B].Add(edge.A);
				}
			}
			return array;
		}
	}

	private void MeasureFlows()
	{
		foreach (ForceSensor item in _circuit.Elements.OfType<ForceSensor>())
		{
			item.Force = item.FindCylinder(_circuit.Elements)?.PistonForce() ?? 0.0;
		}
		foreach (FlowMeter item2 in _circuit.Elements.OfType<FlowMeter>())
		{
			List<int>[] adjacency = BuildAdjacency(item2);
			double num = SideFlow(adjacency, item2.Ports[1]);
			item2.Flow = ((Math.Abs(num) > 1E-06) ? num : (0.0 - SideFlow(adjacency, item2.Ports[0])));
		}
	}

	private double SideFlow(List<int>[] adjacency, Port start)
	{
		HashSet<int> hashSet = new HashSet<int> { start.NodeIndex };
		Queue<int> queue = new Queue<int>();
		queue.Enqueue(start.NodeIndex);
		while (queue.Count > 0)
		{
			int num = queue.Dequeue();
			foreach (int item in adjacency[num])
			{
				if (hashSet.Add(item))
				{
					queue.Enqueue(item);
				}
			}
		}
		double num2 = 0.0;
		foreach (int item2 in hashSet)
		{
			Port port = _ports[item2];
			bool flag = true;
			if (flag == port.Owner is CylinderBase)
			{
				num2 += ((CylinderBase)port.Owner).PortFlow(port);
			}
			else if (flag == port.Owner is AirMotor)
			{
				num2 += ((AirMotor)port.Owner).ConsumptionNow();
			}
			else if (flag == (port.Owner is VacuumGenerator && port == port.Owner.Ports[0]))
			{
				num2 += ((VacuumGenerator)port.Owner).ConsumptionNow();
			}
		}
		foreach (Tube tube in _circuit.Tubes)
		{
			if (tube.Fault == FaultKind.Leak && tube.A.Kind == PortKind.Pneumatic && (hashSet.Contains(tube.A.NodeIndex) || hashSet.Contains(tube.B.NodeIndex)))
			{
				double num3 = Math.Max(tube.A.Pressure, tube.B.Pressure);
				if (num3 > 0.2)
				{
					num2 += 40.0 * (num3 + 1.013) / 7.013;
				}
			}
		}
		return num2;
	}

	private static string NameOf_(CircuitElement e, string fallback, int index)
	{
		if (!string.IsNullOrWhiteSpace(e.Label))
		{
			return e.Label;
		}
		return $"{fallback} {index}";
	}

	public List<(string Name, double Value)> ChannelValues()
	{
		List<(string, double)> list = new List<(string, double)>();
		int num = 0;
		checked
		{
			foreach (CylinderBase item in _circuit.Elements.OfType<CylinderBase>())
			{
				num++;
				string arg = NameOf_(item, "cylinder", num);
				list.Add(($"{arg} position (mm)", item.Position * (double)item.StrokeLength));
				list.Add(($"{arg} speed (m/s)", item.Velocity));
				list.Add(($"{arg} pressure, cap side (bar)", RealPhysics ? item.CapPressure : item.Ports[0].Pressure));
				if (item.Ports.Count > 1)
				{
					list.Add(($"{arg} pressure, rod side (bar)", RealPhysics ? item.RodPressure : item.Ports[1].Pressure));
				}
			}
			num = 0;
			foreach (PressureGauge item2 in _circuit.Elements.OfType<PressureGauge>())
			{
				num++;
				list.Add((string.Format("{0} pressure (bar)", NameOf_(item2, "gauge", num)), item2.Ports[0].Pressure));
			}
			foreach (DirectionalValve item3 in from x in _circuit.Elements.OfType<DirectionalValve>()
				where !string.IsNullOrWhiteSpace(x.Label)
				select x)
			{
				list.Add(($"{item3.Label} valve position", item3.DiagramValue));
			}
			foreach (ElectricCoil item4 in from x in _circuit.Elements.OfType<ElectricCoil>()
				where !string.IsNullOrWhiteSpace(x.Label)
				select x)
			{
				list.Add((string.Format("{0} {1} (on = 1)", item4.Label, (item4.Kind == CoilKind.Lamp) ? "lamp" : "coil"), item4.Active ? 1 : 0));
			}
			num = 0;
			foreach (HydraulicMotor item5 in _circuit.Elements.OfType<HydraulicMotor>())
			{
				num++;
				list.Add((string.Format("{0} speed (rpm)", NameOf_(item5, "motor", num)), item5.Rpm));
			}
			foreach (Accumulator item6 in _circuit.Elements.OfType<Accumulator>())
			{
				list.Add((string.Format("{0} oil volume (l)", NameOf_(item6, "accumulator", 1)), item6.StoredLitres));
			}
			num = 0;
			foreach (FlowMeter item7 in _circuit.Elements.OfType<FlowMeter>())
			{
				num++;
				list.Add((string.Format("{0} flow ({1})", NameOf_(item7, "flow meter", num), item7.Unit), item7.Flow));
			}
			num = 0;
			foreach (ForceSensor item8 in _circuit.Elements.OfType<ForceSensor>())
			{
				num++;
				list.Add((string.Format("{0} force on {1} (N)", NameOf_(item8, "force sensor", num), item8.Cylinder), item8.Force));
			}
			num = 0;
			foreach (AirReceiver item9 in _circuit.Elements.OfType<AirReceiver>())
			{
				num++;
				list.Add((string.Format("{0} pressure (bar)", NameOf_(item9, "receiver", num)), item9.Pressure));
			}
			foreach (PressureSwitch item10 in from x in _circuit.Elements.OfType<PressureSwitch>()
				where !string.IsNullOrWhiteSpace(x.Label)
				select x)
			{
				list.Add((string.Format("{0} {1} switch (on = 1)", item10.Label, (item10.Setting < 0.0) ? "vacuum" : "pressure"), item10.IsOn ? 1 : 0));
			}
			num = 0;
			foreach (SuctionCup item11 in _circuit.Elements.OfType<SuctionCup>())
			{
				num++;
				list.Add((string.Format("{0} vacuum (bar)", NameOf_(item11, "suction cup", num)), item11.Ports[0].Pressure));
			}
			foreach (ElectricCounter item12 in from x in _circuit.Elements.OfType<ElectricCounter>()
				where !string.IsNullOrWhiteSpace(x.Label)
				select x)
			{
				list.Add(($"{item12.Label} count", item12.Count));
			}
			list.Add(("Air consumption (normal litres)", AirConsumed));
			return list;
		}
	}

	public string DiscreteState()
	{
		StringBuilder stringBuilder = new StringBuilder();
		foreach (CircuitElement element in _circuit.Elements)
		{
			bool flag = true;
			if (flag == element is DirectionalValve)
			{
				stringBuilder.Append(((DirectionalValve)element).State);
			}
			else if (flag == element is ElectricCoil)
			{
				stringBuilder.Append(((ElectricCoil)element).Active ? "1" : "0");
			}
			else if (flag == element is ElectricContact)
			{
				stringBuilder.Append(((ElectricContact)element).IsClosed ? "1" : "0");
			}
			else if (flag == element is ElectricCounter)
			{
				stringBuilder.Append(((ElectricCounter)element).Count);
			}
			else if (flag == element is PressureSwitch)
			{
				stringBuilder.Append(((PressureSwitch)element).IsOn ? "1" : "0");
			}
			else
			{
				if (flag != element is CylinderBase)
				{
					continue;
				}
				double position = ((CylinderBase)element).Position;
				stringBuilder.Append((position <= 0.005) ? "-" : ((position >= 0.995) ? "+" : "~"));
			}
			stringBuilder.Append(',');
		}
		return stringBuilder.ToString();
	}

	public void TrimRecordings(double t)
	{
		_Closure_0024__76_002D0 arg = default(_Closure_0024__76_002D0);
		_Closure_0024__76_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__76_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_t = t;
		foreach (List<PointF> item in Channels.Values.Concat(History.Values))
		{
			item.RemoveAll([SpecialName] (PointF pt) => (double)pt.X > CS_0024_003C_003E8__locals0._0024VB_0024Local_t + 1E-06);
		}
		_nextChannelSample = CS_0024_003C_003E8__locals0._0024VB_0024Local_t;
		_nextSample = CS_0024_003C_003E8__locals0._0024VB_0024Local_t;
	}

	private void ResetChannels()
	{
		Channels.Clear();
		_nextChannelSample = 0.0;
		_lastCycles = 0;
		_lastCycleEnd = 0.0;
		LastCycleTime = 0.0;
	}

	private void RecordChannels()
	{
		int num = CompletedCycles();
		if (num > _lastCycles)
		{
			LastCycleTime = (Time - _lastCycleEnd) / (double)checked(num - _lastCycles);
			_lastCycles = num;
			_lastCycleEnd = Time;
		}
		if (Time + 1E-06 < _nextChannelSample)
		{
			return;
		}
		_nextChannelSample = Time + 0.02;
		foreach (var item in ChannelValues())
		{
			List<PointF> value = null;
			if (!Channels.TryGetValue(item.Name, out value))
			{
				value = new List<PointF>();
				Channels[item.Name] = value;
			}
			value.Add(new PointF((float)Time, (float)item.Value));
			if ((double)value[0].X < Time - 120.0)
			{
				value.RemoveAt(0);
			}
		}
	}

	public string CostSummary(ProjectInfo info)
	{
		int num = CompletedCycles();
		if (AirConsumed <= 0.0001)
		{
			return "";
		}
		double num2 = ((num > 0) ? (AirConsumed / (double)num) : AirConsumed);
		double num3 = num2 * info.CyclesPerMinute * 60.0 * info.HoursPerDay * info.DaysPerYear / 1000.0;
		double num4 = num3 * info.AirCostPerM3;
		return string.Format("Air: {0:0.00} NL/{1}", num2, (num > 0) ? "cycle" : "so far") + ((LastCycleTime > 0.0) ? $"   cycle time {LastCycleTime:0.00} s" : "") + $"   ≈ {num3:0} m³/year → ₹{num4:#,##0}/year at {info.CyclesPerMinute:0.#} cycles/min";
	}

	private void PrepareHydraulics(double dt)
	{
		List<HydraulicPump> source = (from p in _circuit.Elements.OfType<HydraulicPump>()
			where p.Running
			select p).ToList();
		HydraulicPumpFlow = source.Sum([SpecialName] (HydraulicPump p) => p.EffectiveFlowLpm);
		int num = _circuit.Elements.OfType<IHydraulicConsumer>().Count([SpecialName] (IHydraulicConsumer c) => c.WantsFlow());
		double num2 = 0.0;
		foreach (Accumulator item in _circuit.Elements.OfType<Accumulator>())
		{
			_accumulatorFed.TryGetValue(item, out var value);
			if (!value && item.StoredLitres > 0.001)
			{
				num2 += 20.0;
			}
		}
		double num3 = HydraulicPumpFlow + num2;
		HydraulicFlowShare = num3 / (double)Math.Max(1, num);
		foreach (Accumulator item2 in _circuit.Elements.OfType<Accumulator>())
		{
			_accumulatorFed.TryGetValue(item2, out var value2);
			double flowOutLpm = ((value2 || num == 0) ? 0.0 : Math.Min(20.0, HydraulicFlowShare * (double)num));
			item2.Exchange(item2.Ports[0].Pressure, value2, flowOutLpm, dt);
		}
	}

	private void AfterSolve()
	{
		_Closure_0024__90_002D0 arg = default(_Closure_0024__90_002D0);
		_Closure_0024__90_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__90_002D0(arg);
		ApplyVacuum();
		int count = _ports.Count;
		if (!_ports.Any([SpecialName] (Port p) => p.Kind == PortKind.Hydraulic))
		{
			return;
		}
		checked
		{
			CS_0024_003C_003E8__locals0._0024VB_0024Local_parent = new int[count - 1 + 1];
			int num = count - 1;
			for (int j = 0; j <= num; j++)
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[j] = j;
			}
			Func<int, int> func = [SpecialName] (int i) =>
			{
				while (CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i] != i)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i] = CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i]];
					i = CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[i];
				}
				return i;
			};
			VB_0024AnonymousDelegate_1<int, bool> vB_0024AnonymousDelegate_ = [SpecialName] (int i) => _ports[i].Kind == PortKind.Hydraulic && _ports[i].State == PortState.Pressurized;
			foreach (Edge edge in _edges)
			{
				if (double.IsPositiveInfinity(edge.LimitAB) && (edge.AB > 0.0 || edge.BA > 0.0) && vB_0024AnonymousDelegate_(edge.A) && vB_0024AnonymousDelegate_(edge.B))
				{
					int num2 = func(edge.A);
					int num3 = func(edge.B);
					if (num2 != num3)
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_parent[num2] = num3;
					}
				}
			}
			Dictionary<int, double> dictionary = new Dictionary<int, double>();
			foreach (HydraulicPump item in from p in _circuit.Elements.OfType<HydraulicPump>()
				where p.Running
				select p)
			{
				int key = func(item.Ports[0].NodeIndex);
				dictionary[key] = Math.Max(dictionary.ContainsKey(key) ? dictionary[key] : 0.0, item.MaxPressureBar);
			}
			Dictionary<int, double> dictionary2 = new Dictionary<int, double>();
			foreach (ReliefValve item2 in _circuit.Elements.OfType<ReliefValve>())
			{
				if (vB_0024AnonymousDelegate_(item2.Ports[0].NodeIndex))
				{
					int key2 = func(item2.Ports[0].NodeIndex);
					dictionary2[key2] = Math.Min(dictionary2.ContainsKey(key2) ? dictionary2[key2] : double.MaxValue, item2.EffectiveSetting);
				}
			}
			HashSet<int> hashSet = new HashSet<int>();
			int num4 = count - 1;
			for (int k = 0; k <= num4; k++)
			{
				if (vB_0024AnonymousDelegate_(k) && _exhausts.Contains(k))
				{
					hashSet.Add(func(k));
				}
			}
			Dictionary<int, double> dictionary3 = new Dictionary<int, double>();
			foreach (IHydraulicConsumer item3 in _circuit.Elements.OfType<IHydraulicConsumer>())
			{
				if (!item3.WantsFlow())
				{
					continue;
				}
				foreach (Port port2 in ((CircuitElement)item3).Ports)
				{
					if (vB_0024AnonymousDelegate_(port2.NodeIndex))
					{
						int key3 = func(port2.NodeIndex);
						dictionary3[key3] = Math.Max(dictionary3.ContainsKey(key3) ? dictionary3[key3] : 0.0, item3.LoadPressure);
					}
				}
			}
			Dictionary<int, double> dictionary4 = new Dictionary<int, double>();
			int num5 = count - 1;
			for (int l = 0; l <= num5; l++)
			{
				if (!vB_0024AnonymousDelegate_(l))
				{
					continue;
				}
				int num6 = func(l);
				if (!dictionary4.ContainsKey(num6))
				{
					double supplyPressure = _ports[l].SupplyPressure;
					double val = (dictionary2.ContainsKey(num6) ? dictionary2[num6] : (dictionary.TryGetValue(num6, out var value) ? value : supplyPressure));
					val = Math.Min(val, Math.Max(supplyPressure, 0.1));
					double num8 = (dictionary4[num6] = (hashSet.Contains(num6) ? Math.Min(val, 3.0) : ((!dictionary3.ContainsKey(num6)) ? val : Math.Min(val, dictionary3[num6] + 2.0))));
					if (dictionary.ContainsKey(num6) && !dictionary2.ContainsKey(num6) && !hashSet.Contains(num6) && !dictionary3.ContainsKey(num6))
					{
						Warnings.Add($"No pressure relief valve: with nothing moving the pump pressure rises to {num8:0} bar. Add a relief valve after the pump.");
					}
				}
			}
			foreach (Edge edge2 in _edges)
			{
				if (!double.IsPositiveInfinity(edge2.LimitAB) && vB_0024AnonymousDelegate_(edge2.A) && vB_0024AnonymousDelegate_(edge2.B))
				{
					int num9 = func(edge2.A);
					int num10 = func(edge2.B);
					if (num9 != num10 && dictionary4.ContainsKey(num9) && dictionary4.ContainsKey(num10))
					{
						dictionary4[num10] = Math.Min(dictionary4[num10], Math.Min(edge2.LimitAB, dictionary4[num9]));
					}
				}
			}
			int num11 = count - 1;
			for (int m = 0; m <= num11; m++)
			{
				if (vB_0024AnonymousDelegate_(m))
				{
					_ports[m].Pressure = dictionary4[func(m)];
					_ports[m].SupplyPressure = _ports[m].Pressure;
				}
			}
			_accumulatorFed.Clear();
			foreach (Accumulator item4 in _circuit.Elements.OfType<Accumulator>())
			{
				int key4 = func(item4.Ports[0].NodeIndex);
				bool flag = vB_0024AnonymousDelegate_(item4.Ports[0].NodeIndex) && dictionary.ContainsKey(key4);
				_accumulatorFed[item4] = flag;
				item4.Discharging = !flag;
			}
			foreach (HydraulicCylinder item5 in _circuit.Elements.OfType<HydraulicCylinder>())
			{
				Port port = item5.Ports[1];
				if (item5.OverrunPressure <= 0.0 || port.State != 0)
				{
					continue;
				}
				foreach (Tube tube in _circuit.Tubes)
				{
					if (tube.A == port && tube.B.State == PortState.Floating)
					{
						tube.B.Pressure = item5.OverrunPressure;
					}
					if (tube.B == port && tube.A.State == PortState.Floating)
					{
						tube.A.Pressure = item5.OverrunPressure;
					}
				}
				port.Pressure = item5.OverrunPressure;
			}
		}
	}
}
