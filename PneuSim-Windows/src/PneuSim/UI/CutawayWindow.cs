using System;
using System.Drawing;
using System.Runtime.CompilerServices;
using System.Windows.Forms;

namespace PneuSim;

public class CutawayWindow : Form
{
	private readonly CutawayView _view;

	private readonly Timer _timer;

	private readonly SystemCutawayView _system;

	private readonly ToolStrip _modes;

	private readonly ToolStripButton _single;

	private readonly ToolStripButton _whole;

	/// <summary>The circuit shown in the whole-workspace view (the running simulation's circuit while simulating).</summary>
	public Func<Circuit> CircuitSource { get; set; }

	public void ShowWholeWorkspace(bool whole)
	{
		_whole.Checked = whole;
		_single.Checked = !whole;
		_system.Visible = whole;
		_view.Visible = !whole;
		Text = whole ? "Cutaway view: whole workspace" : "Cutaway view";
		if (whole && Size.Width < 760)
		{
			Size = new Size(900, 560);
		}
	}

	public Func<CircuitElement> ElementSource { get; set; }

	public Func<bool> SimulatingSource { get; set; }

	public CutawayWindow()
	{
		_view = new CutawayView
		{
			Dock = DockStyle.Fill
		};
		_system = new SystemCutawayView
		{
			Dock = DockStyle.Fill,
			Visible = false
		};
		_single = new ToolStripButton("Selected component")
		{
			CheckOnClick = false,
			Checked = true
		};
		_whole = new ToolStripButton("Whole workspace (all components together)");
		_modes = new ToolStrip(_single, _whole)
		{
			GripStyle = ToolStripGripStyle.Hidden,
			Dock = DockStyle.Top
		};
		_single.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			ShowWholeWorkspace(whole: false);
		};
		_whole.Click += [SpecialName] (object a0, EventArgs a1) =>
		{
			ShowWholeWorkspace(whole: true);
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
		base.Controls.Add(_system);
		base.Controls.Add(_view);
		base.Controls.Add(_modes);
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
		bool flag = SimulatingSource != null && SimulatingSource();
		if (_system.Visible)
		{
			_system.Circuit = ((CircuitSource == null) ? null : CircuitSource());
			_system.Simulating = flag;
			_system.Animate(0.04);
			return;
		}
		_view.Element = ((ElementSource == null) ? null : ElementSource());
		_view.Simulating = flag;
		_view.Animate(0.04);
	}
}
