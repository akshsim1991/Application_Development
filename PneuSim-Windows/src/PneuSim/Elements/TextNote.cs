using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Design;

namespace PneuSim;

public class TextNote : CircuitElement
{
	private static readonly Graphics Measure = Graphics.FromImage(new Bitmap(1, 1));

	private string _text;

	private float _size;

	public override string TypeName { get; }

	public override string DisplayName { get; }

	[Category("Text")]
	[DisplayName("Content")]
	[Description("The text to show. Use Shift+Enter or the ... button for several lines.")]
	[Editor("System.ComponentModel.Design.MultilineStringEditor, System.Design", typeof(UITypeEditor))]
	public string Text
	{
		get
		{
			return _text;
		}
		set
		{
			_text = value ?? "";
		}
	}

	[Category("Text")]
	[DisplayName("Font size")]
	public float FontSize
	{
		get
		{
			return _size;
		}
		set
		{
			_size = Math.Max(6f, Math.Min(48f, value));
		}
	}

	[Category("Text")]
	[DisplayName("Bold")]
	public bool Bold { get; set; }

	public override RectangleF LocalBounds
	{
		get
		{
			using Font font = MakeFont();
			lock (Measure)
			{
				SizeF sizeF = Measure.MeasureString((_text.Length == 0) ? " " : _text, font);
				return new RectangleF(0f, 0f, Math.Max(10f, sizeF.Width), Math.Max(10f, sizeF.Height));
			}
		}
	}

	public TextNote()
	{
		_text = "Text";
		_size = 10f;
		TypeName = "TextNote";
		DisplayName = "Text";
	}

	private Font MakeFont()
	{
		return new Font("Segoe UI", _size, Bold ? FontStyle.Bold : FontStyle.Regular);
	}

	public override void DrawSymbol(DrawSurface g, RenderContext r)
	{
		using Font font = MakeFont();
		g.DrawString(_text, font, Brushes.Black, 0f, 0f);
	}
}
