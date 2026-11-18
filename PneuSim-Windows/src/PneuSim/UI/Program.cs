using System;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
internal sealed class Program
{
	private static MainForm _mainForm;

	[STAThread]
	public static void Main(string[] args)
	{
		Application.EnableVisualStyles();
		Application.SetCompatibleTextRenderingDefault(defaultValue: false);
		Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
		Application.ThreadException += [SpecialName] (object s, ThreadExceptionEventArgs e) =>
		{
			ReportError(e.Exception);
		};
		AppDomain.CurrentDomain.UnhandledException += [SpecialName] (object s, UnhandledExceptionEventArgs e) =>
		{
			ReportError(e.ExceptionObject as Exception);
		};
		_mainForm = new MainForm();
		if (args.Length > 0 && File.Exists(args[0]))
		{
			_mainForm.OpenFile(args[0]);
		}
		else
		{
			_mainForm.OfferRecovery();
		}
		Application.Run(_mainForm);
	}

	private static void ReportError(Exception ex)
	{
		try
		{
			_mainForm?.RecoverFromError();
		}
		catch (Exception projectError)
		{
			ProjectData.SetProjectError(projectError);
			ProjectData.ClearProjectError();
		}
		MessageBox.Show("Something went wrong inside PneuSim, but your work is safe: it has been saved to the recovery file and the simulation was stopped.\r\n\r\nDetails: " + (ex?.Message ?? "unknown error"), "PneuSim", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
	}
}
