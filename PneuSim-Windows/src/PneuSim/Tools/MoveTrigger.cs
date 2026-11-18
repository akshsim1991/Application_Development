using System.Collections.Generic;

namespace PneuSim;

public class MoveTrigger
{
	public readonly List<string> Marks;

	public readonly List<string> Manual;

	public bool AlwaysOn;

	public bool Gated;

	public string Description;

	public MoveTrigger()
	{
		Marks = new List<string>();
		Manual = new List<string>();
		Description = "";
	}
}
