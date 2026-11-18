using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows.Forms;

namespace PneuSim;

public class QuizDialog : Form
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__23_002D0
	{
		public TextBox _0024VB_0024Local_box;

		public QuizDialog _0024VB_0024Me;

		public _Closure_0024__23_002D0(_Closure_0024__23_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_box = arg0._0024VB_0024Local_box;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__R1(object a0, EventArgs a1)
		{
			_Lambda_0024__2();
		}

		[SpecialName]
		internal void _Lambda_0024__2()
		{
			using SaveFileDialog saveFileDialog = new SaveFileDialog
			{
				Filter = "Text file (*.txt)|*.txt",
				FileName = "pneusim-result.txt"
			};
			if (saveFileDialog.ShowDialog(_0024VB_0024Me) == DialogResult.OK)
			{
				File.WriteAllText(saveFileDialog.FileName, _0024VB_0024Local_box.Text);
			}
		}
	}

	private readonly bool _exam;

	private readonly List<QuizQuestion> _questions;

	private readonly int[] _answers;

	private int _index;

	private int _secondsLeft;

	private bool _checked;

	private int _score;

	private readonly Label _header;

	private readonly PictureBox _picture;

	private readonly Label _question;

	private readonly FlowLayoutPanel _optionsPanel;

	private readonly List<RadioButton> _options;

	private readonly Label _feedback;

	private readonly Button _next;

	private readonly Button _back;

	private readonly Timer _timer;

	public QuizDialog(bool exam)
	{
		_header = new Label
		{
			Dock = DockStyle.Top,
			Height = 30,
			Font = new Font("Segoe UI", 10f, FontStyle.Bold),
			Padding = new Padding(10, 6, 0, 0)
		};
		_picture = new PictureBox
		{
			Dock = DockStyle.Top,
			Height = 110,
			SizeMode = PictureBoxSizeMode.CenterImage,
			BackColor = Color.White
		};
		_question = new Label
		{
			Dock = DockStyle.Top,
			Height = 60,
			Font = new Font("Segoe UI", 11f),
			Padding = new Padding(10, 8, 10, 0)
		};
		_optionsPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Top,
			Height = 130,
			FlowDirection = FlowDirection.TopDown,
			Padding = new Padding(14, 0, 0, 0)
		};
		_options = new List<RadioButton>();
		_feedback = new Label
		{
			Dock = DockStyle.Fill,
			Font = new Font("Segoe UI", 9.5f),
			Padding = new Padding(12, 6, 12, 0)
		};
		_next = new Button
		{
			Text = "Check",
			Width = 110,
			Height = 32
		};
		_back = new Button
		{
			Text = "Back",
			Width = 90,
			Height = 32
		};
		_timer = new Timer
		{
			Interval = 1000
		};
		_exam = exam;
		_questions = QuestionBank.Pick(exam ? 20 : 10);
		_answers = Enumerable.Repeat(-1, _questions.Count).ToArray();
		_secondsLeft = 900;
		Text = (exam ? "Exam: pneumatics, electro-pneumatics and hydraulics" : "Practice quiz");
		Font = new Font("Segoe UI", 9f);
		base.Size = new Size(700, 560);
		base.StartPosition = FormStartPosition.CenterParent;
		base.ShowInTaskbar = false;
		int num = 0;
		do
		{
			RadioButton radioButton = new RadioButton
			{
				AutoSize = true,
				Font = new Font("Segoe UI", 10f),
				Margin = new Padding(3, 4, 3, 4)
			};
			_options.Add(radioButton);
			_optionsPanel.Controls.Add(radioButton);
			num = checked(num + 1);
		}
		while (num <= 3);
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Bottom,
			Height = 46,
			FlowDirection = FlowDirection.RightToLeft,
			Padding = new Padding(8)
		};
		flowLayoutPanel.Controls.Add(_next);
		if (exam)
		{
			flowLayoutPanel.Controls.Add(_back);
		}
		base.Controls.Add(_feedback);
		base.Controls.Add(_optionsPanel);
		base.Controls.Add(_question);
		base.Controls.Add(_picture);
		base.Controls.Add(_header);
		base.Controls.Add(flowLayoutPanel);
		base.AcceptButton = _next;
		_next.Click += OnNext;
		_back.Click += OnBack;
		base.KeyPreview = true;
		base.KeyDown += [SpecialName] (object s, KeyEventArgs e) =>
		{
			if (e.KeyCode == Keys.Escape)
			{
				Close();
			}
		};
		_timer.Tick += OnTimer;
		if (exam)
		{
			_timer.Start();
		}
		ShowQuestion();
	}

	private void ShowQuestion()
	{
		QuizQuestion quizQuestion = _questions[_index];
		_checked = false;
		_header.Text = $"Question {checked(_index + 1)} of {_questions.Count}   ({quizQuestion.Topic})" + (_exam ? $"      Time left: {_secondsLeft / 60}:{_secondsLeft % 60:00}" : $"      Score: {_score}");
		_question.Text = quizQuestion.Text;
		if (quizQuestion.SymbolPreset != null)
		{
			_picture.Image = Library.RenderThumbnail(quizQuestion.SymbolPreset.Factory(), 240, 100);
			_picture.Visible = true;
		}
		else
		{
			_picture.Visible = false;
		}
		int num = 0;
		checked
		{
			do
			{
				_options[num].Visible = num < quizQuestion.Options.Length;
				if (num < quizQuestion.Options.Length)
				{
					_options[num].Text = quizQuestion.Options[num];
				}
				_options[num].Checked = _exam && _answers[_index] == num;
				_options[num].ForeColor = (AppSettings.DarkMode ? Theme.DarkFore : SystemColors.ControlText);
				_options[num].Enabled = true;
				num++;
			}
			while (num <= 3);
			_feedback.Text = "";
			_next.Text = ((!_exam) ? "Check" : ((_index == _questions.Count - 1) ? "Finish" : "Next"));
			_back.Enabled = _index > 0;
		}
	}

	private void OnBack(object sender, EventArgs e)
	{
		checked
		{
			if (_index != 0)
			{
				_answers[_index] = Chosen();
				_index--;
				ShowQuestion();
			}
		}
	}

	protected override void OnFormClosed(FormClosedEventArgs e)
	{
		_timer.Stop();
		_timer.Dispose();
		base.OnFormClosed(e);
	}

	private int Chosen()
	{
		return _options.FindIndex([SpecialName] (RadioButton o) => o.Checked);
	}

	private void OnNext(object sender, EventArgs e)
	{
		QuizQuestion quizQuestion = _questions[_index];
		checked
		{
			if (!_exam && !_checked)
			{
				int num = Chosen();
				if (num < 0)
				{
					_feedback.Text = "Choose an answer first.";
					return;
				}
				_answers[_index] = num;
				_checked = true;
				if (num == quizQuestion.Correct)
				{
					_score++;
				}
				_options[quizQuestion.Correct].ForeColor = Color.DarkGreen;
				if (num != quizQuestion.Correct)
				{
					_options[num].ForeColor = Color.Firebrick;
				}
				foreach (RadioButton option in _options)
				{
					option.Enabled = false;
				}
				_feedback.ForeColor = ((num == quizQuestion.Correct) ? Color.DarkGreen : Color.Firebrick);
				_feedback.Text = ((num == quizQuestion.Correct) ? "Correct! " : "Not quite. ") + quizQuestion.Explanation;
				_next.Text = ((_index == _questions.Count - 1) ? "Finish" : "Next");
			}
			else
			{
				if (_exam)
				{
					_answers[_index] = Chosen();
				}
				if (_index < _questions.Count - 1)
				{
					_index++;
					ShowQuestion();
				}
				else
				{
					Finish();
				}
			}
		}
	}

	private void OnTimer(object sender, EventArgs e)
	{
		checked
		{
			_secondsLeft--;
			if (_secondsLeft <= 0)
			{
				_answers[_index] = Chosen();
				Finish();
				return;
			}
		}
		_header.Text = $"Question {checked(_index + 1)} of {_questions.Count}   ({_questions[_index].Topic})      Time left: {_secondsLeft / 60}:{_secondsLeft % 60:00}";
	}

	private void Finish()
	{
		_Closure_0024__23_002D0 arg = default(_Closure_0024__23_002D0);
		_Closure_0024__23_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__23_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Me = this;
		_timer.Stop();
		int num = Enumerable.Range(0, _questions.Count).Count([SpecialName] (int i) => _answers[i] == _questions[i].Correct);
		double num2 = 100.0 * (double)num / (double)_questions.Count;
		string text = ((num2 >= 90.0) ? "Excellent" : ((num2 >= 75.0) ? "Very good" : ((num2 >= 60.0) ? "Good" : ((num2 >= 40.0) ? "Pass" : "Needs more practice"))));
		StringBuilder stringBuilder = new StringBuilder();
		stringBuilder.AppendLine(string.Format("{0} result: {1} of {2} correct ({3:0} %) — {4}", _exam ? "Exam" : "Quiz", num, _questions.Count, num2, text));
		stringBuilder.AppendLine($"Date: {DateTime.Now:dd-MM-yyyy HH:mm}");
		stringBuilder.AppendLine();
		List<int> list = (from i in Enumerable.Range(0, _questions.Count)
			where _answers[i] != _questions[i].Correct
			select i).ToList();
		if (list.Count > 0)
		{
			stringBuilder.AppendLine("Review:");
			foreach (int item in list)
			{
				QuizQuestion quizQuestion = _questions[item];
				stringBuilder.AppendLine(string.Format("Q{0}. {1}{2}", checked(item + 1), quizQuestion.Text, (quizQuestion.SymbolPreset != null) ? " [symbol]" : ""));
				stringBuilder.AppendLine(string.Format("   Your answer: {0}", (_answers[item] < 0) ? "(none)" : quizQuestion.Options[_answers[item]]));
				stringBuilder.AppendLine($"   Correct: {quizQuestion.Options[quizQuestion.Correct]} — {quizQuestion.Explanation}");
			}
		}
		base.Controls.Clear();
		CS_0024_003C_003E8__locals0._0024VB_0024Local_box = new TextBox
		{
			Multiline = true,
			ReadOnly = true,
			Dock = DockStyle.Fill,
			ScrollBars = ScrollBars.Vertical,
			Font = new Font("Consolas", 9.5f),
			BackColor = Color.White,
			Text = stringBuilder.ToString().Replace("\n", "\r\n").Replace("\r\r\n", "\r\n")
		};
		FlowLayoutPanel flowLayoutPanel = new FlowLayoutPanel
		{
			Dock = DockStyle.Bottom,
			Height = 46,
			FlowDirection = FlowDirection.RightToLeft,
			Padding = new Padding(8)
		};
		Button button = new Button
		{
			Text = "Close",
			Width = 90,
			Height = 30,
			DialogResult = DialogResult.OK
		};
		Button button2 = new Button
		{
			Text = "Save result...",
			Width = 110,
			Height = 30
		};
		button2.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			CS_0024_003C_003E8__locals0._Lambda_0024__2();
		};
		flowLayoutPanel.Controls.AddRange(new Control[2] { button, button2 });
		base.Controls.Add(CS_0024_003C_003E8__locals0._0024VB_0024Local_box);
		base.Controls.Add(flowLayoutPanel);
		base.AcceptButton = button;
	}
}
