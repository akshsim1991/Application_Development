using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Runtime.CompilerServices;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class ParameterSweep
{
	public const double TimeStep = 0.005;

	public static SweepResult Run(Project project, SweepSettings settings, Action<int> progress = null)
	{
		SweepResult sweepResult = new SweepResult();
		int num = Math.Max(2, Math.Min(50, settings.Steps));
		string xml = project.ToXml();
		List<string> list = null;
		checked
		{
			int num2 = num - 1;
			for (int j = 0; j <= num2; j++)
			{
				double value = settings.FromValue + (settings.ToValue - settings.FromValue) * (double)j / (double)(num - 1);
				Project project2 = Project.FromXml(xml);
				List<CircuitElement> list2 = project2.AllElements().ToList();
				Circuit circuit = project2.SimulationCircuit();
				LiveTuning.SetValue(list2[settings.ElementIndex], settings.Knob, value);
				double value2 = LiveTuning.GetValue(list2[settings.ElementIndex], settings.Knob);
				List<CylinderBase> list3 = circuit.Elements.OfType<CylinderBase>().ToList();
				if (list == null)
				{
					list = list3.Select([SpecialName] (CylinderBase c, int i) => (!string.IsNullOrWhiteSpace(c.Label)) ? c.Label : $"cylinder {i + 1}").ToList();
					sweepResult.Columns.Add(settings.Knob.Caption);
					sweepResult.Columns.Add("Cycle time (s)");
					sweepResult.Columns.Add("Air per cycle (NL)");
					sweepResult.Columns.Add("Air used in the run (NL)");
					foreach (string item in list)
					{
						sweepResult.Columns.Add($"{item} first fully out at (s)");
						sweepResult.Columns.Add($"{item} top speed (m/s)");
					}
				}
				Simulator simulator = new Simulator(circuit)
				{
					RealPhysics = settings.RealPhysics
				};
				simulator.Reset();
				foreach (int item2 in settings.Operate)
				{
					if (item2 >= 0 && item2 < list2.Count)
					{
						CircuitElement circuitElement = list2[item2];
						RectangleF localBounds = circuitElement.LocalBounds;
						circuitElement.OnSimMouseDown(new PointF(localBounds.Left + 2f, localBounds.Top + localBounds.Height / 2f));
					}
				}
				simulator.RunLogic();
				double[] array = list3.Select([SpecialName] (CylinderBase c) => double.NaN).ToArray();
				double[] array2 = list3.Select([SpecialName] (CylinderBase c) => 0.0).ToArray();
				int num3 = (int)Math.Round(settings.Seconds / 0.005);
				for (int k = 1; k <= num3; k++)
				{
					simulator.Step(0.005);
					int num4 = list3.Count - 1;
					for (int l = 0; l <= num4; l++)
					{
						if (double.IsNaN(array[l]) && list3[l].Position >= 0.995)
						{
							array[l] = simulator.Time;
						}
						array2[l] = Math.Max(array2[l], Math.Abs(list3[l].Velocity));
					}
				}
				int num5 = simulator.CompletedCycles();
				List<double> list4 = new List<double>
				{
					value2,
					(simulator.LastCycleTime > 0.0) ? simulator.LastCycleTime : double.NaN,
					(num5 > 0) ? (simulator.AirConsumed / (double)num5) : double.NaN,
					simulator.AirConsumed
				};
				int num6 = list3.Count - 1;
				for (int m = 0; m <= num6; m++)
				{
					list4.Add(array[m]);
					list4.Add(array2[m]);
				}
				sweepResult.Rows.Add(list4.ToArray());
				progress?.Invoke((int)Math.Round((double)((j + 1) * 100) / (double)num));
			}
			return sweepResult;
		}
	}
}
