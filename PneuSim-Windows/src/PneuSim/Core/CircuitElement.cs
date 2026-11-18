using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[TypeDescriptionProvider(typeof(ElementDescriptionProvider))]
public abstract class CircuitElement
{
	[Browsable(false)]
	public FaultKind Fault { get; set; }

	[Browsable(false)]
	public bool FaultHidden { get; set; }

	[Browsable(false)]
	public int Id { get; set; }

	[Browsable(false)]
	public float X { get; set; }

	[Browsable(false)]
	public float Y { get; set; }

	[Browsable(false)]
	public int Rotation { get; set; }

	[Category("General")]
	[Description("Identifier shown next to the symbol, e.g. 1A, 1V1, 1S2.")]
	public string Label { get; set; }

	[Browsable(false)]
	public List<Port> Ports { get; }

	[Browsable(false)]
	public string CrossReference { get; set; }

	[Browsable(false)]
	public int PageIndex { get; set; }

	[Browsable(false)]
	public abstract string TypeName { get; }

	[Browsable(false)]
	public abstract string DisplayName { get; }

	[Browsable(false)]
	public abstract RectangleF LocalBounds { get; }

	[Browsable(false)]
	public virtual PointF LabelAnchor
	{
		get
		{
			RectangleF localBounds = LocalBounds;
			return new PointF(localBounds.Left, localBounds.Top);
		}
	}

	[Browsable(false)]
	public virtual bool IsManuallyOperated => false;

	protected CircuitElement()
	{
		Fault = FaultKind.None;
		Label = "";
		Ports = new List<Port>();
		CrossReference = "";
	}

	/// <summary>Rated maximum pressure in bar. 0 means the default for the medium (10 bar air, 160 bar oil).</summary>
	[Category("Safety")]
	[DisplayName("Rated maximum pressure (bar)")]
	[Description("The highest pressure this component is built for. PneuSim sounds an alarm if it is exceeded. 0 = default (10 bar air, 160 bar oil).")]
	public double RatedPressureBar { get; set; }

	/// <summary>Pneumatic or Hydraulic for components that carry a fluid; null for purely electrical parts.</summary>
	public PortKind? FluidKind()
	{
		foreach (Port port in Ports)
		{
			if (port.Kind != PortKind.Electric)
			{
				return port.Kind;
			}
		}
		return null;
	}

	public double EffectiveRatedPressure => (RatedPressureBar > 0.0) ? RatedPressureBar : ((FluidKind() == PortKind.Hydraulic) ? 160.0 : 10.0);

	/// <summary>Highest offending pressure in bar if a port exceeds the rating (or the -0.95 bar vacuum rating); otherwise null.</summary>
	public double? OverRatedPressure()
	{
		if (!FluidKind().HasValue)
		{
			return null;
		}
		double effectiveRatedPressure = EffectiveRatedPressure;
		foreach (Port port in Ports)
		{
			if (port.Kind != PortKind.Electric && port.ConnectionCount > 0)
			{
				if (port.Pressure > effectiveRatedPressure + 0.05)
				{
					return port.Pressure;
				}
				if (port.Kind == PortKind.Pneumatic && port.Pressure < -0.97)
				{
					return port.Pressure;
				}
			}
		}
		return null;
	}

	public virtual bool ShowProperty(string name)
	{
		return true;
	}

	public virtual FaultKind[] PossibleFaults()
	{
		return Array.Empty<FaultKind>();
	}

	public virtual string FaultDescription(FaultKind kind)
	{
		return Faults.Describe(kind);
	}

	public virtual List<(string Name, string Value)> InspectValues()
	{
		return new List<(string, string)>();
	}

	public virtual void AfterStateRestored()
	{
	}

	protected Port AddPort(string name, float x, float y, float dx, float dy, bool vents = false, PortKind kind = PortKind.Pneumatic)
	{
		Port port = new Port(this, name)
		{
			Local = new PointF(x, y),
			Direction = new PointF(dx, dy),
			VentsWhenOpen = vents,
			Kind = kind
		};
		Ports.Add(port);
		return port;
	}

	public Port GetPort(string name)
	{
		return Ports.FirstOrDefault([SpecialName] (Port p) => Operators.CompareString(p.Name, name, TextCompare: false) == 0);
	}

	public Matrix GetMatrix()
	{
		Matrix matrix = new Matrix();
		matrix.Translate(X, Y);
		matrix.Rotate(90f * (float)(Rotation % 4));
		return matrix;
	}

	public PointF ToWorld(PointF p)
	{
		PointF[] array = new PointF[1] { p };
		using (Matrix matrix = GetMatrix())
		{
			matrix.TransformPoints(array);
		}
		return array[0];
	}

	public PointF RotateVector(PointF v)
	{
		PointF[] array = new PointF[1] { v };
		using (Matrix matrix = GetMatrix())
		{
			matrix.TransformVectors(array);
		}
		return new PointF((float)Math.Round(array[0].X, 4), (float)Math.Round(array[0].Y, 4));
	}

	public PointF ToLocal(PointF p)
	{
		PointF[] array = new PointF[1] { p };
		using (Matrix matrix = GetMatrix())
		{
			matrix.Invert();
			matrix.TransformPoints(array);
		}
		return array[0];
	}

	public RectangleF WorldBounds()
	{
		RectangleF localBounds = LocalBounds;
		PointF[] array = new PointF[4]
		{
			new PointF(localBounds.Left, localBounds.Top),
			new PointF(localBounds.Right, localBounds.Top),
			new PointF(localBounds.Right, localBounds.Bottom),
			new PointF(localBounds.Left, localBounds.Bottom)
		};
		using (Matrix matrix = GetMatrix())
		{
			matrix.TransformPoints(array);
		}
		float left = array.Min([SpecialName] (PointF p) => p.X);
		float top = array.Min([SpecialName] (PointF p) => p.Y);
		float right = array.Max([SpecialName] (PointF p) => p.X);
		float bottom = array.Max([SpecialName] (PointF p) => p.Y);
		return RectangleF.FromLTRB(left, top, right, bottom);
	}

	public virtual bool HitTest(PointF world)
	{
		RectangleF localBounds = LocalBounds;
		localBounds.Inflate(2f, 2f);
		return localBounds.Contains(ToLocal(world));
	}

	public abstract void DrawSymbol(DrawSurface g, RenderContext r);

	public virtual void ResetSim()
	{
		foreach (Port port in Ports)
		{
			port.ResetSim();
		}
	}

	public virtual void AddEdges(Simulator sim)
	{
	}

	public virtual void AddTerminals(Simulator sim)
	{
	}

	public virtual bool UpdateLogic(Simulator sim)
	{
		return false;
	}

	public virtual void UpdateDynamics(Simulator sim, double dt)
	{
	}

	public virtual void OnSimMouseDown(PointF local)
	{
	}

	public virtual void OnSimMouseUp()
	{
	}

	public override string ToString()
	{
		if (!string.IsNullOrEmpty(Label))
		{
			return $"{Label}  ({DisplayName})";
		}
		return DisplayName;
	}
}
