using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Globalization;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Xml.Linq;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class Circuit
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__10_002D0
	{
		public CircuitElement _0024VB_0024Local_e;

		public Func<Tube, bool> _0024I0;

		public _Closure_0024__10_002D0(_Closure_0024__10_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_e = arg0._0024VB_0024Local_e;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(Tube tb)
		{
			if (tb.A.Owner != _0024VB_0024Local_e)
			{
				return tb.B.Owner == _0024VB_0024Local_e;
			}
			return true;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__30_002D0
	{
		public HashSet<CircuitElement> _0024VB_0024Local_set_;

		public Func<Tube, bool> _0024I0;

		public _Closure_0024__30_002D0(_Closure_0024__30_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_set_ = arg0._0024VB_0024Local_set_;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__0(Tube tb)
		{
			if (_0024VB_0024Local_set_.Contains(tb.A.Owner))
			{
				return _0024VB_0024Local_set_.Contains(tb.B.Owner);
			}
			return false;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__31_002D0
	{
		public CircuitElement _0024VB_0024Local_e;

		public _Closure_0024__31_002D0(_Closure_0024__31_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_e = arg0._0024VB_0024Local_e;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0(FaultKind k, bool h)
		{
			_0024VB_0024Local_e.Fault = k;
			_0024VB_0024Local_e.FaultHidden = h;
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__31_002D1
	{
		public Tube _0024VB_0024Local_t;

		public _Closure_0024__31_002D1(_Closure_0024__31_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_t = arg0._0024VB_0024Local_t;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__1(FaultKind k, bool h)
		{
			_0024VB_0024Local_t.Fault = k;
			_0024VB_0024Local_t.FaultHidden = h;
		}
	}

	private int _nextId;

	public List<CircuitElement> Elements { get; }

	public List<Tube> Tubes { get; }

	public Circuit()
	{
		Elements = new List<CircuitElement>();
		Tubes = new List<Tube>();
		_nextId = 1;
	}

	public void Clear()
	{
		Elements.Clear();
		Tubes.Clear();
		_nextId = 1;
	}

	public T Add<T>(T e, float x, float y, string label = null) where T : CircuitElement
	{
		e.X = x;
		e.Y = y;
		if (label != null)
		{
			e.Label = label;
		}
		if (e.Id == 0 || Elements.Any([SpecialName] (CircuitElement o) => o.Id == e.Id))
		{
			e.Id = _nextId;
		}
		_nextId = Math.Max(_nextId, checked(e.Id + 1));
		Elements.Add(e);
		return e;
	}

	public void Remove(CircuitElement e)
	{
		_Closure_0024__10_002D0 arg = default(_Closure_0024__10_002D0);
		_Closure_0024__10_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__10_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_e = e;
		foreach (Tube item in Tubes.Where([SpecialName] (Tube tb) => tb.A.Owner == CS_0024_003C_003E8__locals0._0024VB_0024Local_e || tb.B.Owner == CS_0024_003C_003E8__locals0._0024VB_0024Local_e).ToList())
		{
			RemoveTube(item);
		}
		Elements.Remove(CS_0024_003C_003E8__locals0._0024VB_0024Local_e);
	}

	public static bool CanConnect(Port a, Port b)
	{
		if (a != null && b != null && a != b && a.Owner != b.Owner)
		{
			return a.Kind == b.Kind;
		}
		return false;
	}

	public Tube Connect(Port a, Port b)
	{
		if (!CanConnect(a, b))
		{
			return null;
		}
		if (Tubes.Any([SpecialName] (Tube t) => (t.A == a && t.B == b) || (t.A == b && t.B == a)))
		{
			return null;
		}
		Tube tube = new Tube(a, b);
		tube.Obstacles = () => Elements;
		Tubes.Add(tube);
		checked
		{
			a.ConnectionCount++;
			b.ConnectionCount++;
			return tube;
		}
	}

	public Tube Connect(CircuitElement a, string aPort, CircuitElement b, string bPort)
	{
		return Connect(a.GetPort(aPort), b.GetPort(bPort));
	}

	public void RemoveTube(Tube t)
	{
		checked
		{
			if (Tubes.Remove(t))
			{
				t.A.ConnectionCount--;
				t.B.ConnectionCount--;
			}
		}
	}

	public void CleanupTubes()
	{
		foreach (Tube item in Tubes.Where([SpecialName] (Tube tb) => !tb.A.Owner.Ports.Contains(tb.A) || !tb.B.Owner.Ports.Contains(tb.B) || tb.A.Kind != tb.B.Kind).ToList())
		{
			RemoveTube(item);
		}
	}

	public IEnumerable<Port> AllPorts()
	{
		return Elements.SelectMany([SpecialName] (CircuitElement e) => e.Ports);
	}

	public Port FindPortAt(PointF p, float radius)
	{
		Port result = null;
		float num = radius;
		foreach (Port item in AllPorts())
		{
			PointF pointF = item.WorldPos();
			float num2 = (float)Math.Sqrt(Math.Pow(pointF.X - p.X, 2.0) + Math.Pow(pointF.Y - p.Y, 2.0));
			if (num2 <= num)
			{
				result = item;
				num = num2;
			}
		}
		return result;
	}

	public CircuitElement FindElementAt(PointF p)
	{
		checked
		{
			for (int i = Elements.Count - 1; i >= 0; i += -1)
			{
				if (Elements[i].HitTest(p))
				{
					return Elements[i];
				}
			}
			return null;
		}
	}

	public Tube FindTubeAt(PointF p, float tolerance)
	{
		return Tubes.FirstOrDefault([SpecialName] (Tube t) => t.DistanceTo(p) <= tolerance);
	}

	public void EnsureOnSheet(float margin = 20f)
	{
		RectangleF rectangleF = Bounds();
		if (rectangleF.IsEmpty)
		{
			return;
		}
		float num = ((rectangleF.Left < 0f) ? ((float)(Math.Ceiling((margin - rectangleF.Left) / 10f) * 10.0)) : 0f);
		float num2 = ((rectangleF.Top < 0f) ? ((float)(Math.Ceiling((margin - rectangleF.Top) / 10f) * 10.0)) : 0f);
		if (num == 0f && num2 == 0f)
		{
			return;
		}
		foreach (CircuitElement element in Elements)
		{
			element.X += num;
			CircuitElement circuitElement = element;
			element.Y = circuitElement.Y + num2;
		}
		foreach (Tube item in Tubes.Where([SpecialName] (Tube tb) => tb.Mid.HasValue))
		{
			item.Mid = item.Mid.Value + ((Operators.CompareString(item.MidAxis(), "X", TextCompare: false) == 0) ? num : num2);
		}
	}

	public RectangleF Bounds()
	{
		if (Elements.Count == 0)
		{
			return RectangleF.Empty;
		}
		RectangleF rectangleF = Elements[0].WorldBounds();
		foreach (CircuitElement item in Elements.Skip(1))
		{
			rectangleF = RectangleF.Union(rectangleF, item.WorldBounds());
		}
		return rectangleF;
	}

	public void Save(string path)
	{
		ToXDocument(Elements).Save(path);
	}

	public static Circuit Load(string path)
	{
		return FromXDocument(XDocument.Load(path));
	}

	public string ToXml()
	{
		return ToXDocument(Elements).ToString(SaveOptions.DisableFormatting);
	}

	public static Circuit FromXml(string xml)
	{
		return FromXDocument(XDocument.Parse(xml));
	}

	public string ExtractXml(IEnumerable<CircuitElement> subset)
	{
		return ToXDocument(subset.ToList()).ToString(SaveOptions.DisableFormatting);
	}

	public List<CircuitElement> Merge(string xml, float dx, float dy)
	{
		Circuit circuit = FromXml(xml);
		List<CircuitElement> list = new List<CircuitElement>();
		foreach (CircuitElement element in circuit.Elements)
		{
			element.Id = 0;
			Add(element, element.X + dx, element.Y + dy);
			list.Add(element);
		}
		checked
		{
			foreach (Tube tube2 in circuit.Tubes)
			{
				tube2.A.ConnectionCount--;
				tube2.B.ConnectionCount--;
				Tube tube = Connect(tube2.A, tube2.B);
				if (tube != null && tube2.Mid.HasValue)
				{
					tube.Mid = tube2.Mid.Value + ((Operators.CompareString(tube.MidAxis(), "X", TextCompare: false) == 0) ? dx : dy);
				}
				if (tube != null)
				{
					tube.Fault = tube2.Fault;
					tube.FaultHidden = tube2.FaultHidden;
				}
			}
			return list;
		}
	}

	public XElement ToXElement()
	{
		return ToXDocument(Elements).Root;
	}

	public static Circuit FromXElement(XElement root)
	{
		return FromXDocument(new XDocument(new XElement(root)));
	}

	private XDocument ToXDocument(List<CircuitElement> subset)
	{
		_Closure_0024__30_002D0 arg = default(_Closure_0024__30_002D0);
		_Closure_0024__30_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__30_002D0(arg);
		CultureInfo invariantCulture = CultureInfo.InvariantCulture;
		XElement xElement = new XElement("PneuSimCircuit", new XAttribute("version", 2));
		foreach (CircuitElement item in subset)
		{
			XElement xElement2 = new XElement("Element", new XAttribute("type", item.TypeName), new XAttribute("id", item.Id), new XAttribute("x", item.X.ToString(invariantCulture)), new XAttribute("y", item.Y.ToString(invariantCulture)), new XAttribute("rotation", item.Rotation));
			WriteFault(xElement2, item.Fault, item.FaultHidden);
			foreach (PropertyInfo item2 in PersistentProperties(item))
			{
				object objectValue = RuntimeHelpers.GetObjectValue(item2.GetValue(item));
				if ((item2.Name == "RatedPressureBar" && Convert.ToDouble(objectValue, invariantCulture) == 0.0) || (item2.Name == "ValueLabel" && string.IsNullOrEmpty(Convert.ToString(objectValue, invariantCulture))) || (item is CounterbalanceValve && ((item2.Name == "Hydraulic" || item2.Name == "ExternalPilot") && objectValue is bool defaultFlag && defaultFlag)))
				{
					continue;
				}
				xElement2.Add(new XElement("Property", new XAttribute("name", item2.Name), new XAttribute("value", Convert.ToString(RuntimeHelpers.GetObjectValue(objectValue), invariantCulture))));
			}
			xElement.Add(xElement2);
		}
		CS_0024_003C_003E8__locals0._0024VB_0024Local_set_ = new HashSet<CircuitElement>(subset);
		foreach (Tube item3 in Tubes.Where([SpecialName] (Tube tb) => CS_0024_003C_003E8__locals0._0024VB_0024Local_set_.Contains(tb.A.Owner) && CS_0024_003C_003E8__locals0._0024VB_0024Local_set_.Contains(tb.B.Owner)))
		{
			XElement xElement3 = new XElement("Tube", new XAttribute("from", item3.A.Owner.Id), new XAttribute("fromPort", item3.A.Name), new XAttribute("to", item3.B.Owner.Id), new XAttribute("toPort", item3.B.Name));
			if (item3.Mid.HasValue)
			{
				xElement3.Add(new XAttribute("mid", item3.Mid.Value.ToString(invariantCulture)));
			}
			WriteFault(xElement3, item3.Fault, item3.FaultHidden);
			xElement.Add(xElement3);
		}
		return new XDocument(xElement);
	}

	private static Circuit FromXDocument(XDocument doc)
	{
		CultureInfo invariantCulture = CultureInfo.InvariantCulture;
		if (doc.Root == null || Operators.CompareString(doc.Root.Name.LocalName, "PneuSimCircuit", TextCompare: false) != 0)
		{
			throw new InvalidOperationException("This is not a PneuSim circuit file.");
		}
		Circuit circuit = new Circuit();
		Dictionary<int, CircuitElement> dictionary = new Dictionary<int, CircuitElement>();
		_Closure_0024__31_002D0 closure_0024__31_002D = default(_Closure_0024__31_002D0);
		foreach (XElement item in doc.Root.Elements("Element"))
		{
			closure_0024__31_002D = new _Closure_0024__31_002D0(closure_0024__31_002D);
			closure_0024__31_002D._0024VB_0024Local_e = ElementFactory.Create((string)item.Attribute("type"));
			if (closure_0024__31_002D._0024VB_0024Local_e == null)
			{
				continue;
			}
			closure_0024__31_002D._0024VB_0024Local_e.Id = (int)item.Attribute("id");
			closure_0024__31_002D._0024VB_0024Local_e.Rotation = (int)item.Attribute("rotation");
			foreach (XElement item2 in item.Elements("Property"))
			{
				PropertyInfo property = closure_0024__31_002D._0024VB_0024Local_e.GetType().GetProperty((string)item2.Attribute("name"));
				if ((object)property != null && property.CanWrite)
				{
					string value = (string)item2.Attribute("value");
					try
					{
						property.SetValue(value: RuntimeHelpers.GetObjectValue((!property.PropertyType.IsEnum) ? RuntimeHelpers.GetObjectValue(Convert.ChangeType(value, property.PropertyType, invariantCulture)) : RuntimeHelpers.GetObjectValue(Enum.Parse(property.PropertyType, value))), obj: closure_0024__31_002D._0024VB_0024Local_e);
					}
					catch (Exception ex) when (((Func<bool>)delegate
					{
						// Could not convert BlockContainer to single expression
						ProjectData.SetProjectError(ex);
						return ex is FormatException || ex is ArgumentException;
					}).Invoke())
					{
						ProjectData.ClearProjectError();
					}
				}
			}
			ReadFault(item, closure_0024__31_002D._Lambda_0024__0);
			int id = closure_0024__31_002D._0024VB_0024Local_e.Id;
			circuit.Add(closure_0024__31_002D._0024VB_0024Local_e, float.Parse((string)item.Attribute("x"), invariantCulture), float.Parse((string)item.Attribute("y"), invariantCulture));
			dictionary[id] = closure_0024__31_002D._0024VB_0024Local_e;
		}
		_Closure_0024__31_002D1 closure_0024__31_002D2 = default(_Closure_0024__31_002D1);
		foreach (XElement item3 in doc.Root.Elements("Tube"))
		{
			CircuitElement value2 = null;
			CircuitElement value3 = null;
			if (dictionary.TryGetValue((int)item3.Attribute("from"), out value2) && dictionary.TryGetValue((int)item3.Attribute("to"), out value3))
			{
				closure_0024__31_002D2 = new _Closure_0024__31_002D1(closure_0024__31_002D2);
				closure_0024__31_002D2._0024VB_0024Local_t = circuit.Connect(value2.GetPort((string)item3.Attribute("fromPort")), value3.GetPort((string)item3.Attribute("toPort")));
				XAttribute xAttribute = item3.Attribute("mid");
				if (closure_0024__31_002D2._0024VB_0024Local_t != null && xAttribute != null)
				{
					closure_0024__31_002D2._0024VB_0024Local_t.Mid = float.Parse(xAttribute.Value, invariantCulture);
				}
				if (closure_0024__31_002D2._0024VB_0024Local_t != null)
				{
					ReadFault(item3, closure_0024__31_002D2._Lambda_0024__1);
				}
			}
		}
		return circuit;
	}

	private static void WriteFault(XElement x, FaultKind kind, bool hidden)
	{
		if (kind != 0)
		{
			if (hidden)
			{
				x.Add(new XAttribute("hf2", Faults.Protect(kind)));
			}
			else
			{
				x.Add(new XAttribute("fault", kind.ToString()));
			}
		}
	}

	private static void ReadFault(XElement x, Action<FaultKind, bool> apply)
	{
		XAttribute xAttribute = x.Attribute("fault");
		XAttribute xAttribute2 = x.Attribute("hf");
		XAttribute xAttribute3 = x.Attribute("hf2");
		if (xAttribute != null && Enum.TryParse<FaultKind>(xAttribute.Value, out var result))
		{
			apply(result, arg2: false);
		}
		else if (xAttribute3 != null)
		{
			result = Faults.Unprotect(xAttribute3.Value);
			if (result != 0)
			{
				apply(result, arg2: true);
			}
		}
		else if (xAttribute2 != null)
		{
			result = Faults.Unscramble(xAttribute2.Value);
			if (result != 0)
			{
				apply(result, arg2: true);
			}
		}
	}

	public int FaultCount()
	{
		return checked(Elements.Where([SpecialName] (CircuitElement e) => e.Fault != FaultKind.None).Count() + Tubes.Where([SpecialName] (Tube t) => t.Fault != FaultKind.None).Count());
	}

	private static IEnumerable<PropertyInfo> PersistentProperties(CircuitElement e)
	{
		return from p in e.GetType().GetProperties(BindingFlags.Instance | BindingFlags.Public)
			where p.CanRead && p.CanWrite && (object)p.GetSetMethod() != null && p.GetIndexParameters().Length == 0 && (p.GetCustomAttribute<BrowsableAttribute>() == null || p.GetCustomAttribute<BrowsableAttribute>().Browsable)
			select p;
	}
}
