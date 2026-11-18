namespace PneuSim;

public class QuizQuestion
{
	public string Topic { get; set; }

	public string Text { get; set; }

	public string[] Options { get; set; }

	public int Correct { get; set; }

	public string Explanation { get; set; }

	public LibraryPreset SymbolPreset { get; set; }
}
