using System;
using System.Collections.Generic;
using System.Drawing;

namespace PneuSim;

public class Tube
{
	public Port A { get; }

	public Port B { get; }

	public float? Mid { get; set; }

	public FaultKind Fault { get; set; }

	public bool FaultHidden { get; set; }

	public bool IsElectric => A.Kind == PortKind.Electric;

	public bool IsPressurized => A.IsPressurized;

	public Tube(Port a, Port b)
	{
		Fault = FaultKind.None;
		A = a;
		B = b;
	}

	public FaultKind[] PossibleFaults()
	{
		if (IsElectric)
		{
			return new FaultKind[1] { FaultKind.Blocked };
		}
		return new FaultKind[2]
		{
			FaultKind.Leak,
			FaultKind.Blocked
		};
	}

	public string FaultDescription(FaultKind kind)
	{
		if (IsElectric)
		{
			return (kind == FaultKind.Blocked) ? "Wire broken (no connection)" : Faults.Describe(kind);
		}
		string arg = ((A.Kind == PortKind.Hydraulic) ? "Hose" : "Tube");
		return kind switch
		{
			FaultKind.Leak => $"{arg} leaking (cut or loose fitting)", 
			FaultKind.Blocked => $"{arg} blocked (kinked or crushed)", 
			_ => Faults.Describe(kind), 
		};
	}

	public PointF[] Route()
	{
		return RouteBetween(A.WorldPos(), A.WorldDir(), B.WorldPos(), B.WorldDir(), Mid);
	}

	public string MidAxis()
	{
		PointF pointF = EffectiveDir(A.WorldPos(), A.WorldDir(), B.WorldPos());
		PointF pointF2 = EffectiveDir(B.WorldPos(), B.WorldDir(), A.WorldPos());
		bool flag = Math.Abs(pointF.X) > 0.5f;
		bool flag2 = Math.Abs(pointF2.X) > 0.5f;
		if (flag && flag2)
		{
			return "X";
		}
		if (!flag && !flag2)
		{
			return "Y";
		}
		return null;
	}

	private static PointF EffectiveDir(PointF p, PointF d, PointF other)
	{
		if (d.X != 0f || d.Y != 0f)
		{
			return d;
		}
		float value = other.X - p.X;
		float num = other.Y - p.Y;
		PointF result = ((!(Math.Abs(value) > Math.Abs(num))) ? new PointF(0f, (num == 0f) ? 1 : Math.Sign(num)) : new PointF(Math.Sign(value), 0f));
		return result;
	}

	public static PointF[] RouteBetween(PointF a, PointF da, PointF b, PointF db, float? mid = null)
	{
		float num = ((da.X == 0f && da.Y == 0f) ? 0f : 10f);
		float num2 = ((db.X == 0f && db.Y == 0f) ? 0f : 10f);
		da = EffectiveDir(a, da, b);
		db = EffectiveDir(b, db, a);
		PointF item = new PointF(a.X + da.X * num, a.Y + da.Y * num);
		PointF item2 = new PointF(b.X + db.X * num2, b.Y + db.Y * num2);
		bool flag = Math.Abs(da.X) > 0.5f;
		bool flag2 = Math.Abs(db.X) > 0.5f;
		List<PointF> list = new List<PointF> { a, item };
		if (flag && flag2)
		{
			float x = mid ?? ((item.X + item2.X) / 2f);
			list.Add(new PointF(x, item.Y));
			list.Add(new PointF(x, item2.Y));
		}
		else if (!flag && !flag2)
		{
			float y = mid ?? ((item.Y + item2.Y) / 2f);
			list.Add(new PointF(item.X, y));
			list.Add(new PointF(item2.X, y));
		}
		else if (flag)
		{
			list.Add(new PointF(item2.X, item.Y));
		}
		else
		{
			list.Add(new PointF(item.X, item2.Y));
		}
		list.Add(item2);
		list.Add(b);
		PointF[] array = Simplify(list);
		if (array.Length < 2)
		{
			return new PointF[2] { a, b };
		}
		return array;
	}

	private static PointF[] Simplify(List<PointF> pts)
	{
		List<PointF> list = new List<PointF>();
		checked
		{
			foreach (PointF pt in pts)
			{
				if (list.Count > 0 && Near(list[list.Count - 1], pt))
				{
					continue;
				}
				if (list.Count >= 2)
				{
					PointF pointF = list[list.Count - 2];
					PointF pointF2 = list[list.Count - 1];
					if (((double)Math.Abs(pointF.X - pointF2.X) < 0.01 && (double)Math.Abs(pointF2.X - pt.X) < 0.01) || ((double)Math.Abs(pointF.Y - pointF2.Y) < 0.01 && (double)Math.Abs(pointF2.Y - pt.Y) < 0.01))
					{
						list[list.Count - 1] = pt;
						continue;
					}
				}
				list.Add(pt);
			}
			return list.ToArray();
		}
	}

	private static bool Near(PointF a, PointF b)
	{
		if ((double)Math.Abs(a.X - b.X) < 0.01)
		{
			return (double)Math.Abs(a.Y - b.Y) < 0.01;
		}
		return false;
	}

	public float DistanceTo(PointF p)
	{
		PointF[] array = Route();
		float num = float.MaxValue;
		checked
		{
			int num2 = array.Length - 2;
			for (int i = 0; i <= num2; i++)
			{
				num = Math.Min(num, SegmentDistance(p, array[i], array[i + 1]));
			}
			return num;
		}
	}

	private static float SegmentDistance(PointF p, PointF a, PointF b)
	{
		float num = b.X - a.X;
		float num2 = b.Y - a.Y;
		float num3 = num * num + num2 * num2;
		float val = ((num3 < 0.0001f) ? 0f : (((p.X - a.X) * num + (p.Y - a.Y) * num2) / num3));
		val = Math.Max(0f, Math.Min(1f, val));
		float num4 = a.X + val * num - p.X;
		float num5 = a.Y + val * num2 - p.Y;
		return (float)Math.Sqrt(num4 * num4 + num5 * num5);
	}
}
