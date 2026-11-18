using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class CutawayView : Control
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__19_002D0
	{
		public float _0024VB_0024Local_left;

		public DirectionalValve _0024VB_0024Local_v;

		public CutawayPalette _0024VB_0024Local_pal;

		public _Closure_0024__19_002D0(_Closure_0024__19_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_left = arg0._0024VB_0024Local_left;
				_0024VB_0024Local_v = arg0._0024VB_0024Local_v;
				_0024VB_0024Local_pal = arg0._0024VB_0024Local_pal;
			}
		}

		[SpecialName]
		internal float _Lambda_0024__0(float offset)
		{
			return _0024VB_0024Local_left + 60f + offset * 3.2f;
		}

		[SpecialName]
		internal Color _Lambda_0024__1(string name)
		{
			return Cutaways.Fluid(_0024VB_0024Local_v.GetPort(name), _0024VB_0024Local_pal);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__19_002D1
	{
		public string[] _0024VB_0024Local_fl;

		public _Closure_0024__19_002D1(_Closure_0024__19_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_fl = arg0._0024VB_0024Local_fl;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__3((string Name, float Offset, bool Top, bool Vents) p)
		{
			return Operators.CompareString(p.Name, _0024VB_0024Local_fl[0], TextCompare: false) == 0;
		}

		[SpecialName]
		internal bool _Lambda_0024__4((string Name, float Offset, bool Top, bool Vents) p)
		{
			return Operators.CompareString(p.Name, _0024VB_0024Local_fl[1], TextCompare: false) == 0;
		}
	}

	private CircuitElement _element;

	private float _shift;

	private double _phase;

	private const float ViewScale = 3.2f;

	public bool Simulating { get; set; }

	public CircuitElement Element
	{
		get
		{
			return _element;
		}
		set
		{
			if (value != _element)
			{
				_shift = TargetShift(value as DirectionalValve);
			}
			_element = value;
		}
	}

	public DirectionalValve Valve
	{
		get
		{
			return _element as DirectionalValve;
		}
		set
		{
			Element = value;
		}
	}

	public CutawayView()
	{
		SetStyle(ControlStyles.UserPaint | ControlStyles.ResizeRedraw | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer, value: true);
	}

	public static float TargetShift(DirectionalValve v)
	{
		if (v == null)
		{
			return 0f;
		}
		float num = v.BoxSize * 3.2f * 0.45f;
		return (v.State == 1) ? num : ((v.State == 2) ? (0f - num) : 0f);
	}

	public void Animate(double dt)
	{
		float num = TargetShift(_element as DirectionalValve);
		float num2 = (float)(260.0 * dt);
		if (Math.Abs(num - _shift) <= num2)
		{
			_shift = num;
		}
		else
		{
			_shift += (float)Math.Sign(num - _shift) * num2;
		}
		double value = 0.0;
		if (_element is AirMotor)
		{
			value = ((AirMotor)_element).Rpm;
		}
		if (_element is HydraulicMotor)
		{
			value = ((HydraulicMotor)_element).Rpm;
		}
		if (_element is HydraulicPump && Simulating && ((HydraulicPump)_element).Running)
		{
			value = 60.0;
		}
		_phase = (_phase + (double)Math.Sign(value) * Math.Min(Math.Abs(value), 120.0) * 3.0 * dt) % 360.0;
		Invalidate();
	}

	protected override void OnPaint(PaintEventArgs e)
	{
		Render(e.Graphics);
	}

	public void Render(Graphics g)
	{
		g.SmoothingMode = SmoothingMode.AntiAlias;
		CutawayPalette cutawayPalette = CutawayPalette.Current();
		g.Clear(cutawayPalette.Back);
		checked
		{
			using Font font = new Font("Segoe UI", 8.5f);
			using Font font2 = new Font("Segoe UI", 9f, FontStyle.Bold);
			using SolidBrush brush2 = new SolidBrush(cutawayPalette.Text);
			using SolidBrush brush = new SolidBrush(cutawayPalette.SubText);
			if (_element == null)
			{
				g.DrawString("Select a component (or hover over one) to see inside it.\r\nStart the simulation to watch the parts move.", font, brush, 12f, 12f);
				return;
			}
			g.DrawString(string.Format("{0}{1}", string.IsNullOrEmpty(_element.Label) ? "" : (_element.Label + ": "), _element.DisplayName), font2, brush2, 8f, 6f);
			if (_element is DirectionalValve)
			{
				DrawValve(g, (DirectionalValve)_element, cutawayPalette, font, font2);
			}
			else
			{
				RectangleF area = new RectangleF(8f, 44f, base.Width - 16, base.Height - 44 - 24);
				string s = Cutaways.Draw(g, _element, area, cutawayPalette, _phase, Simulating);
				g.DrawString(s, font, brush, 8f, 24f);
			}
			g.DrawString("Coloured = under pressure (blue air, orange oil, red current)   Pale = vented or no pressure", font, brush, 8f, base.Height - 20);
		}
	}

	private void DrawValve(Graphics g, DirectionalValve v, CutawayPalette pal, Font f, Font fb)
	{
		DrawValveCutaway(g, v, pal, f, fb, base.Width, _shift);
	}

	/// <summary>Draws the inside of a directional valve in a virtual space viewWidth wide (about 215 high); shift is the current spool position.</summary>
	public static void DrawValveCutaway(Graphics g, DirectionalValve v, CutawayPalette pal, Font f, Font fb, float viewWidth, float shift)
	{
		_Closure_0024__19_002D0 arg = default(_Closure_0024__19_002D0);
		_Closure_0024__19_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__19_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_v = v;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_pal = pal;
		(string, float, bool, bool)[] array = CS_0024_003C_003E8__locals0._0024VB_0024Local_v.WorkingPortLayout();
		float num = CS_0024_003C_003E8__locals0._0024VB_0024Local_v.BoxSize * 3.2f + 120f;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_left = (viewWidth - num) / 2f;
		float num2 = 70f;
		float num3 = num2 + 34f;
		float num4 = num3 + 36f;
		float num5 = num4 + 34f;
		VB_0024AnonymousDelegate_23<float, float> vB_0024AnonymousDelegate_ = [SpecialName] (float offset) => CS_0024_003C_003E8__locals0._0024VB_0024Local_left + 60f + offset * 3.2f;
		VB_0024AnonymousDelegate_24<string, Color> vB_0024AnonymousDelegate_2 = [SpecialName] (string name) => Cutaways.Fluid(CS_0024_003C_003E8__locals0._0024VB_0024Local_v.GetPort(name), CS_0024_003C_003E8__locals0._0024VB_0024Local_pal);
		checked
		{
			using SolidBrush brush4 = new SolidBrush(CS_0024_003C_003E8__locals0._0024VB_0024Local_pal.Text);
			using SolidBrush brush = new SolidBrush(CS_0024_003C_003E8__locals0._0024VB_0024Local_pal.SubText);
			using Pen pen = new Pen(CS_0024_003C_003E8__locals0._0024VB_0024Local_pal.Edge);
			string arg2 = ((CS_0024_003C_003E8__locals0._0024VB_0024Local_v.State == 0) ? "normal position" : ((CS_0024_003C_003E8__locals0._0024VB_0024Local_v.State == 1) ? "position a" : "position b"));
			string[][] array2 = CS_0024_003C_003E8__locals0._0024VB_0024Local_v.PassagesIn(CS_0024_003C_003E8__locals0._0024VB_0024Local_v.State);
			g.DrawString($"Now in {arg2}: " + ((array2.Length == 0) ? "all ports closed" : string.Join(",  ", array2.Select([SpecialName] (string[] fl) => $"{fl[0]} → {fl[1]}"))), f, brush, 8f, 24f);
			using (SolidBrush brush2 = new SolidBrush(CS_0024_003C_003E8__locals0._0024VB_0024Local_pal.Metal))
			{
				g.FillRectangle(brush2, CS_0024_003C_003E8__locals0._0024VB_0024Local_left, num2, num, num5 - num2);
			}
			g.DrawRectangle(pen, CS_0024_003C_003E8__locals0._0024VB_0024Local_left, num2, num, num5 - num2);
			(string, float, bool, bool)[] array3 = array;
			for (int i = 0; i < array3.Length; i++)
			{
				(string, float, bool, bool) tuple = array3[i];
				float num6 = vB_0024AnonymousDelegate_(tuple.Item2);
				using (SolidBrush brush3 = new SolidBrush(vB_0024AnonymousDelegate_2(tuple.Item1)))
				{
					if (tuple.Item3)
					{
						g.FillRectangle(brush3, num6 - 7f, num2 - 12f, 14f, num3 - num2 + 12f);
					}
					else
					{
						g.FillRectangle(brush3, num6 - 7f, num4, 14f, num5 - num4 + 12f);
					}
				}
				g.DrawString(tuple.Item1, fb, brush4, num6 - 4f, tuple.Item3 ? (num2 - 28f) : (num5 + 12f));
				if (tuple.Item4)
				{
					g.DrawString("exhaust", f, brush, num6 - 18f, num5 + 26f);
				}
			}
			using (SolidBrush brush5 = new SolidBrush(CS_0024_003C_003E8__locals0._0024VB_0024Local_pal.Cavity))
			{
				g.FillRectangle(brush5, CS_0024_003C_003E8__locals0._0024VB_0024Local_left + 10f, num3, num - 20f, num4 - num3);
			}
			float num7 = TargetShift(CS_0024_003C_003E8__locals0._0024VB_0024Local_v);
			float num8 = shift - num7;
			using (SolidBrush brush6 = new SolidBrush(CS_0024_003C_003E8__locals0._0024VB_0024Local_pal.MetalDark))
			{
				g.FillRectangle(brush6, CS_0024_003C_003E8__locals0._0024VB_0024Local_left + 14f + shift, num3 + 2f, num - 28f, num4 - num3 - 4f);
			}
			string[][] array4 = array2;
			_Closure_0024__19_002D1 closure_0024__19_002D = default(_Closure_0024__19_002D1);
			for (int j = 0; j < array4.Length; j++)
			{
				closure_0024__19_002D = new _Closure_0024__19_002D1(closure_0024__19_002D);
				closure_0024__19_002D._0024VB_0024Local_fl = array4[j];
				(string, float, bool, bool) tuple2 = array.FirstOrDefault(closure_0024__19_002D._Lambda_0024__3);
				(string, float, bool, bool) tuple3 = array.FirstOrDefault(closure_0024__19_002D._Lambda_0024__4);
				if (tuple2.Item1 != null && tuple3.Item1 != null)
				{
					float num9 = Math.Min(vB_0024AnonymousDelegate_(tuple2.Item2), vB_0024AnonymousDelegate_(tuple3.Item2)) - 9f + num8;
					float num10 = Math.Max(vB_0024AnonymousDelegate_(tuple2.Item2), vB_0024AnonymousDelegate_(tuple3.Item2)) + 9f + num8;
					using (SolidBrush brush7 = new SolidBrush(vB_0024AnonymousDelegate_2(closure_0024__19_002D._0024VB_0024Local_fl[0])))
					{
						g.FillRectangle(brush7, num9, num3 + 6f, num10 - num9, num4 - num3 - 12f);
					}
					using Pen pen2 = new Pen(Color.White, 2f)
					{
						EndCap = LineCap.ArrowAnchor
					};
					float num11 = (num3 + num4) / 2f;
					g.DrawLine(pen2, vB_0024AnonymousDelegate_(tuple2.Item2) + num8, num11, vB_0024AnonymousDelegate_(tuple3.Item2) + num8, num11);
				}
			}
			float num12 = (num3 + num4) / 2f;
			using (SolidBrush brush8 = new SolidBrush(CS_0024_003C_003E8__locals0._0024VB_0024Local_pal.MetalDark))
			{
				g.FillRectangle(brush8, CS_0024_003C_003E8__locals0._0024VB_0024Local_left - 18f + shift, num12 - 4f, 32f, 8f);
				g.FillRectangle(brush8, CS_0024_003C_003E8__locals0._0024VB_0024Local_left + num - 14f + shift, num12 - 4f, 32f, 8f);
			}
			bool flag = CS_0024_003C_003E8__locals0._0024VB_0024Local_v.Positions == 3;
			bool flag2 = CS_0024_003C_003E8__locals0._0024VB_0024Local_v.Positions == 3 || CS_0024_003C_003E8__locals0._0024VB_0024Local_v.ReturnType == ValveReturn.Spring;
			using (Pen pen3 = new Pen(CS_0024_003C_003E8__locals0._0024VB_0024Local_pal.Spring, 1.5f))
			{
				if (flag2)
				{
					Cutaways.Zigzag(g, pen3, CS_0024_003C_003E8__locals0._0024VB_0024Local_left + num + 18f + shift, CS_0024_003C_003E8__locals0._0024VB_0024Local_left + num + 64f, num12, 8f);
				}
				if (flag)
				{
					Cutaways.Zigzag(g, pen3, CS_0024_003C_003E8__locals0._0024VB_0024Local_left - 64f, CS_0024_003C_003E8__locals0._0024VB_0024Local_left - 18f + shift, num12, 8f);
				}
			}
			string text;
			switch (CS_0024_003C_003E8__locals0._0024VB_0024Local_v.Actuator)
			{
			case ValveActuator.Solenoid:
				text = $"solenoid {CS_0024_003C_003E8__locals0._0024VB_0024Local_v.SolenoidLabel}";
				break;
			case ValveActuator.Pilot:
			case ValveActuator.DelayedPilot:
				text = $"pilot {CS_0024_003C_003E8__locals0._0024VB_0024Local_v.PilotPortName(leftSide: true)}";
				break;
			case ValveActuator.RollerLever:
				text = $"roller {CS_0024_003C_003E8__locals0._0024VB_0024Local_v.TriggerMark}";
				break;
			case ValveActuator.IdleReturnRoller:
				text = $"idle-return roller {CS_0024_003C_003E8__locals0._0024VB_0024Local_v.TriggerMark}";
				break;
			case ValveActuator.Selector:
				text = "selector";
				break;
			default:
				text = "push button";
				break;
			}
			string text2 = ((CS_0024_003C_003E8__locals0._0024VB_0024Local_v.ReturnType == ValveReturn.Spring) ? "spring" : ((CS_0024_003C_003E8__locals0._0024VB_0024Local_v.ReturnType == ValveReturn.Solenoid) ? $"solenoid {CS_0024_003C_003E8__locals0._0024VB_0024Local_v.ReturnSolenoidLabel}" : $"pilot {CS_0024_003C_003E8__locals0._0024VB_0024Local_v.PilotPortName(leftSide: false)}"));
			if (CS_0024_003C_003E8__locals0._0024VB_0024Local_v.Positions == 3)
			{
				text2 += " + centring springs";
			}
			SizeF sizeF = g.MeasureString(text, f);
			g.DrawString(text, f, brush4, Math.Max(2f, CS_0024_003C_003E8__locals0._0024VB_0024Local_left - 4f - sizeF.Width), num3 - 6f - sizeF.Height);
			g.DrawString(text2, f, brush4, Math.Min(viewWidth - g.MeasureString(text2, f).Width - 2f, CS_0024_003C_003E8__locals0._0024VB_0024Local_left + num + 4f), num3 - 6f - sizeF.Height);
		}
	}
}
