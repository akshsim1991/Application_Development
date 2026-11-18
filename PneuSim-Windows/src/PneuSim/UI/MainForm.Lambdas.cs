using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows.Forms;
using Microsoft.VisualBasic;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public partial class MainForm
{

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__86_002D0()
	{
		_checkTimer.Stop();
		RunStaticChecks();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__86_002D1()
	{
		ApplyFix(SelectedIssue()?.Fix);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__86_002D2()
	{
		CheckIssue checkIssue = SelectedIssue();
		if (checkIssue != null)
		{
			ApplyFix(CircuitAnalysis.ElectroFix(checkIssue));
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__117_002D0()
	{
		if (_running)
		{
			_simulator.RunLogic();
		}
		_canvas.Invalidate();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D0()
	{
		OnTroubleCommand(null, "start");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D1()
	{
		OnTroubleCommand(null, "hint");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D2()
	{
		OnTroubleCommand(null, "found");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D3()
	{
		OnTroubleCommand(null, "giveup");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D4()
	{
		OnTroubleCommand(null, "add");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__118_002D5()
	{
		OnTroubleCommand(null, "clear");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D0()
	{
		ReplayStep(-1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D1()
	{
		ReplayStep(1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D2()
	{
		ReplayEvent(-1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D3()
	{
		ReplayEvent(1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__128_002D4()
	{
		if (!_updatingTimeline && _recorder != null)
		{
			PauseForReplay();
			_recorder.Restore(_timeline.Value);
			AfterReplayMove();
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__167_002D0()
	{
		_properties.SelectedObjects = _canvas.SelectedElements.Cast<object>().ToArray();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__167_002D3()
	{
		_statusZoom.Text = $"Zoom {_canvas.Zoom * 100f:0}%";
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__167_002D4()
	{
		if (!_switchingPages)
		{
			ShowPage(_pageTabs.SelectedIndex);
		}
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__168_002D2()
	{
		OnLibraryActivate(null, EventArgs.Empty);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D3()
	{
		_canvas.CutSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D4()
	{
		_canvas.CopySelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D5()
	{
		_canvas.Paste();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D6()
	{
		_canvas.DuplicateSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D7()
	{
		_canvas.SelectAll();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D8()
	{
		_canvas.DeleteSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D9()
	{
		_canvas.RotateSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D13()
	{
		ShowPage(checked(Math.Min(_project.Pages.Count - 1, _pageIndex + 1)));
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D14()
	{
		ShowPage(Math.Max(0, checked(_pageIndex - 1)));
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D15()
	{
		SetZoom(_canvas.Zoom * 1.2f);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D16()
	{
		SetZoom(_canvas.Zoom / 1.2f);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D17()
	{
		SetZoom(1f);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D20()
	{
		AppSettings.ThemeMode = (AppSettings.DarkMode ? "Light" : "Dark");
		ApplyTheme(!AppSettings.DarkMode);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D21()
	{
		ReplayStep(-1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D22()
	{
		ReplayStep(1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D23()
	{
		ReplayEvent(-1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D24()
	{
		ReplayEvent(1);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__172_002D25()
	{
		ShowBottomTab("Getting started");
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__173_002D2()
	{
		_canvas.RotateSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__173_002D3()
	{
		_canvas.DeleteSelection();
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__173_002D4()
	{
		SetZoom(_canvas.Zoom * 1.2f);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__173_002D5()
	{
		SetZoom(_canvas.Zoom / 1.2f);
	}
}
