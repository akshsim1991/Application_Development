using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Xml.Linq;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class Project
{
	[Serializable]
	[CompilerGenerated]
	internal sealed class _Closure_0024__
	{
		public static readonly _Closure_0024__ _0024I;

		public static Func<ProjectPage, IEnumerable<CircuitElement>> _0024I14_002D0;

		public static Func<ElectricCoil, bool> _0024I18_002D0;

		public static Func<ElectricContact, bool> _0024I18_002D1;

		public static Func<DirectionalValve, string> _0024I18_002D4;

		public static Func<ElectricContact, bool> _0024I18_002D5;

		public static Func<ElectricContact, string> _0024I18_002D6;

		public static Func<ElectricContact, bool> _0024I18_002D7;

		public static Func<ElectricContact, string> _0024I18_002D8;

		public static Func<PageConnector, string> _0024I18_002D10;

		static _Closure_0024__()
		{
			_0024I = new _Closure_0024__();
		}

		[SpecialName]
		internal IEnumerable<CircuitElement> _Lambda_0024__14_002D0(ProjectPage p)
		{
			return p.Circuit.Elements;
		}

		[SpecialName]
		internal bool _Lambda_0024__18_002D0(ElectricCoil c)
		{
			if (c.Kind != CoilKind.Lamp)
			{
				return !string.IsNullOrWhiteSpace(c.Label);
			}
			return false;
		}

		[SpecialName]
		internal bool _Lambda_0024__18_002D1(ElectricContact c)
		{
			return c.Operator == ContactOperator.Relay;
		}

		[SpecialName]
		internal string _Lambda_0024__18_002D4(DirectionalValve v)
		{
			return (string.IsNullOrEmpty(v.Label) ? "valve" : v.Label) + " " + LocationOf(v);
		}

		[SpecialName]
		internal bool _Lambda_0024__18_002D5(ElectricContact k)
		{
			return !k.NormallyClosed;
		}

		[SpecialName]
		internal string _Lambda_0024__18_002D6(ElectricContact k)
		{
			return LocationOf(k);
		}

		[SpecialName]
		internal bool _Lambda_0024__18_002D7(ElectricContact k)
		{
			return k.NormallyClosed;
		}

		[SpecialName]
		internal string _Lambda_0024__18_002D8(ElectricContact k)
		{
			return LocationOf(k);
		}

		[SpecialName]
		internal string _Lambda_0024__18_002D10(PageConnector o)
		{
			return LocationOf(o);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__18_002D0
	{
		public ElectricCoil _0024VB_0024Local_coil;

		public _Closure_0024__18_002D0(_Closure_0024__18_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_coil = arg0._0024VB_0024Local_coil;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__2(ElectricContact k)
		{
			return string.Equals(k.Reference?.Trim(), _0024VB_0024Local_coil.Label.Trim(), StringComparison.OrdinalIgnoreCase);
		}

		[SpecialName]
		internal bool _Lambda_0024__3(DirectionalValve v)
		{
			return v.UsesSolenoid(_0024VB_0024Local_coil.Label);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__18_002D1
	{
		public PageConnector _0024VB_0024Local_pc;

		public _Closure_0024__18_002D1(_Closure_0024__18_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_pc = arg0._0024VB_0024Local_pc;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__9(PageConnector o)
		{
			if (o != _0024VB_0024Local_pc)
			{
				return o.Matches(_0024VB_0024Local_pc);
			}
			return false;
		}
	}

	public const float ColumnWidth = 100f;

	public List<ProjectPage> Pages { get; }

	public ProjectInfo Info { get; set; }

	public Dictionary<string, double> PartCosts { get; }

	public Project()
	{
		Pages = new List<ProjectPage>();
		Info = new ProjectInfo();
		PartCosts = new Dictionary<string, double>();
	}

	public Project(Circuit first, string title = null)
	{
		Pages = new List<ProjectPage>();
		Info = new ProjectInfo();
		PartCosts = new Dictionary<string, double>();
		Pages.Add(new ProjectPage
		{
			Name = "Page 1",
			Circuit = first
		});
		if (title != null)
		{
			Info.Title = title;
		}
	}

	public ProjectPage AddPage(string name = null)
	{
		ProjectPage projectPage = new ProjectPage
		{
			Name = (name ?? $"Page {checked(Pages.Count + 1)}"),
			Circuit = new Circuit()
		};
		Pages.Add(projectPage);
		return projectPage;
	}

	public Circuit SimulationCircuit()
	{
		if (Pages.Count == 1)
		{
			return Pages[0].Circuit;
		}
		Circuit circuit = new Circuit();
		foreach (ProjectPage page in Pages)
		{
			circuit.Elements.AddRange(page.Circuit.Elements);
			circuit.Tubes.AddRange(page.Circuit.Tubes);
		}
		return circuit;
	}

	public IEnumerable<CircuitElement> AllElements()
	{
		return Pages.SelectMany([SpecialName] (ProjectPage p) => p.Circuit.Elements);
	}

	public static int ColumnAt(float x)
	{
		return Math.Max(1, checked((int)Math.Floor(x / 100f) + 1));
	}

	public static string LocationOf(CircuitElement e)
	{
		RectangleF rectangleF = e.WorldBounds();
		return $"{checked(e.PageIndex + 1)}.{ColumnAt(rectangleF.X + rectangleF.Width / 2f)}";
	}

	public void UpdateCrossReferences()
	{
		checked
		{
			int num = Pages.Count - 1;
			for (int i = 0; i <= num; i++)
			{
				foreach (CircuitElement element in Pages[i].Circuit.Elements)
				{
					element.PageIndex = i;
					element.CrossReference = "";
				}
			}
			List<CircuitElement> source = AllElements().ToList();
			List<ElectricCoil> list = (from c in source.OfType<ElectricCoil>()
				where c.Kind != CoilKind.Lamp && !string.IsNullOrWhiteSpace(c.Label)
				select c).ToList();
			List<ElectricContact> source2 = (from c in source.OfType<ElectricContact>()
				where c.Operator == ContactOperator.Relay
				select c).ToList();
			using (List<ElectricCoil>.Enumerator enumerator2 = list.GetEnumerator())
			{
				_Closure_0024__18_002D0 closure_0024__18_002D = default(_Closure_0024__18_002D0);
				while (enumerator2.MoveNext())
				{
					closure_0024__18_002D = new _Closure_0024__18_002D0(closure_0024__18_002D);
					closure_0024__18_002D._0024VB_0024Local_coil = enumerator2.Current;
					List<ElectricContact> list2 = source2.Where(closure_0024__18_002D._Lambda_0024__2).ToList();
					if (closure_0024__18_002D._0024VB_0024Local_coil.Kind == CoilKind.Solenoid)
					{
						List<DirectionalValve> source3 = source.OfType<DirectionalValve>().Where(closure_0024__18_002D._Lambda_0024__3).ToList();
						closure_0024__18_002D._0024VB_0024Local_coil.CrossReference = string.Join(" ", source3.Select([SpecialName] (DirectionalValve v) => (string.IsNullOrEmpty(v.Label) ? "valve" : v.Label) + " " + LocationOf(v)));
					}
					else
					{
						IEnumerable<string> enumerable = from k in list2
							where !k.NormallyClosed
							select LocationOf(k);
						IEnumerable<string> enumerable2 = from k in list2
							where k.NormallyClosed
							select LocationOf(k);
						List<string> list3 = new List<string>();
						if (enumerable.Any())
						{
							list3.Add("NO " + string.Join(",", enumerable));
						}
						if (enumerable2.Any())
						{
							list3.Add("NC " + string.Join(",", enumerable2));
						}
						closure_0024__18_002D._0024VB_0024Local_coil.CrossReference = string.Join("  ", list3);
					}
					foreach (ElectricContact item in list2)
					{
						item.CrossReference = LocationOf(closure_0024__18_002D._0024VB_0024Local_coil);
					}
				}
			}
			List<PageConnector> list4 = source.OfType<PageConnector>().ToList();
			using List<PageConnector>.Enumerator enumerator4 = list4.GetEnumerator();
			_Closure_0024__18_002D1 closure_0024__18_002D2 = default(_Closure_0024__18_002D1);
			while (enumerator4.MoveNext())
			{
				closure_0024__18_002D2 = new _Closure_0024__18_002D1(closure_0024__18_002D2);
				closure_0024__18_002D2._0024VB_0024Local_pc = enumerator4.Current;
				List<string> list5 = list4.Where(closure_0024__18_002D2._Lambda_0024__9).Select((_Closure_0024__._0024I18_002D10 == null) ? (_Closure_0024__._0024I18_002D10 = [SpecialName] (PageConnector o) => LocationOf(o)) : _Closure_0024__._0024I18_002D10).ToList();
				closure_0024__18_002D2._0024VB_0024Local_pc.CrossReference = ((list5.Count == 0) ? "" : ("→ " + string.Join(", ", list5)));
			}
		}
	}

	public string ToXml()
	{
		return ToXDocument().ToString(SaveOptions.DisableFormatting);
	}

	public static Project FromXml(string xml)
	{
		return FromXDocument(XDocument.Parse(xml));
	}

	public void Save(string path)
	{
		ToXDocument().Save(path);
	}

	public static Project Load(string path)
	{
		return FromXDocument(XDocument.Load(path));
	}

	private XDocument ToXDocument()
	{
		CultureInfo invariantCulture = CultureInfo.InvariantCulture;
		XElement xElement = new XElement("PneuSimProject", new XAttribute("version", 3), Info.ToXElement());
		XElement xElement2 = new XElement("PartCosts");
		foreach (KeyValuePair<string, double> partCost in PartCosts)
		{
			xElement2.Add(new XElement("Part", new XAttribute("name", partCost.Key), new XAttribute("cost", partCost.Value.ToString(invariantCulture))));
		}
		xElement.Add(xElement2);
		foreach (ProjectPage page in Pages)
		{
			xElement.Add(new XElement("Page", new XAttribute("name", page.Name), page.Circuit.ToXElement()));
		}
		return new XDocument(xElement);
	}

	private static Project FromXDocument(XDocument doc)
	{
		if (doc.Root == null)
		{
			throw new InvalidOperationException("Empty file.");
		}
		if (Operators.CompareString(doc.Root.Name.LocalName, "PneuSimCircuit", TextCompare: false) == 0)
		{
			Circuit circuit = Circuit.FromXElement(doc.Root);
			circuit.EnsureOnSheet();
			return new Project(circuit);
		}
		if (Operators.CompareString(doc.Root.Name.LocalName, "PneuSimProject", TextCompare: false) != 0)
		{
			throw new InvalidOperationException("This is not a PneuSim file.");
		}
		Project project = new Project
		{
			Info = ProjectInfo.FromXElement(doc.Root.Element("Info"))
		};
		XElement xElement = doc.Root.Element("PartCosts");
		if (xElement != null)
		{
			foreach (XElement item in xElement.Elements("Part"))
			{
				if (double.TryParse((string)item.Attribute("cost"), NumberStyles.Float, CultureInfo.InvariantCulture, out var result))
				{
					project.PartCosts[(string)item.Attribute("name")] = result;
				}
			}
		}
		foreach (XElement item2 in doc.Root.Elements("Page"))
		{
			XElement xElement2 = item2.Element("PneuSimCircuit");
			project.Pages.Add(new ProjectPage
			{
				Name = (string)item2.Attribute("name"),
				Circuit = ((xElement2 == null) ? new Circuit() : Circuit.FromXElement(xElement2))
			});
		}
		if (project.Pages.Count == 0)
		{
			project.AddPage();
		}
		foreach (ProjectPage page in project.Pages)
		{
			page.Circuit.EnsureOnSheet();
		}
		return project;
	}
}
