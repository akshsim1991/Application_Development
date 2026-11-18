using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.Text;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Sizing sheets: bore, tube, valve, pump, motor, accumulator, compressor and receiver, using generic standard sizes.</summary>
public sealed class SizingDialog : Form
{
	private sealed class Sheet
	{
		public readonly TabPage Page;

		public readonly TableLayoutPanel Grid = new TableLayoutPanel
		{
			Dock = DockStyle.Top,
			AutoSize = true,
			ColumnCount = 2,
			Padding = new Padding(8, 8, 8, 4)
		};

		public readonly TextBox Result = new TextBox
		{
			Multiline = true,
			ReadOnly = true,
			Dock = DockStyle.Fill,
			ScrollBars = ScrollBars.Vertical,
			Font = new Font("Consolas", 9.5f)
		};

		public readonly Dictionary<string, NumericUpDown> Inputs = new Dictionary<string, NumericUpDown>();

		public Func<Sheet, string> Compute;

		public Sheet(TabControl tabs, string title, string help)
		{
			Page = new TabPage(title);
			Label value = new Label
			{
				Text = help,
				Dock = DockStyle.Top,
				Height = 44,
				Padding = new Padding(8, 6, 8, 0)
			};
			Grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 330f));
			Grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 110f));
			Page.Controls.Add(Result);
			Page.Controls.Add(Grid);
			Page.Controls.Add(value);
			tabs.TabPages.Add(Page);
		}

		public void Add(string key, string label, decimal value, decimal min, decimal max, int decimals)
		{
			NumericUpDown numericUpDown = new NumericUpDown
			{
				Minimum = min,
				Maximum = max,
				DecimalPlaces = decimals,
				Value = value,
				Width = 100,
				Increment = ((decimals == 0) ? 1m : ((decimals == 1) ? 0.5m : 0.1m))
			};
			int row = Grid.RowCount;
			Grid.RowCount = row + 1;
			Grid.Controls.Add(new Label
			{
				Text = label,
				AutoSize = false,
				Width = 320,
				Height = 24,
				TextAlign = ContentAlignment.MiddleLeft
			}, 0, row);
			Grid.Controls.Add(numericUpDown, 1, row);
			Inputs[key] = numericUpDown;
			numericUpDown.ValueChanged += delegate
			{
				Refresh_();
			};
		}

		public double V(string key)
		{
			return (double)Inputs[key].Value;
		}

		public void Refresh_()
		{
			try
			{
				Result.Text = Compute(this).Replace("\n", "\r\n");
			}
			catch (Exception ex)
			{
				Result.Text = "Check the inputs: " + ex.Message;
			}
		}
	}

	private static string F(double v, string fmt = "0.##")
	{
		return v.ToString(fmt, CultureInfo.CurrentCulture);
	}

	public SizingDialog()
	{
		Text = "Sizing sheets";
		Font = new Font("Segoe UI", 9f);
		base.StartPosition = FormStartPosition.CenterParent;
		base.ShowInTaskbar = false;
		base.Size = new Size(640, 640);
		MinimumSize = new Size(560, 520);
		TabControl tabControl = new TabControl
		{
			Dock = DockStyle.Fill
		};
		Sheet sheet = new Sheet(tabControl, "Cylinder", "Find the cylinder bore for a force, then check the force, air use and speed. Sizes are standard series; results are guidance, check the manufacturer's data.");
		sheet.Add("F", "Force needed (N)", 1000m, 1m, 1000000m, 0);
		sheet.Add("P", "Working pressure (bar)", 6m, 1m, 400m, 1);
		sheet.Add("U", "Usable share of theoretical force (0.7 air, 0.9 oil)", 0.7m, 0.3m, 1m, 2);
		sheet.Add("S", "Stroke (mm)", 100m, 5m, 3000m, 0);
		sheet.Add("T", "Time for the stroke (s)", 0.5m, 0.05m, 60m, 2);
		sheet.Compute = delegate(Sheet s)
		{
			double num = Sizing.RecommendedBoreMm(s.V("F"), s.V("P"), s.V("U"));
			double num2 = Sizing.RecommendedRodMm(num);
			double num3 = s.V("S") / s.V("T");
			StringBuilder stringBuilder = new StringBuilder();
			stringBuilder.AppendLine("Theoretical bore:  " + F(Sizing.RequiredBoreMm(s.V("F"), s.V("P"), s.V("U")), "0.0") + " mm");
			stringBuilder.AppendLine("Standard bore:     " + F(num, "0") + " mm  (rod " + F(num2, "0") + " mm)");
			stringBuilder.AppendLine("Force extending:   " + F(Sizing.Force(num, num2, s.V("P"), s.V("U"), retract: false), "0") + " N");
			stringBuilder.AppendLine("Force retracting:  " + F(Sizing.Force(num, num2, s.V("P"), s.V("U"), retract: true), "0") + " N");
			stringBuilder.AppendLine("Speed needed:      " + F(num3, "0") + " mm/s");
			stringBuilder.AppendLine("Free air per cycle:" + F(Sizing.AirPerCycleNl(num, num2, s.V("S"), s.V("P")), "0.00") + " NL");
			double num4 = Sizing.AirFlowNlMin(num, num3, s.V("P"));
			var tuple = Sizing.RecommendedValve(num4);
			stringBuilder.AppendLine("Peak air flow:     " + F(num4, "0") + " NL/min");
			stringBuilder.AppendLine("Valve:             port " + tuple.Port + " (nominal flow about " + F(tuple.Nominal, "0") + " NL/min, includes a 1.5 safety factor)");
			var tuple2 = Sizing.RecommendedTube(Sizing.PneumaticTubes, Sizing.RequiredAirTubeIdMm(num4, s.V("P"), 15.0));
			stringBuilder.AppendLine("Tube:              " + F(tuple2.Od, "0") + " mm outer / " + F(tuple2.Id, "0.0") + " mm inner (air speed under 15 m/s)");
			return stringBuilder.ToString();
		};
		Sheet sheet2 = new Sheet(tabControl, "Air line", "Tube and valve size for a compressed-air flow, and the pressure drop to expect.");
		sheet2.Add("Q", "Free air flow (NL/min)", 1000m, 1m, 200000m, 0);
		sheet2.Add("P", "Working pressure (bar)", 6m, 1m, 16m, 1);
		sheet2.Add("V", "Maximum air speed in the tube (m/s)", 15m, 3m, 30m, 0);
		sheet2.Add("L", "Tube length (m)", 5m, 0.1m, 500m, 1);
		sheet2.Compute = delegate(Sheet s)
		{
			double num5 = Sizing.RequiredAirTubeIdMm(s.V("Q"), s.V("P"), s.V("V"));
			var tuple3 = Sizing.RecommendedTube(Sizing.PneumaticTubes, num5);
			var tuple4 = Sizing.RecommendedValve(s.V("Q"), 1.2);
			double num6 = s.V("Q") / 60000.0 * 1.013 / (s.V("P") + 1.013);
			double num7 = tuple3.Id / 1000.0;
			double num8 = num6 / (Math.PI / 4.0 * num7 * num7);
			double num9 = 0.02 * (s.V("L") / num7) * 0.5 * ((s.V("P") + 1.013) * 100000.0 / 287.0 / 293.0) * num8 * num8 / 100000.0;
			StringBuilder stringBuilder2 = new StringBuilder();
			stringBuilder2.AppendLine("Tube inner diameter needed: " + F(num5, "0.0") + " mm");
			stringBuilder2.AppendLine("Standard tube:              " + F(tuple3.Od, "0") + " mm outer / " + F(tuple3.Id, "0.0") + " mm inner");
			stringBuilder2.AppendLine("Air speed in that tube:     " + F(num8, "0.0") + " m/s");
			stringBuilder2.AppendLine("Pressure drop over the line:" + F(num9, "0.00") + " bar (estimate, friction factor 0.02)");
			stringBuilder2.AppendLine("Valve / fitting size:       " + tuple4.Port + " (nominal flow about " + F(tuple4.Nominal, "0") + " NL/min)");
			if (num9 > 0.1 * s.V("P"))
			{
				stringBuilder2.AppendLine("Warning: the pressure drop is more than 10 % of the supply pressure; use a larger tube or a shorter line.");
			}
			return stringBuilder2.ToString();
		};
		Sheet sheet3 = new Sheet(tabControl, "Hydraulic pump and motor", "Pump flow, displacement, electric motor power, tank volume and tube size for a hydraulic cylinder.");
		sheet3.Add("D", "Cylinder bore (mm)", 63m, 10m, 500m, 0);
		sheet3.Add("V", "Speed needed (mm/s)", 100m, 1m, 2000m, 0);
		sheet3.Add("P", "Working pressure (bar)", 160m, 10m, 700m, 0);
		sheet3.Add("N", "Pump speed (rpm)", 1450m, 500m, 3600m, 0);
		sheet3.Compute = delegate(Sheet s)
		{
			double num10 = Sizing.PumpFlowLMin(s.V("D"), s.V("V"));
			double num11 = Sizing.MotorPowerKw(num10, s.V("P"), 0.85);
			double v2 = Sizing.NextUp(Sizing.StandardMotorsKw, num11);
			var tuple5 = Sizing.RecommendedTube(Sizing.HydraulicTubes, Sizing.RequiredOilTubeIdMm(num10, 4.0));
			StringBuilder stringBuilder3 = new StringBuilder();
			stringBuilder3.AppendLine("Pump flow:          " + F(num10, "0.0") + " l/min");
			stringBuilder3.AppendLine("Pump displacement:  " + F(Sizing.PumpDisplacementCc(num10, s.V("N"), 0.95), "0.0") + " cm3/rev");
			stringBuilder3.AppendLine("Hydraulic power:    " + F(Sizing.HydraulicPowerKw(num10, s.V("P")), "0.00") + " kW");
			stringBuilder3.AppendLine("Electric motor:     " + F(num11, "0.00") + " kW needed, standard size " + F(v2, "0.##") + " kW");
			stringBuilder3.AppendLine("Tank volume:        about " + F(Sizing.TankLitres(num10), "0") + " l (3 x pump flow per minute)");
			stringBuilder3.AppendLine("Pressure tube:      " + F(tuple5.Od, "0") + " mm outer / " + F(tuple5.Id, "0") + " mm inner (oil speed under 4 m/s)");
			return stringBuilder3.ToString();
		};
		Sheet sheet4 = new Sheet(tabControl, "Accumulator", "Gas-charged accumulator volume to deliver a volume of oil between a minimum and maximum pressure (Boyle's law). Pressures in bar gauge.");
		sheet4.Add("DV", "Oil volume to deliver (l)", 1m, 0.01m, 500m, 2);
		sheet4.Add("P1", "Minimum working pressure (bar)", 100m, 1m, 700m, 0);
		sheet4.Add("P2", "Maximum working pressure (bar)", 200m, 2m, 700m, 0);
		sheet4.Add("N", "Gas exponent (1.0 slow discharge, 1.4 fast)", 1.4m, 1m, 1.4m, 1);
		sheet4.Compute = delegate(Sheet s)
		{
			double num12 = 0.9 * (s.V("P1") + 1.013) - 1.013;
			double num13 = Sizing.AccumulatorLitres(s.V("DV"), num12 + 1.013, s.V("P1") + 1.013, s.V("P2") + 1.013, s.V("N"));
			StringBuilder stringBuilder4 = new StringBuilder();
			stringBuilder4.AppendLine("Gas precharge pressure:  " + F(num12, "0") + " bar (90 % of the minimum working pressure)");
			stringBuilder4.AppendLine("Accumulator volume:      " + F(num13, "0.0") + " l needed");
			stringBuilder4.AppendLine("Standard size:           " + F(Sizing.NextUp(Sizing.StandardAccumulatorsL, num13), "0.###") + " l");
			return stringBuilder4.ToString();
		};
		Sheet sheet5 = new Sheet(tabControl, "Compressor and receiver", "Compressor delivery and air receiver volume for a compressed-air demand.");
		sheet5.Add("Q", "Total air demand (NL/min)", 1500m, 10m, 500000m, 0);
		sheet5.Add("PEAK", "Peak demand (NL/min)", 3000m, 10m, 500000m, 0);
		sheet5.Add("SIM", "Share of consumers running at the same time (0.1 to 1)", 0.8m, 0.1m, 1m, 2);
		sheet5.Add("RES", "Reserve for growth (1.0 = none, 1.25 = 25 %)", 1.25m, 1m, 3m, 2);
		sheet5.Add("T", "Buffer time the receiver must carry the peak (min)", 0.5m, 0.05m, 10m, 2);
		sheet5.Add("DP", "Pressure drop allowed in the receiver (bar)", 1m, 0.2m, 4m, 1);
		sheet5.Compute = delegate(Sheet s)
		{
			double num14 = Sizing.CompressorFadNlMin(s.V("Q"), s.V("SIM"), s.V("RES"));
			double num15 = Sizing.ReceiverLitres(s.V("PEAK"), s.V("T"), s.V("DP"));
			StringBuilder stringBuilder5 = new StringBuilder();
			stringBuilder5.AppendLine("Compressor free air delivery: " + F(num14, "0") + " NL/min (" + F(num14 / 1000.0, "0.00") + " m3/min)");
			stringBuilder5.AppendLine("Drive power (estimate):       " + F(Sizing.CompressorKw(num14), "0.0") + " kW");
			stringBuilder5.AppendLine("Receiver volume:              " + F(num15, "0") + " l needed, standard size " + F(Sizing.NextUp(Sizing.StandardReceiversL, num15), "0") + " l");
			return stringBuilder5.ToString();
		};
		foreach (Sheet item in new Sheet[5] { sheet, sheet2, sheet3, sheet4, sheet5 })
		{
			item.Refresh_();
		}
		Button button = new Button
		{
			Text = "Close",
			DialogResult = DialogResult.OK,
			Dock = DockStyle.Bottom,
			Height = 34
		};
		base.Controls.Add(tabControl);
		base.Controls.Add(button);
		base.AcceptButton = button;
		base.CancelButton = button;
	}
}
