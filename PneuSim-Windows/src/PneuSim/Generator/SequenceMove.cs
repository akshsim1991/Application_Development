using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class SequenceMove
{
	public char Letter { get; }

	public bool Extend { get; }

	public SequenceMove(char letter, bool extend)
	{
		Letter = char.ToUpperInvariant(letter);
		Extend = extend;
	}

	public override string ToString()
	{
		return Conversions.ToString(Letter) + (Extend ? "+" : "-");
	}
}
