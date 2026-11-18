using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class CalculatorsDialog : Form
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__2_002D0
	{
		public TextBox _0024VB_0024Local_result;

		public Func<double[], string> _0024VB_0024Local_calc;

		public List<NumericUpDown> _0024VB_0024Local_boxes;

		public VB_0024AnonymousDelegate_22 _0024VB_0024Local_update;

		public VB_0024AnonymousDelegate_22 _0024I2;

		public EventHandler _0024IR1;

		public _Closure_0024__2_002D0(_Closure_0024__2_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_result = arg0._0024VB_0024Local_result;
				_0024VB_0024Local_calc = arg0._0024VB_0024Local_calc;
				_0024VB_0024Local_boxes = arg0._0024VB_0024Local_boxes;
				_0024VB_0024Local_update = arg0._0024VB_0024Local_update;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0()
		{
			try
			{
				_0024VB_0024Local_result.Text = _0024VB_0024Local_calc(_0024VB_0024Local_boxes.Select([SpecialName] (NumericUpDown b) => Convert.ToDouble(b.Value)).ToArray());
			}
			catch (Exception ex)
			{
				ProjectData.SetProjectError(ex);
				Exception ex2 = ex;
				_0024VB_0024Local_result.Text = ex2.Message;
				ProjectData.ClearProjectError();
			}
		}

		[SpecialName]
		internal void _Lambda_0024__R1(object a0, EventArgs a1)
		{
			((VB_0024AnonymousDelegate_22)([SpecialName] () =>
			{
				_0024VB_0024Local_update();
			}))();
		}

		[SpecialName]
		internal void _Lambda_0024__2()
		{
			_0024VB_0024Local_update();
		}
	}

	private readonly TabControl _tabs;

	public CalculatorsDialog()
	{
		_tabs = new TabControl
		{
			Dock = DockStyle.Fill
		};
		Text = "Calculators";
		Font = new Font("Segoe UI", 9f);
		base.Size = new Size(560, 470);
		base.StartPosition = FormStartPosition.CenterParent;
		base.ShowInTaskbar = false;
		base.Controls.Add(_tabs);
		base.KeyPreview = true;
		base.KeyDown += [SpecialName] (object s, KeyEventArgs e) =>
		{
			if (e.KeyCode == Keys.Escape)
			{
				Close();
			}
		};
		AddCalculator("Cylinder force", new(string, double)[4]
		{
			("Bore (mm)", 32.0),
			("Rod diameter (mm)", 12.0),
			("Pressure (bar)", 6.0),
			("Efficiency (%)", 90.0)
		}, [SpecialName] (double[] v) =>
		{
			double num18 = Math.PI / 4.0 * Math.Pow(v[0] / 1000.0, 2.0);
			double num19 = Math.PI / 4.0 * (Math.Pow(v[0] / 1000.0, 2.0) - Math.Pow(v[1] / 1000.0, 2.0));
			double num20 = v[3] / 100.0;
			double num21 = v[2] * 100000.0 * num18 * num20;
			double num22 = v[2] * 100000.0 * num19 * num20;
			return $"Piston area: {num18 * 10000.0:0.00} cm²   Annulus area: {num19 * 10000.0:0.00} cm²" + "\r\n\r\n" + $"Extending force:  {num21:0} N  ({num21 / 9.81:0.0} kgf)" + "\r\n" + $"Retracting force: {num22:0} N  ({num22 / 9.81:0.0} kgf)" + "\r\n\r\nF = p × A × efficiency.  1 bar = 100 000 N/m².\r\nChoose a cylinder about 1.5–2 × larger than the load for reliable speed.";
		});
		AddCalculator("Air consumption", new(string, double)[8]
		{
			("Bore (mm)", 32.0),
			("Rod diameter (mm)", 12.0),
			("Stroke (mm)", 100.0),
			("Pressure (bar)", 6.0),
			("Cycles per minute", 10.0),
			("Air cost (₹ per m³)", 2.0),
			("Hours per day", 8.0),
			("Days per year", 300.0)
		}, [SpecialName] (double[] v) =>
		{
			double num11 = v[2] / 1000.0;
			double num12 = (v[3] + 1.013) / 1.013;
			double num13 = Math.PI / 4.0 * Math.Pow(v[0] / 1000.0, 2.0) * num11 * num12 * 1000.0;
			double num14 = Math.PI / 4.0 * (Math.Pow(v[0] / 1000.0, 2.0) - Math.Pow(v[1] / 1000.0, 2.0)) * num11 * num12 * 1000.0;
			double num15 = num13 + num14;
			double num16 = num15 * v[4];
			double num17 = num16 * 60.0 * v[6] * v[7] / 1000.0;
			return $"Free air per cycle: {num15:0.00} NL  (extend {num13:0.00} + retract {num14:0.00})" + "\r\n" + $"Consumption: {num16:0.0} NL/min" + "\r\n" + $"Per year: {num17:#,##0} m³ → about ₹{num17 * v[5]:#,##0}" + "\r\n\r\nFree air = swept volume × (p + 1.013) / 1.013.  Add 10–20 % for tubing and leaks.";
		});
		AddCalculator("Valve size", new(string, double)[3]
		{
			("Bore (mm)", 32.0),
			("Desired speed (m/s)", 0.5),
			("Pressure (bar)", 6.0)
		}, [SpecialName] (double[] v) =>
		{
			double num7 = Math.PI / 4.0 * Math.Pow(v[0] / 1000.0, 2.0);
			double num8 = (v[2] + 1.013) / 1.013;
			double num9 = num7 * v[1] * num8 * 60000.0;
			double num10 = num9 * 1.2;
			return $"Flow needed while moving: {num9:0} NL/min" + "\r\n" + $"Choose a valve with nominal flow Qn ≥ {num10:0} NL/min" + "\r\n" + $"   ≈ Cv {num10 / 981.0:0.00}   ≈ Kv {num10 / 1078.0:0.00}   ≈ sonic conductance C {num10 / 60.0 / 7.0 / 0.96:0.00} NL/(s·bar)" + "\r\n\r\nQn is the flow at 6 bar inlet and 1 bar pressure drop (catalogue value).\r\nRule of thumb: Qn ≈ 981 × Cv ≈ 1078 × Kv (l/min).";
		});
		AddCalculator("Hydraulics", new(string, double)[5]
		{
			("Bore (mm)", 40.0),
			("Rod diameter (mm)", 22.0),
			("Pump flow (l/min)", 8.0),
			("Pressure (bar)", 60.0),
			("Stroke (mm)", 200.0)
		}, [SpecialName] (double[] v) =>
		{
			double num = Math.PI / 4.0 * Math.Pow(v[0] / 1000.0, 2.0);
			double num2 = Math.PI / 4.0 * (Math.Pow(v[0] / 1000.0, 2.0) - Math.Pow(v[1] / 1000.0, 2.0));
			double num3 = v[2] / 60000.0;
			double num4 = num3 / num;
			double num5 = num3 / num2;
			double num6 = v[3] * v[2] / 600.0;
			return $"Extending speed: {num4:0.000} m/s  ({v[4] / 1000.0 / num4:0.00} s per stroke)" + "\r\n" + $"Retracting speed: {num5:0.000} m/s  ({v[4] / 1000.0 / num5:0.00} s per stroke)" + "\r\n" + $"Extending force: {v[3] * 100000.0 * num / 1000.0:0.0} kN   Retracting: {v[3] * 100000.0 * num2 / 1000.0:0.0} kN" + "\r\n" + $"Hydraulic power: {num6:0.00} kW   (drive motor ≈ {num6 / 0.85:0.00} kW)" + "\r\n\r\nv = Q / A,  F = p × A,  P (kW) = p (bar) × Q (l/min) / 600.";
		});
	}

	private void AddCalculator(string title, (string Label, double Value)[] inputs, Func<double[], string> calc)
	{
		_Closure_0024__2_002D0 arg = default(_Closure_0024__2_002D0);
		_Closure_0024__2_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__2_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_calc = calc;
		TabPage tabPage = new TabPage(title)
		{
			Padding = new Padding(10)
		};
		TableLayoutPanel tableLayoutPanel = new TableLayoutPanel
		{
			Dock = DockStyle.Top,
			AutoSize = true,
			ColumnCount = 2
		};
		CS_0024_003C_003E8__locals0._0024VB_0024Local_boxes = new List<NumericUpDown>();
		CS_0024_003C_003E8__locals0._0024VB_0024Local_result = new TextBox
		{
			Multiline = true,
			ReadOnly = true,
			Dock = DockStyle.Fill,
			Font = new Font("Consolas", 9.5f),
			BackColor = Color.White,
			ScrollBars = ScrollBars.Vertical
		};
		CS_0024_003C_003E8__locals0._0024VB_0024Local_update = [SpecialName] () =>
		{
			try
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_result.Text = CS_0024_003C_003E8__locals0._0024VB_0024Local_calc(CS_0024_003C_003E8__locals0._0024VB_0024Local_boxes.Select([SpecialName] (NumericUpDown b) => Convert.ToDouble(b.Value)).ToArray());
			}
			catch (Exception ex)
			{
				ProjectData.SetProjectError(ex);
				Exception ex2 = ex;
				CS_0024_003C_003E8__locals0._0024VB_0024Local_result.Text = ex2.Message;
				ProjectData.ClearProjectError();
			}
		};
		for (int i = 0; i < inputs.Length; i = checked(i + 1))
		{
			(string, double) tuple = inputs[i];
			tableLayoutPanel.Controls.Add(new Label
			{
				Text = tuple.Item1,
				AutoSize = true,
				Padding = new Padding(0, 6, 10, 0)
			});
			NumericUpDown numericUpDown = new NumericUpDown
			{
				DecimalPlaces = 2,
				Maximum = 100000m,
				Minimum = 0m,
				Width = 110,
				Value = new decimal(tuple.Item2),
				Increment = ((tuple.Item2 >= 10.0) ? 1m : 0.1m)
			};
			numericUpDown.ValueChanged += [SpecialName] (object a0, EventArgs a1) =>
			{
				((VB_0024AnonymousDelegate_22)([SpecialName] () =>
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_update();
				}))();
			};
			CS_0024_003C_003E8__locals0._0024VB_0024Local_boxes.Add(numericUpDown);
			tableLayoutPanel.Controls.Add(numericUpDown);
		}
		Panel panel = new Panel
		{
			Dock = DockStyle.Fill,
			Padding = new Padding(0, 10, 0, 0)
		};
		panel.Controls.Add(CS_0024_003C_003E8__locals0._0024VB_0024Local_result);
		tabPage.Controls.Add(panel);
		tabPage.Controls.Add(tableLayoutPanel);
		_tabs.TabPages.Add(tabPage);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_update();
	}
}
