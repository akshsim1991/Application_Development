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
			_mainForm.ShowStartScreen();
		}
		_mainForm.Shown += delegate
		{
			_mainForm.StartDailyUpdateCheck();
		};
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
		string report = ErrorLog.Write(ex, "Unhandled error");
		using ErrorDialog errorDialog = new ErrorDialog(report, "Something went wrong inside PneuSim, but your work is safe: it has been saved to the recovery file and the simulation was stopped.\r\n\r\nThe details below were saved to a log on this computer (" + ErrorLog.Folder + "). Nothing is sent anywhere. Use \"Copy error report\" if you want to send it to the developer.");
		errorDialog.ShowDialog();
	}
}
