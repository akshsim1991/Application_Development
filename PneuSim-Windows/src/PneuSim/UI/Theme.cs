using System.Drawing;
using System.Runtime.CompilerServices;
using System.Windows.Forms;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

[StandardModule]
public sealed class Theme
{
	/// <summary>Maps a dark-theme colour to the AMOLED (pure black) version when that theme is on.</summary>
	public static Color Dk(Color c)
	{
		if (!AppSettings.Amoled)
		{
			return c;
		}
		int num = (c.R + c.G + c.B) / 3;
		if (num <= 34)
		{
			return Color.FromArgb(c.A, 6, 6, 8);
		}
		if (num <= 42)
		{
			return Color.FromArgb(c.A, 0, 0, 0);
		}
		if (num <= 60)
		{
			return Color.FromArgb(c.A, 18, 19, 22);
		}
		return c;
	}

	public static Color DarkBack => Dk(Color.FromArgb(37, 39, 46));

	public static readonly Color DarkFore = Color.FromArgb(225, 228, 235);

	public static Color DarkField => Dk(Color.FromArgb(28, 30, 36));

	public static Color DarkHeader => Dk(Color.FromArgb(52, 56, 66));

	public static void Apply(Control root)
	{
		if (AppSettings.DarkMode)
		{
			root.BackColor = DarkBack;
			root.ForeColor = DarkFore;
			ApplyChildren(root.Controls);
		}
	}

	private static void ApplyChildren(Control.ControlCollection controls)
	{
		foreach (Control control in controls)
		{
			bool flag = true;
			if (flag == control is TextBoxBase || flag == control is ListBox || flag == control is ListView || flag == control is ComboBox || flag == control is NumericUpDown)
			{
				control.BackColor = DarkField;
				control.ForeColor = DarkFore;
			}
			else if (flag == control is DataGridView)
			{
				DataGridView obj = (DataGridView)control;
				obj.BackgroundColor = DarkField;
				obj.GridColor = DarkHeader;
				obj.EnableHeadersVisualStyles = false;
				obj.DefaultCellStyle.BackColor = DarkField;
				obj.DefaultCellStyle.ForeColor = DarkFore;
				obj.DefaultCellStyle.SelectionBackColor = Color.FromArgb(62, 68, 82);
				obj.ColumnHeadersDefaultCellStyle.BackColor = DarkHeader;
				obj.ColumnHeadersDefaultCellStyle.ForeColor = DarkFore;
				obj.RowHeadersDefaultCellStyle.BackColor = DarkHeader;
			}
			else if (flag == control is PropertyGrid)
			{
				PropertyGrid obj2 = (PropertyGrid)control;
				obj2.ViewBackColor = DarkField;
				obj2.ViewForeColor = DarkFore;
				obj2.LineColor = DarkHeader;
				obj2.CategoryForeColor = DarkFore;
				obj2.HelpBackColor = DarkBack;
				obj2.HelpForeColor = DarkFore;
				obj2.BackColor = DarkBack;
			}
			else if (flag == control is Button)
			{
				control.BackColor = DarkHeader;
				control.ForeColor = DarkFore;
				((Button)control).FlatStyle = FlatStyle.Flat;
				((Button)control).FlatAppearance.BorderColor = Dk(Color.FromArgb(78, 84, 98));
			}
			else if (flag == control is ThemedTabControl)
			{
				control.BackColor = DarkBack;
				control.ForeColor = DarkFore;
				control.Invalidate();
			}
			else if (flag == control is TabControl)
			{
				OwnerDrawTabs((TabControl)control);
				control.BackColor = DarkBack;
				control.ForeColor = DarkFore;
			}
			else if (flag != control is PictureBox)
			{
				control.BackColor = DarkBack;
				control.ForeColor = DarkFore;
			}
			if (control.HasChildren)
			{
				ApplyChildren(control.Controls);
			}
		}
	}

	public static void OwnerDrawTabs(TabControl tc)
	{
		if (tc.DrawMode == TabDrawMode.OwnerDrawFixed)
		{
			return;
		}
		tc.DrawMode = TabDrawMode.OwnerDrawFixed;
		tc.DrawItem += [SpecialName] (object s, DrawItemEventArgs e) =>
		{
			bool darkMode = AppSettings.DarkMode;
			bool flag = e.Index == tc.SelectedIndex;
			Color color = ((!darkMode) ? (flag ? SystemColors.Window : SystemColors.Control) : (flag ? DarkHeader : DarkBack));
			Color foreColor = (darkMode ? DarkFore : SystemColors.ControlText);
			using (SolidBrush brush = new SolidBrush(color))
			{
				e.Graphics.FillRectangle(brush, e.Bounds);
			}
			TextRenderer.DrawText(e.Graphics, tc.TabPages[e.Index].Text, tc.Font, e.Bounds, foreColor, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
		};
	}

	[System.Runtime.InteropServices.DllImport("uxtheme.dll", CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
	private static extern int SetWindowTheme(System.IntPtr hwnd, string subAppName, string subIdList);

	private static readonly System.Collections.Generic.HashSet<Control> Watched = new System.Collections.Generic.HashSet<Control>();

	/// <summary>Makes the native scroll bars of every control follow the dark or light theme (Windows 10 and later; ignored elsewhere).</summary>
	public static void ScrollBars(Control root, bool dark)
	{
		try
		{
			if (root.IsHandleCreated)
			{
				SetWindowTheme(root.Handle, dark ? "DarkMode_Explorer" : "Explorer", null);
			}
			else if (Watched.Add(root))
			{
				root.HandleCreated += delegate
				{
					try
					{
						SetWindowTheme(root.Handle, AppSettings.DarkMode ? "DarkMode_Explorer" : "Explorer", null);
					}
					catch (System.Exception)
					{
					}
				};
			}
		}
		catch (System.Exception)
		{
		}
		foreach (Control child in root.Controls)
		{
			ScrollBars(child, dark);
		}
	}
}
