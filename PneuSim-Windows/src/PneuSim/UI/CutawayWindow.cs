using System;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Windows.Forms;

namespace PneuSim;

public class CutawayWindow : Form
{
	private readonly CutawayView _view;

	private readonly Timer _timer;

	public Func<CircuitElement> ElementSource { get; set; }

	public Func<bool> SimulatingSource { get; set; }

	public CutawayWindow()
	{
		_view = new CutawayView
		{
			Dock = DockStyle.Fill
		};
		_timer = new Timer
		{
			Interval = 40
		};
		Text = "Cutaway view";
		Font = new Font("Segoe UI", 9f);
		base.Size = new Size(600, 340);
		base.FormBorderStyle = FormBorderStyle.SizableToolWindow;
		base.StartPosition = FormStartPosition.Manual;
		base.ShowInTaskbar = false;
		base.KeyPreview = true;
		base.Controls.Add(_view);
		base.KeyDown += [SpecialName] (object s, KeyEventArgs e) =>
		{
			if (e.KeyCode == Keys.Escape)
			{
				Close();
			}
		};
		_timer.Tick += [SpecialName] (object a0, EventArgs a1) =>
		{
			_Lambda_0024__10_002D1();
		};
		_timer.Start();
	}

	protected override void OnFormClosed(FormClosedEventArgs e)
	{
		_timer.Stop();
		_timer.Dispose();
		base.OnFormClosed(e);
	}

	[SpecialName]
	[CompilerGenerated]
	private void _Lambda_0024__10_002D1()
	{
		_view.Element = ((ElementSource == null) ? null : ElementSource());
		_view.Simulating = SimulatingSource != null && SimulatingSource();
		_view.Animate(0.04);
	}
}
