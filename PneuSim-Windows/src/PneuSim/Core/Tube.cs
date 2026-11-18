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

	internal Circuit Circuit { get; set; }

	internal PointF[] DefaultRoute()
	{
		return RouteBetween(A.WorldPos(), A.WorldDir(), B.WorldPos(), B.WorldDir(), Mid);
	}

	private static float OverlapLength(PointF p, PointF q, List<PointF[]> sibs)
	{
		float total = 0f;
		bool vertical = Math.Abs(p.X - q.X) < 0.01f;
		bool horizontal = Math.Abs(p.Y - q.Y) < 0.01f;
		if (!vertical && !horizontal)
		{
			return 0f;
		}
		foreach (PointF[] r in sibs)
		{
			for (int i = 0; i < r.Length - 1; i++)
			{
				PointF c = r[i];
				PointF d = r[i + 1];
				if (vertical && Math.Abs(c.X - d.X) < 0.01f && Math.Abs(c.X - p.X) < 1f)
				{
					float o = Math.Min(Math.Max(p.Y, q.Y), Math.Max(c.Y, d.Y)) - Math.Max(Math.Min(p.Y, q.Y), Math.Min(c.Y, d.Y));
					if (o > 2f)
					{
						total += o;
					}
				}
				else if (horizontal && Math.Abs(c.Y - d.Y) < 0.01f && Math.Abs(c.Y - p.Y) < 1f)
				{
					float o2 = Math.Min(Math.Max(p.X, q.X), Math.Max(c.X, d.X)) - Math.Max(Math.Min(p.X, q.X), Math.Min(c.X, d.X));
					if (o2 > 2f)
					{
						total += o2;
					}
				}
			}
		}
		return total;
	}

	private static float OverlapTotal(PointF[] r, List<PointF[]> sibs)
	{
		float total = 0f;
		for (int i = 1; i < r.Length - 2; i++)
		{
			total += OverlapLength(r[i], r[i + 1], sibs);
		}
		return total;
	}

	/// <summary>The line of this tube: the neat standard route, or, when that would run through a component or lie on another line, an automatic route around them (computed for all tubes together by the circuit). A line dragged by hand is kept.</summary>
	public PointF[] Route()
	{
		return (Circuit != null) ? Circuit.RouteOf(this) : DefaultRoute();
	}

	internal PointF[] Compute(List<RectangleF> obs, List<PointF[]> sibs)
	{
		PointF[] def = DefaultRoute();
		if (Mid.HasValue || def.Length < 4)
		{
			return def;
		}
		if (CountHits(def, obs) == 0 && OverlapTotal(def, sibs) < 6f)
		{
			return def;
		}
		PointF[] found = null;
		try
		{
			found = FindRoute(obs, sibs);
		}
		catch (Exception)
		{
			found = null;
		}
		return found ?? def;
	}

	private static int CountHits(PointF[] r, List<RectangleF> obs)
	{
		int n = 0;
		for (int i = 1; i < r.Length - 2; i++)
		{
			RectangleF seg = RectangleF.FromLTRB(Math.Min(r[i].X, r[i + 1].X) - 0.5f, Math.Min(r[i].Y, r[i + 1].Y) - 0.5f, Math.Max(r[i].X, r[i + 1].X) + 0.5f, Math.Max(r[i].Y, r[i + 1].Y) + 0.5f);
			foreach (RectangleF o in obs)
			{
				RectangleF b = o;
				b.Inflate(-2f, -2f);
				if (b.Width > 0f && b.Height > 0f && b.IntersectsWith(seg))
				{
					n++;
				}
			}
		}
		return n;
	}

	private static readonly int[] Dx = new int[4] { 1, 0, -1, 0 };

	private static readonly int[] Dy = new int[4] { 0, 1, 0, -1 };

	private static int DirIndex(PointF d)
	{
		if (Math.Abs(d.X) > 0.5f)
		{
			return (d.X > 0f) ? 0 : 2;
		}
		return (d.Y > 0f) ? 1 : 3;
	}

	/// <summary>Shortest neat (horizontal and vertical lines only) route between the two stub ends that avoids every obstacle, with few bends.</summary>
	private PointF[] FindRoute(List<RectangleF> obs, List<PointF[]> sibs)
	{
		PointF a = A.WorldPos();
		PointF b = B.WorldPos();
		PointF da = EffectiveDir(a, A.WorldDir(), b);
		PointF db = EffectiveDir(b, B.WorldDir(), a);
		float la = (A.WorldDir().X == 0f && A.WorldDir().Y == 0f) ? 0f : 10f;
		float lb = (B.WorldDir().X == 0f && B.WorldDir().Y == 0f) ? 0f : 10f;
		PointF sa = new PointF(a.X + da.X * la, a.Y + da.Y * la);
		PointF sb = new PointF(b.X + db.X * lb, b.Y + db.Y * lb);
		SortedSet<float> xset = new SortedSet<float> { sa.X, sb.X };
		SortedSet<float> yset = new SortedSet<float> { sa.Y, sb.Y };
		foreach (RectangleF r in obs)
		{
			xset.Add(r.Left - 6f);
			xset.Add(r.Right + 6f);
			yset.Add(r.Top - 6f);
			yset.Add(r.Bottom + 6f);
		}
		foreach (PointF[] r in sibs)
		{
			for (int i = 0; i < r.Length - 1; i++)
			{
				if (Math.Abs(r[i].X - r[i + 1].X) < 0.01f)
				{
					xset.Add(r[i].X + 12f);
					xset.Add(r[i].X - 12f);
				}
				else if (Math.Abs(r[i].Y - r[i + 1].Y) < 0.01f)
				{
					yset.Add(r[i].Y + 12f);
					yset.Add(r[i].Y - 12f);
				}
			}
		}
		List<float> xs = Dedup(xset);
		List<float> ys = Dedup(yset);
		int nx = xs.Count;
		int ny = ys.Count;
		int si = xs.FindIndex((float v) => Math.Abs(v - sa.X) < 0.05f);
		int sj = ys.FindIndex((float v) => Math.Abs(v - sa.Y) < 0.05f);
		int gi = xs.FindIndex((float v) => Math.Abs(v - sb.X) < 0.05f);
		int gj = ys.FindIndex((float v) => Math.Abs(v - sb.Y) < 0.05f);
		if (si < 0 || sj < 0 || gi < 0 || gj < 0)
		{
			return null;
		}
		bool Free(int i, int j)
		{
			if ((i == si && j == sj) || (i == gi && j == gj))
			{
				return true;
			}
			foreach (RectangleF r in obs)
			{
				if (xs[i] > r.Left && xs[i] < r.Right && ys[j] > r.Top && ys[j] < r.Bottom)
				{
					return false;
				}
			}
			return true;
		}
		bool SegmentFree(int i1, int j1, int i2, int j2)
		{
			RectangleF seg = RectangleF.FromLTRB(Math.Min(xs[i1], xs[i2]), Math.Min(ys[j1], ys[j2]), Math.Max(xs[i1], xs[i2]), Math.Max(ys[j1], ys[j2]));
			foreach (RectangleF r in obs)
			{
				RectangleF b2 = r;
				b2.Inflate(-1f, -1f);
				if (b2.Width <= 0f || b2.Height <= 0f)
				{
					continue;
				}
				if (seg.Right > b2.Left && seg.Left < b2.Right && seg.Bottom > b2.Top && seg.Top < b2.Bottom)
				{
					return false;
				}
				if (seg.Width == 0f && seg.X > b2.Left && seg.X < b2.Right && seg.Bottom > b2.Top && seg.Top < b2.Bottom)
				{
					return false;
				}
				if (seg.Height == 0f && seg.Y > b2.Top && seg.Y < b2.Bottom && seg.Right > b2.Left && seg.Left < b2.Right)
				{
					return false;
				}
			}
			return true;
		}
		int total = nx * ny * 4;
		float[] dist = new float[total];
		int[] prev = new int[total];
		for (int k = 0; k < total; k++)
		{
			dist[k] = float.MaxValue;
			prev[k] = -1;
		}
		int startDir = DirIndex(da);
		int goalBack = DirIndex(db);
		SortedSet<(float, int)> queue = new SortedSet<(float, int)>();
		int start = (si * ny + sj) * 4 + startDir;
		dist[start] = 0f;
		queue.Add((0f, start));
		int goal = -1;
		while (queue.Count > 0)
		{
			(float, int) top = queue.Min;
			queue.Remove(top);
			int state = top.Item2;
			if (top.Item1 > dist[state])
			{
				continue;
			}
			int dir = state % 4;
			int cell = state / 4;
			int ci = cell / ny;
			int cj = cell % ny;
			if (ci == gi && cj == gj && dir != goalBack)
			{
				goal = state;
				break;
			}
			for (int nd = 0; nd < 4; nd++)
			{
				if (nd == (dir + 2) % 4)
				{
					continue;
				}
				int ni = ci + Dx[nd];
				int nj = cj + Dy[nd];
				if (ni < 0 || nj < 0 || ni >= nx || nj >= ny || !Free(ni, nj) || !SegmentFree(ci, cj, ni, nj))
				{
					continue;
				}
				float len = Math.Abs(xs[ni] - xs[ci]) + Math.Abs(ys[nj] - ys[cj]);
				float cost = top.Item1 + len + ((nd != dir) ? 30f : 0f) + 3f * OverlapLength(new PointF(xs[ci], ys[cj]), new PointF(xs[ni], ys[nj]), sibs);
				int ns = (ni * ny + nj) * 4 + nd;
				if (cost < dist[ns])
				{
					dist[ns] = cost;
					prev[ns] = state;
					queue.Add((cost, ns));
				}
			}
		}
		if (goal < 0)
		{
			return null;
		}
		List<PointF> path = new List<PointF>();
		for (int st = goal; st >= 0; st = prev[st])
		{
			int cell = st / 4;
			path.Add(new PointF(xs[cell / ny], ys[cell % ny]));
		}
		path.Reverse();
		List<PointF> all = new List<PointF> { a };
		all.AddRange(path);
		all.Add(b);
		PointF[] result = Simplify(all);
		return (result.Length < 2) ? null : result;
	}

	private static List<float> Dedup(SortedSet<float> set)
	{
		List<float> list = new List<float>();
		foreach (float v in set)
		{
			if (list.Count == 0 || v - list[list.Count - 1] > 0.05f)
			{
				list.Add(v);
			}
		}
		return list;
	}

	private int Collisions(PointF[] r, IEnumerable<CircuitElement> obstacles)
	{
		int n = 0;
		for (int i = 1; i < r.Length - 2; i++)
		{
			RectangleF seg = RectangleF.FromLTRB(Math.Min(r[i].X, r[i + 1].X) - 0.5f, Math.Min(r[i].Y, r[i + 1].Y) - 0.5f, Math.Max(r[i].X, r[i + 1].X) + 0.5f, Math.Max(r[i].Y, r[i + 1].Y) + 0.5f);
			foreach (CircuitElement e in obstacles)
			{
				RectangleF b = e.WorldBounds();
				b.Inflate(-2f, -2f);
				if (b.Width > 0f && b.Height > 0f && b.IntersectsWith(seg))
				{
					n++;
				}
			}
		}
		return n;
	}

	/// <summary>Picks the middle-segment position so the line does not run through components and is short. With force the manual position is discarded first; otherwise it is only changed when the current line crosses a component.</summary>
	public void AutoRoute(IEnumerable<CircuitElement> obstacles, bool force)
	{
		if (force)
		{
			Mid = null;
		}
		else if (Mid.HasValue && Collisions(Route(), new List<CircuitElement>(obstacles)) > 0)
		{
			Mid = null;
		}
	}

	private static float Length(PointF[] r)
	{
		float t = 0f;
		for (int i = 0; i < r.Length - 1; i++)
		{
			t += Math.Abs(r[i + 1].X - r[i].X) + Math.Abs(r[i + 1].Y - r[i].Y);
		}
		return t;
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
