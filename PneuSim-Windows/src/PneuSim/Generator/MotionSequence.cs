using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text.RegularExpressions;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class MotionSequence
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__13_002D0
	{
		public char _0024VB_0024Local_letter;

		public Func<SequenceMove, bool> _0024I2;

		public _Closure_0024__13_002D0(_Closure_0024__13_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_letter = arg0._0024VB_0024Local_letter;
			}
		}

		[SpecialName]
		internal bool _Lambda_0024__2(SequenceMove m)
		{
			return m.Letter == _0024VB_0024Local_letter;
		}
	}

	public List<List<SequenceMove>> Steps { get; }

	public List<char> Letters { get; }

	public bool HasParallelSteps => Steps.Any([SpecialName] (List<SequenceMove> s) => s.Count > 1);

	public MotionSequence()
	{
		Steps = new List<List<SequenceMove>>();
		Letters = new List<char>();
	}

	public int Number(char letter)
	{
		return checked(Letters.IndexOf(letter) + 1);
	}

	public IEnumerable<SequenceMove> AllMoves()
	{
		return Steps.SelectMany([SpecialName] (List<SequenceMove> s) => s);
	}

	public override string ToString()
	{
		return string.Join(" ", Steps.Select([SpecialName] (List<SequenceMove> s) => (s.Count != 1) ? ("(" + string.Join(" ", s) + ")") : s[0].ToString()));
	}

	public static MotionSequence Parse(string text)
	{
		MotionSequence motionSequence = new MotionSequence();
		string text2 = (text ?? "").Replace(",", " ").Replace(";", " ").Trim();
		if (text2.Length == 0)
		{
			throw new FormatException("Type a sequence, for example: A+ B+ B- A-");
		}
		int num = 0;
		Regex regex = new Regex("\\G\\s*(?:\\(([^()]*)\\)|([A-Za-z])\\s*([+-]))");
		Regex regex2 = new Regex("([A-Za-z])\\s*([+-])");
		while (num < text2.Length)
		{
			Match match = regex.Match(text2, num);
			if (!match.Success || match.Length == 0)
			{
				if (text2.Substring(num).Trim().Length == 0)
				{
					break;
				}
				throw new FormatException($"Cannot read \"{text2.Substring(num).Trim()}\". Write each movement as a letter followed by + or -, e.g. A+ B+ B- A-. Put simultaneous movements in brackets: (B+ C+).");
			}
			List<SequenceMove> list = new List<SequenceMove>();
			if (match.Groups[1].Success)
			{
				foreach (Match item in regex2.Matches(match.Groups[1].Value))
				{
					list.Add(new SequenceMove(item.Groups[1].Value[0], Operators.CompareString(item.Groups[2].Value, "+", TextCompare: false) == 0));
				}
				if (list.Count == 0)
				{
					throw new FormatException("Empty brackets in the sequence.");
				}
			}
			else
			{
				list.Add(new SequenceMove(match.Groups[2].Value[0], Operators.CompareString(match.Groups[3].Value, "+", TextCompare: false) == 0));
			}
			motionSequence.Steps.Add(list);
			num = checked(match.Index + match.Length);
		}
		motionSequence.Letters.AddRange(from c in (from mv in motionSequence.AllMoves()
				select mv.Letter).Distinct()
			orderby c
			select c);
		motionSequence.Validate();
		return motionSequence;
	}

	private void Validate()
	{
		if (Steps.Count < 2)
		{
			throw new FormatException("A sequence needs at least two steps.");
		}
		if (Letters.Count > 8)
		{
			throw new FormatException("At most 8 cylinders are supported.");
		}
		foreach (List<SequenceMove> step in Steps)
		{
			IGrouping<char, SequenceMove> grouping = (from mv in step
				group mv by mv.Letter).FirstOrDefault([SpecialName] (IGrouping<char, SequenceMove> gr) => gr.Count() > 1);
			if (grouping != null)
			{
				throw new FormatException($"Cylinder {grouping.Key} appears twice in the same step.");
			}
		}
		using List<char>.Enumerator enumerator2 = Letters.GetEnumerator();
		_Closure_0024__13_002D0 closure_0024__13_002D = default(_Closure_0024__13_002D0);
		while (enumerator2.MoveNext())
		{
			closure_0024__13_002D = new _Closure_0024__13_002D0(closure_0024__13_002D);
			closure_0024__13_002D._0024VB_0024Local_letter = enumerator2.Current;
			bool flag = false;
			foreach (SequenceMove item in AllMoves().Where(closure_0024__13_002D._Lambda_0024__2))
			{
				if (item.Extend == flag)
				{
					throw new FormatException($"Cylinder {closure_0024__13_002D._0024VB_0024Local_letter} must alternate: it starts retracted, so its first movement is {closure_0024__13_002D._0024VB_0024Local_letter}+, then {closure_0024__13_002D._0024VB_0024Local_letter}-, and so on.");
				}
				flag = item.Extend;
			}
			if (flag)
			{
				throw new FormatException($"Cylinder {closure_0024__13_002D._0024VB_0024Local_letter} must end retracted ({closure_0024__13_002D._0024VB_0024Local_letter}-) so the cycle can repeat.");
			}
		}
	}
}
