using System;

namespace PneuSim;

public class LibraryPreset
{
	public string Category { get; }

	public string Name { get; }

	public Func<CircuitElement> Factory { get; }

	public LibraryPreset(string category, string name, Func<CircuitElement> factory)
	{
		Category = category;
		Name = name;
		Factory = factory;
	}
}
