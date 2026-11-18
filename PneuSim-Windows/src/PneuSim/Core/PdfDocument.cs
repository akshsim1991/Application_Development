using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using Microsoft.VisualBasic.CompilerServices;

namespace PneuSim;

public class PdfDocument
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__3_002D0
	{
		public Encoding _0024VB_0024Local_latin1;

		public _Closure_0024__3_002D0(_Closure_0024__3_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_latin1 = arg0._0024VB_0024Local_latin1;
			}
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__3_002D1
	{
		public FileStream _0024VB_0024Local_fs;

		public _Closure_0024__3_002D0 _0024VB_0024NonLocal__0024VB_0024Closure_2;

		public _Closure_0024__3_002D1(_Closure_0024__3_002D1 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_fs = arg0._0024VB_0024Local_fs;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0(string s)
		{
			byte[] bytes = _0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_latin1.GetBytes(s);
			_0024VB_0024Local_fs.Write(bytes, 0, bytes.Length);
		}
	}

	[CompilerGenerated]
	internal sealed class _Closure_0024__3_002D2
	{
		public List<long> _0024VB_0024Local_offsets;

		public VB_0024AnonymousDelegate_2<string> _0024VB_0024Local_write;

		public _Closure_0024__3_002D1 _0024VB_0024NonLocal__0024VB_0024Closure_3;

		public _Closure_0024__3_002D2(_Closure_0024__3_002D2 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_offsets = arg0._0024VB_0024Local_offsets;
				_0024VB_0024Local_write = arg0._0024VB_0024Local_write;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__1(int objNo)
		{
			while (_0024VB_0024Local_offsets.Count < objNo)
			{
				_0024VB_0024Local_offsets.Add(0L);
			}
			_0024VB_0024Local_offsets[checked(objNo - 1)] = _0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_fs.Position;
			_0024VB_0024Local_write($"{objNo} 0 obj" + "\n");
		}
	}

	private readonly List<PdfPage> _pages;

	public PdfDocument()
	{
		_pages = new List<PdfPage>();
	}

	public IReadOnlyList<PdfPage> Pages => _pages;

	public PdfPage AddPage(float width, float height)
	{
		PdfPage pdfPage = new PdfPage(width, height);
		_pages.Add(pdfPage);
		return pdfPage;
	}

	public void Save(string path)
	{
		_Closure_0024__3_002D0 arg = new _Closure_0024__3_002D0(null);
		arg._0024VB_0024Local_latin1 = Encoding.GetEncoding(28591);
		_Closure_0024__3_002D1 arg2 = new _Closure_0024__3_002D1(null);
		arg2._0024VB_0024NonLocal__0024VB_0024Closure_2 = arg;
		arg2._0024VB_0024Local_fs = new FileStream(path, FileMode.Create, FileAccess.Write);
		checked
		{
			try
			{
				_Closure_0024__3_002D2 arg3 = default(_Closure_0024__3_002D2);
				_Closure_0024__3_002D2 CS_0024_003C_003E8__locals0 = new _Closure_0024__3_002D2(arg3);
				CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_3 = arg2;
				CS_0024_003C_003E8__locals0._0024VB_0024Local_offsets = new List<long>();
				CS_0024_003C_003E8__locals0._0024VB_0024Local_write = [SpecialName] (string s) =>
				{
					byte[] bytes2 = CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_latin1.GetBytes(s);
					CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_fs.Write(bytes2, 0, bytes2.Length);
				};
				VB_0024AnonymousDelegate_3<int> vB_0024AnonymousDelegate_ = [SpecialName] (int objNo) =>
				{
					while (CS_0024_003C_003E8__locals0._0024VB_0024Local_offsets.Count < objNo)
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_offsets.Add(0L);
					}
					CS_0024_003C_003E8__locals0._0024VB_0024Local_offsets[objNo - 1] = CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_fs.Position;
					CS_0024_003C_003E8__locals0._0024VB_0024Local_write($"{objNo} 0 obj" + "\n");
				};
				CS_0024_003C_003E8__locals0._0024VB_0024Local_write("%PDF-1.4\n%âãÏÓ\n");
				int count = _pages.Count;
				int num2 = 5 + count * 2;
				List<List<int>> list = new List<List<int>>();
				foreach (PdfPage page in _pages)
				{
					List<int> list2 = new List<int>();
					foreach (PdfImage image in page.Images)
					{
						_ = image;
						list2.Add(num2);
						num2++;
					}
					list.Add(list2);
				}
				vB_0024AnonymousDelegate_(1);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_write("<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
				string arg4 = string.Join(" ", from i in Enumerable.Range(0, count)
					select $"{5 + i * 2} 0 R");
				vB_0024AnonymousDelegate_(2);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_write($"<< /Type /Pages /Kids [{arg4}] /Count {count} >>" + "\nendobj\n");
				vB_0024AnonymousDelegate_(3);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_write("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>\nendobj\n");
				vB_0024AnonymousDelegate_(4);
				CS_0024_003C_003E8__locals0._0024VB_0024Local_write("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>\nendobj\n");
				int num3 = count - 1;
				for (int j = 0; j <= num3; j++)
				{
					PdfPage pdfPage = _pages[j];
					int num4 = 5 + j * 2;
					vB_0024AnonymousDelegate_(num4);
					string arg5 = ((pdfPage.Images.Count == 0) ? "" : (" /XObject << " + string.Join(" ", list[j].Select([SpecialName] (int num, int k) => $"/Im{k} {num} 0 R")) + " >>"));
					CS_0024_003C_003E8__locals0._0024VB_0024Local_write($"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {F(pdfPage.Width)} {F(pdfPage.Height)}] " + $"/Resources << /Font << /F1 3 0 R /F2 4 0 R >>{arg5} >> /Contents {num4 + 1} 0 R >>" + "\nendobj\n");
					byte[] bytes = CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024NonLocal__0024VB_0024Closure_2._0024VB_0024Local_latin1.GetBytes(pdfPage.Content.ToString());
					vB_0024AnonymousDelegate_(num4 + 1);
					CS_0024_003C_003E8__locals0._0024VB_0024Local_write($"<< /Length {bytes.Length} >>" + "\nstream\n");
					CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_fs.Write(bytes, 0, bytes.Length);
					CS_0024_003C_003E8__locals0._0024VB_0024Local_write("\nendstream\nendobj\n");
				}
				int num5 = count - 1;
				for (int l = 0; l <= num5; l++)
				{
					int num6 = _pages[l].Images.Count - 1;
					for (int m = 0; m <= num6; m++)
					{
						PdfImage pdfImage = _pages[l].Images[m];
						vB_0024AnonymousDelegate_(list[l][m]);
						CS_0024_003C_003E8__locals0._0024VB_0024Local_write($"<< /Type /XObject /Subtype /Image /Width {pdfImage.PixelWidth} /Height {pdfImage.PixelHeight} /ColorSpace /DeviceRGB " + $"/BitsPerComponent 8 /Filter /FlateDecode /Length {pdfImage.Data.Length} >>" + "\nstream\n");
						CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_fs.Write(pdfImage.Data, 0, pdfImage.Data.Length);
						CS_0024_003C_003E8__locals0._0024VB_0024Local_write("\nendstream\nendobj\n");
					}
				}
				long position = CS_0024_003C_003E8__locals0._0024VB_0024NonLocal__0024VB_0024Closure_3._0024VB_0024Local_fs.Position;
				CS_0024_003C_003E8__locals0._0024VB_0024Local_write("xref\n" + $"0 {CS_0024_003C_003E8__locals0._0024VB_0024Local_offsets.Count + 1}" + "\n0000000000 65535 f \n");
				foreach (long _0024VB_0024Local_offset in CS_0024_003C_003E8__locals0._0024VB_0024Local_offsets)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_write(_0024VB_0024Local_offset.ToString("0000000000") + " 00000 n \n");
				}
				CS_0024_003C_003E8__locals0._0024VB_0024Local_write("trailer\n" + $"<< /Size {CS_0024_003C_003E8__locals0._0024VB_0024Local_offsets.Count + 1} /Root 1 0 R >>" + "\nstartxref\n" + Conversions.ToString(position) + "\n%%EOF\n");
			}
			finally
			{
				if (arg2._0024VB_0024Local_fs != null)
				{
					((IDisposable)arg2._0024VB_0024Local_fs).Dispose();
				}
			}
		}
	}

	internal static string F(float v)
	{
		return v.ToString("0.###", CultureInfo.InvariantCulture);
	}
}
