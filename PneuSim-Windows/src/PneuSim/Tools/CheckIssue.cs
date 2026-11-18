using System;

namespace PneuSim;

public class CheckIssue
{
	public IssueSeverity Severity { get; set; }

	public string Message { get; set; }

	public CircuitElement Element { get; set; }

	public string FixLabel { get; set; }

	public Func<Circuit> Fix { get; set; }

	public override string ToString()
	{
		return $"[{Severity}] {Message}";
	}
}
