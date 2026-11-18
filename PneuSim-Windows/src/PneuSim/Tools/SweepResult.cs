using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;

namespace PneuSim;

public class SweepResult
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__9_002D0
	{
		public CultureInfo _0024VB_0024Local_inv;

		public Func<double, string> _0024I1;

		public _Closure_0024__9_002D0(_Closure_0024__9_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_inv = arg0._0024VB_0024Local_inv;
			}
		}

		[SpecialName]
		internal string _Lambda_0024__1(double v)
		{
			if (!double.IsNaN(v))
			{
				return v.ToString("0.####", _0024VB_0024Local_inv);
			}
			return "";
		}
	}

	public List<string> Columns { get; set; }

	public List<double[]> Rows { get; set; }

	public SweepResult()
	{
		Columns = new List<string>();
		Rows = new List<double[]>();
	}

	public string ToCsv()
	{
		_Closure_0024__9_002D0 arg = default(_Closure_0024__9_002D0);
		_Closure_0024__9_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__9_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_inv = CultureInfo.InvariantCulture;
		VB_0024AnonymousDelegate_19<string, string> vB_0024AnonymousDelegate_ = [SpecialName] (string s) => "\"" + s.Replace("\"", "\"\"") + "\"";
		StringBuilder stringBuilder = new StringBuilder();
		List<string> columns = Columns;
		VB_0024AnonymousDelegate_19<string, string> vB_0024AnonymousDelegate_2 = vB_0024AnonymousDelegate_;
		stringBuilder.AppendLine(string.Join(",", columns.Select((vB_0024AnonymousDelegate_2 == null) ? null : new Func<string, string>(vB_0024AnonymousDelegate_2.Invoke))));
		foreach (double[] row in Rows)
		{
			stringBuilder.AppendLine(string.Join(",", row.Select([SpecialName] (double v) => (!double.IsNaN(v)) ? v.ToString("0.####", CS_0024_003C_003E8__locals0._0024VB_0024Local_inv) : "")));
		}
		return stringBuilder.ToString();
	}
}
