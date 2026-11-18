using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class SimulationRecorder
{
	private class Frame
	{
		public double Time;

		public string Signature;

		public List<object[]> Values;

		public Frame()
		{
			Values = new List<object[]>();
		}
	}

	public const double Interval = 0.05;

	public const int MaxFrames = 2400;

	private static readonly Dictionary<Type, FieldInfo[]> FieldCache = new Dictionary<Type, FieldInfo[]>();

	private readonly Simulator _sim;

	private readonly List<Frame> _frames;

	private int _index;

	public int Count => _frames.Count;

	public int Index => _index;

	public bool IsRewound
	{
		get
		{
			if (_index >= 0)
			{
				return _index < checked(_frames.Count - 1);
			}
			return false;
		}
	}

	public SimulationRecorder(Simulator sim)
	{
		_frames = new List<Frame>();
		_index = -1;
		_sim = sim;
	}

	public double TimeAt(int i)
	{
		return _frames[i].Time;
	}

	private static FieldInfo[] FieldsOf(Type t)
	{
		FieldInfo[] value = null;
		lock (FieldCache)
		{
			if (FieldCache.TryGetValue(t, out value))
			{
				return value;
			}
			List<FieldInfo> list = new List<FieldInfo>();
			Type type = t;
			while ((object)type != null && (object)type != typeof(object))
			{
				FieldInfo[] fields = type.GetFields(BindingFlags.DeclaredOnly | BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
				foreach (FieldInfo fieldInfo in fields)
				{
					if (!fieldInfo.IsInitOnly && (fieldInfo.FieldType.IsValueType || (object)fieldInfo.FieldType == typeof(string)))
					{
						list.Add(fieldInfo);
					}
				}
				type = type.BaseType;
			}
			value = list.ToArray();
			FieldCache[t] = value;
		}
		return value;
	}

	private static object[] Capture(object o)
	{
		FieldInfo[] array = FieldsOf(o.GetType());
		checked
		{
			object[] array2 = new object[array.Length - 1 + 1];
			int num = array.Length - 1;
			for (int i = 0; i <= num; i++)
			{
				array2[i] = RuntimeHelpers.GetObjectValue(array[i].GetValue(RuntimeHelpers.GetObjectValue(o)));
			}
			return array2;
		}
	}

	private static void Apply(object o, object[] values)
	{
		FieldInfo[] array = FieldsOf(o.GetType());
		checked
		{
			int num = array.Length - 1;
			for (int i = 0; i <= num; i++)
			{
				array[i].SetValue(RuntimeHelpers.GetObjectValue(o), RuntimeHelpers.GetObjectValue(values[i]));
			}
		}
	}

	private IEnumerable<object> Objects()
	{
		List<object> list = new List<object> { _sim };
		foreach (CircuitElement element in _sim.Circuit.Elements)
		{
			list.Add(element);
			list.AddRange(element.Ports);
		}
		return list;
	}

	public void Record(bool force = false)
	{
		if (IsRewound)
		{
			TruncateAfterCurrent();
		}
		checked
		{
			if (!force && _frames.Count > 0 && _sim.Time < _frames[_frames.Count - 1].Time + 0.05 - 1E-06)
			{
				return;
			}
			Frame frame = new Frame
			{
				Time = _sim.Time,
				Signature = _sim.DiscreteState()
			};
			foreach (object item in Objects())
			{
				object objectValue = RuntimeHelpers.GetObjectValue(item);
				frame.Values.Add(Capture(RuntimeHelpers.GetObjectValue(objectValue)));
			}
			_frames.Add(frame);
			if (_frames.Count > 2400)
			{
				_frames.RemoveAt(0);
			}
			_index = _frames.Count - 1;
		}
	}

	public void Restore(int i)
	{
		if (_frames.Count == 0)
		{
			return;
		}
		checked
		{
			i = Math.Max(0, Math.Min(_frames.Count - 1, i));
			Frame frame = _frames[i];
			List<object> list = Objects().ToList();
			int num = Math.Min(list.Count, frame.Values.Count) - 1;
			for (int j = 0; j <= num; j++)
			{
				Apply(RuntimeHelpers.GetObjectValue(list[j]), frame.Values[j]);
			}
			foreach (CircuitElement element in _sim.Circuit.Elements)
			{
				element.AfterStateRestored();
			}
			_sim.TrimRecordings(frame.Time);
			_index = i;
		}
	}

	public void TruncateAfterCurrent()
	{
		checked
		{
			if (_index >= 0 && _index < _frames.Count - 1)
			{
				_frames.RemoveRange(_index + 1, _frames.Count - _index - 1);
			}
		}
	}

	public int NextEventFrame()
	{
		if (_index < 0)
		{
			return -1;
		}
		string signature = _frames[_index].Signature;
		checked
		{
			int num = _index + 1;
			int num2 = _frames.Count - 1;
			for (int i = num; i <= num2; i++)
			{
				if (Operators.CompareString(_frames[i].Signature, signature, TextCompare: false) != 0)
				{
					return i;
				}
			}
			return -1;
		}
	}

	public int PreviousEventFrame()
	{
		if (_index <= 0)
		{
			return -1;
		}
		string signature = _frames[_index].Signature;
		checked
		{
			for (int i = _index - 1; i >= 0; i += -1)
			{
				if (Operators.CompareString(_frames[i].Signature, signature, TextCompare: false) != 0)
				{
					return i;
				}
			}
			return -1;
		}
	}
}
