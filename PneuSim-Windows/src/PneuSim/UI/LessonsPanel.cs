using System;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class LessonsPanel : UserControl
{
	private readonly ListBox _list;

	private readonly TextBox _text;

	private readonly TextBox _result;

	public Func<Circuit> CurrentCircuit { get; set; }

	public event EventHandler<Circuit> LoadCircuit;

	public LessonsPanel()
	{
		_list = new ListBox
		{
			Dock = DockStyle.Fill,
			IntegralHeight = false
		};
		_text = new TextBox
		{
			Dock = DockStyle.Fill,
			Multiline = true,
			ReadOnly = true,
			ScrollBars = ScrollBars.Vertical,
			WordWrap = true,
			Font = new Font("Segoe UI", 9.5f),
			BackColor = Color.White,
			BorderStyle = BorderStyle.None
		};
		_result = new TextBox
		{
			Dock = DockStyle.Bottom,
			Height = 58,
			Multiline = true,
			ReadOnly = true,
			WordWrap = true,
			ScrollBars = ScrollBars.Vertical,
			Font = new Font("Segoe UI", 9.5f, FontStyle.Bold),
			BorderStyle = BorderStyle.None,
			BackColor = Color.White
		};
		Lesson[] all = Lessons.All;
		foreach (Lesson item in all)
		{
			_list.Items.Add(item);
		}
		TableLayoutPanel tableLayoutPanel = new TableLayoutPanel
		{
			Dock = DockStyle.Bottom,
			Height = 96,
			ColumnCount = 1,
			RowCount = 3
		};
		Button button = new Button
		{
			Text = "Load starting parts",
			Dock = DockStyle.Fill
		};
		Button button2 = new Button
		{
			Text = "Check my solution",
			Dock = DockStyle.Fill
		};
		Button button3 = new Button
		{
			Text = "Hint",
			Dock = DockStyle.Fill
		};
		int num = 0;
		do
		{
			tableLayoutPanel.RowStyles.Add(new RowStyle(SizeType.Percent, 33.3f));
			num = checked(num + 1);
		}
		while (num <= 2);
		tableLayoutPanel.Controls.Add(button, 0, 0);
		tableLayoutPanel.Controls.Add(button2, 0, 1);
		tableLayoutPanel.Controls.Add(button3, 0, 2);
		Panel panel = new Panel
		{
			Dock = DockStyle.Left,
			Width = 220
		};
		panel.Controls.Add(_list);
		panel.Controls.Add(tableLayoutPanel);
		Panel panel2 = new Panel
		{
			Dock = DockStyle.Fill,
			Padding = new Padding(8, 4, 4, 4)
		};
		panel2.Controls.Add(_text);
		panel2.Controls.Add(_result);
		base.Controls.Add(panel2);
		base.Controls.Add(panel);
		_list.SelectedIndexChanged += [SpecialName] (object a0, EventArgs a1) =>
		{
			ShowLesson();
		};
		button.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__11_002D1();
		};
		button2.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			CheckLesson();
		};
		button3.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__11_002D3();
		};
		_list.DrawItem += DrawItem;
		_list.DrawMode = DrawMode.OwnerDrawFixed;
		_list.ItemHeight = 22;
		_list.SelectedIndex = 0;
	}

	private Lesson Current()
	{
		return _list.SelectedItem as Lesson;
	}

	private void ShowResult(string text, Color color)
	{
		_result.ForeColor = (AppSettings.DarkMode ? ControlPaint.LightLight(color) : color);
		_result.Text = text;
	}

	private void ShowLesson()
	{
		Lesson lesson = Current();
		if (lesson != null)
		{
			_text.Text = lesson.Title + (AppSettings.CompletedLessons.Contains(lesson.Title) ? "   (passed)" : "") + "\r\n\r\nTASK\r\n" + lesson.Goal + "\r\n\r\n" + ((lesson.Start == null) ? "Start with an empty page (File > New) or press 'Load starting parts' for an empty page." : "Press 'Load starting parts' to get the components, then connect them.") + "\r\nWhen you think it works, press 'Check my solution'. PneuSim tests your circuit by simulating it.";
			_result.Text = "";
		}
	}

	private void CheckLesson()
	{
		Lesson lesson = Current();
		if (lesson == null || CurrentCircuit == null)
		{
			return;
		}
		try
		{
			LessonResult lessonResult = lesson.Check(CurrentCircuit());
			ShowResult((lessonResult.Passed ? "✔ " : "✘ ") + lessonResult.Feedback, lessonResult.Passed ? Color.DarkGreen : Color.Firebrick);
			if (lessonResult.Passed && AppSettings.CompletedLessons.Add(lesson.Title))
			{
				AppSettings.Save();
				_list.Invalidate();
			}
		}
		catch (Exception ex)
		{
			ProjectData.SetProjectError(ex);
			Exception ex2 = ex;
			ShowResult("Could not test the circuit: " + ex2.Message, Color.Firebrick);
			ProjectData.ClearProjectError();
		}
	}

	private void DrawItem(object sender, DrawItemEventArgs e)
	{
		checked
		{
			if (e.Index >= 0)
			{
				e.DrawBackground();
				Lesson lesson = (Lesson)_list.Items[e.Index];
				bool flag = AppSettings.CompletedLessons.Contains(lesson.Title);
				string text = (flag ? "✔ " : "   ");
				using SolidBrush brush = new SolidBrush(((e.State & DrawItemState.Selected) != 0) ? SystemColors.HighlightText : (flag ? Color.SeaGreen : e.ForeColor));
				e.Graphics.DrawString(text + lesson.Title, e.Font, brush, e.Bounds.X + 2, e.Bounds.Y + 3);
			}
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__11_002D1()
	{
		Lesson lesson = Current();
		if (lesson != null)
		{
			LoadCircuit?.Invoke(this, (lesson.Start == null) ? new Circuit() : lesson.Start());
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__11_002D3()
	{
		Lesson lesson = Current();
		if (lesson != null)
		{
			ShowResult("Hint: " + lesson.Hint, Color.DarkBlue);
		}
	}
}
