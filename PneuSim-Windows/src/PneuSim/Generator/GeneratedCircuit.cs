namespace PneuSim;

public class GeneratedCircuit
{
	public Circuit Circuit { get; set; }

	public string Explanation { get; set; }

	/// <summary>True when the generated parts were added to a copy of the workspace circuit instead of making a new circuit.</summary>
	public bool MergedIntoWorkspace { get; set; }
}
