using System;

namespace PneuSim;

public class Lesson
{
	public string Title { get; set; }

	public string Goal { get; set; }

	public string Hint { get; set; }

	public Func<Circuit> Start { get; set; }

	public Func<Circuit, LessonResult> Check { get; set; }

	public override string ToString()
	{
		return Title;
	}
}
