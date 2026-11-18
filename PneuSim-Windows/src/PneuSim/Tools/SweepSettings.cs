using System.Collections.Generic;

namespace PneuSim;

public class SweepSettings
{
	public int ElementIndex { get; set; }

	public LiveTuning.Knob Knob { get; set; }

	public double FromValue { get; set; }

	public double ToValue { get; set; }

	public int Steps { get; set; }

	public double Seconds { get; set; }

	public List<int> Operate { get; set; }

	public bool RealPhysics { get; set; }

	public SweepSettings()
	{
		Steps = 6;
		Seconds = 10.0;
		Operate = new List<int>();
	}
}
