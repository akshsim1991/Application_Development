using System.Drawing;
using System.Windows.Forms;

namespace PneuSim;

/// <summary>Tab strip painted by PneuSim in flat colours that follow the light, dark or AMOLED theme (no system borders).</summary>
public sealed class ThemedTabControl : TabControl
{
	public ThemedTabControl()
	{
		SetStyle(ControlStyles.UserPaint | ControlStyles.AllPaintingInWmPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, value: true);
	}

	protected override void OnPaint(PaintEventArgs e)
	{
		bool dark = AppSettings.DarkMode;
		Color back = dark ? Theme.DarkBack : SystemColors.Control;
		Color fore = dark ? Theme.DarkFore : SystemColors.ControlText;
		Color sel = dark ? Theme.DarkHeader : SystemColors.Window;
		Color accent = dark ? Color.FromArgb(70, 200, 210) : Color.FromArgb(0, 118, 132);
		Graphics g = e.Graphics;
		g.Clear(back);
		for (int i = 0; i < TabCount; i++)
		{
			Rectangle r = GetTabRect(i);
			bool s = i == SelectedIndex;
			using (SolidBrush b = new SolidBrush(s ? sel : back))
			{
				g.FillRectangle(b, r);
			}
			if (s)
			{
				using SolidBrush ab = new SolidBrush(accent);
				g.FillRectangle(ab, r.X, (Alignment == TabAlignment.Bottom) ? r.Y : (r.Bottom - 2), r.Width, 2);
			}
			TextRenderer.DrawText(g, TabPages[i].Text, Font, r, s ? accent : fore, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
		}
	}
}
