using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace PneuSim;

public class GifRecorder
{
	[CompilerGenerated]
	internal sealed class _Closure_0024__19_002D0
	{
		public int _0024VB_0024Local_bitBuffer;

		public int _0024VB_0024Local_bitCount;

		public int _0024VB_0024Local_codeSize;

		public List<byte> _0024VB_0024Local_output;

		public _Closure_0024__19_002D0(_Closure_0024__19_002D0 arg0)
		{
			if (arg0 != null)
			{
				_0024VB_0024Local_bitBuffer = arg0._0024VB_0024Local_bitBuffer;
				_0024VB_0024Local_bitCount = arg0._0024VB_0024Local_bitCount;
				_0024VB_0024Local_codeSize = arg0._0024VB_0024Local_codeSize;
				_0024VB_0024Local_output = arg0._0024VB_0024Local_output;
			}
		}

		[SpecialName]
		internal void _Lambda_0024__0(int code)
		{
			_0024VB_0024Local_bitBuffer |= code << _0024VB_0024Local_bitCount;
			checked
			{
				_0024VB_0024Local_bitCount += _0024VB_0024Local_codeSize;
				while (_0024VB_0024Local_bitCount >= 8)
				{
					_0024VB_0024Local_output.Add((byte)(_0024VB_0024Local_bitBuffer & 0xFF));
					_0024VB_0024Local_bitBuffer >>= 8;
					_0024VB_0024Local_bitCount -= 8;
				}
			}
		}
	}

	private const int MaxFrames = 900;

	private readonly List<byte[]> _frames;

	private readonly byte[] _palette;

	public int Width { get; }

	public int Height { get; }

	public int DelayCs { get; }

	public int FrameCount => _frames.Count;

	public GifRecorder(int width, int height, int delayCs = 8)
	{
		_frames = new List<byte[]>();
		_palette = BuildPalette();
		Width = width;
		Height = height;
		DelayCs = delayCs;
	}

	private static byte[] BuildPalette()
	{
		byte[] array = new byte[768];
		int i = 0;
		int num = 0;
		checked
		{
			do
			{
				int num2 = 0;
				do
				{
					int num3 = 0;
					do
					{
						array[i * 3] = (byte)(num * 51);
						array[i * 3 + 1] = (byte)unchecked(checked(num2 * 255) / 6);
						array[i * 3 + 2] = (byte)(num3 * 51);
						i++;
						num3++;
					}
					while (num3 <= 5);
					num2++;
				}
				while (num2 <= 6);
				num++;
			}
			while (num <= 5);
			for (; i < 256; i++)
			{
				byte b = (byte)((i - 252) * 64 + 32);
				array[i * 3] = b;
				array[i * 3 + 1] = b;
				array[i * 3 + 2] = b;
			}
			return array;
		}
	}

	private static byte IndexOf(int r, int gr, int b)
	{
		checked
		{
			return (byte)(unchecked(checked(r + 25) / 51) * 42 + unchecked(checked(gr * 6 + 127) / 255) * 6 + unchecked(checked(b + 25) / 51));
		}
	}

	public bool AddFrame(Bitmap bmp)
	{
		if (_frames.Count >= 900)
		{
			return false;
		}
		Rectangle rect = new Rectangle(0, 0, Width, Height);
		checked
		{
			using (Bitmap bitmap = new Bitmap(Width, Height, PixelFormat.Format24bppRgb))
			{
				using (Graphics graphics = Graphics.FromImage(bitmap))
				{
					graphics.Clear(Color.White);
					graphics.DrawImage(bmp, 0, 0, Width, Height);
				}
				BitmapData bitmapData = bitmap.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format24bppRgb);
				byte[] array = new byte[bitmapData.Stride * Height - 1 + 1];
				Marshal.Copy(bitmapData.Scan0, array, 0, array.Length);
				bitmap.UnlockBits(bitmapData);
				byte[] array2 = new byte[Width * Height - 1 + 1];
				int num = Height - 1;
				for (int i = 0; i <= num; i++)
				{
					int num2 = i * bitmapData.Stride;
					int num3 = Width - 1;
					for (int j = 0; j <= num3; j++)
					{
						int num4 = num2 + j * 3;
						array2[i * Width + j] = IndexOf(array[num4 + 2], array[num4 + 1], array[num4]);
					}
				}
				_frames.Add(array2);
			}
			return true;
		}
	}

	public void Save(string path)
	{
		checked
		{
			using FileStream output = new FileStream(path, FileMode.Create);
			using BinaryWriter binaryWriter = new BinaryWriter(output);
			binaryWriter.Write(Encoding.ASCII.GetBytes("GIF89a"));
			binaryWriter.Write((ushort)Width);
			binaryWriter.Write((ushort)Height);
			binaryWriter.Write((byte)247);
			binaryWriter.Write((byte)0);
			binaryWriter.Write((byte)0);
			binaryWriter.Write(_palette);
			binaryWriter.Write(new byte[3] { 33, 255, 11 });
			binaryWriter.Write(Encoding.ASCII.GetBytes("NETSCAPE2.0"));
			binaryWriter.Write(new byte[5] { 3, 1, 0, 0, 0 });
			foreach (byte[] frame in _frames)
			{
				binaryWriter.Write(new byte[4] { 33, 249, 4, 0 });
				binaryWriter.Write((ushort)DelayCs);
				binaryWriter.Write(new byte[2]);
				binaryWriter.Write((byte)44);
				binaryWriter.Write((ushort)0);
				binaryWriter.Write((ushort)0);
				binaryWriter.Write((ushort)Width);
				binaryWriter.Write((ushort)Height);
				binaryWriter.Write((byte)0);
				binaryWriter.Write((byte)8);
				byte[] array = Compress(frame);
				int num;
				for (int i = 0; i < array.Length; i += num)
				{
					num = Math.Min(255, array.Length - i);
					binaryWriter.Write((byte)num);
					binaryWriter.Write(array, i, num);
				}
				binaryWriter.Write((byte)0);
			}
			binaryWriter.Write((byte)59);
		}
	}

	private static byte[] Compress(byte[] pixels)
	{
		_Closure_0024__19_002D0 arg = default(_Closure_0024__19_002D0);
		_Closure_0024__19_002D0 CS_0024_003C_003E8__locals0 = new _Closure_0024__19_002D0(arg);
		CS_0024_003C_003E8__locals0._0024VB_0024Local_output = new List<byte>();
		CS_0024_003C_003E8__locals0._0024VB_0024Local_bitBuffer = 0;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_bitCount = 0;
		CS_0024_003C_003E8__locals0._0024VB_0024Local_codeSize = 9;
		checked
		{
			VB_0024AnonymousDelegate_18<int> vB_0024AnonymousDelegate_ = [SpecialName] (int code) =>
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_bitBuffer |= code << CS_0024_003C_003E8__locals0._0024VB_0024Local_bitCount;
				CS_0024_003C_003E8__locals0._0024VB_0024Local_bitCount += CS_0024_003C_003E8__locals0._0024VB_0024Local_codeSize;
				while (CS_0024_003C_003E8__locals0._0024VB_0024Local_bitCount >= 8)
				{
					CS_0024_003C_003E8__locals0._0024VB_0024Local_output.Add((byte)(CS_0024_003C_003E8__locals0._0024VB_0024Local_bitBuffer & 0xFF));
					CS_0024_003C_003E8__locals0._0024VB_0024Local_bitBuffer >>= 8;
					CS_0024_003C_003E8__locals0._0024VB_0024Local_bitCount -= 8;
				}
			};
			Dictionary<int, int> dictionary = new Dictionary<int, int>();
			int num = 258;
			vB_0024AnonymousDelegate_(256);
			int num2 = pixels[0];
			int num3 = pixels.Length - 1;
			for (int i = 1; i <= num3; i++)
			{
				int num4 = pixels[i];
				int key = (num2 << 8) | num4;
				if (dictionary.TryGetValue(key, out var value))
				{
					num2 = value;
					continue;
				}
				vB_0024AnonymousDelegate_(num2);
				if (num < 4096)
				{
					dictionary[key] = num;
					num++;
					if (num > 1 << CS_0024_003C_003E8__locals0._0024VB_0024Local_codeSize && CS_0024_003C_003E8__locals0._0024VB_0024Local_codeSize < 12)
					{
						CS_0024_003C_003E8__locals0._0024VB_0024Local_codeSize++;
					}
				}
				else
				{
					vB_0024AnonymousDelegate_(256);
					dictionary.Clear();
					num = 258;
					CS_0024_003C_003E8__locals0._0024VB_0024Local_codeSize = 9;
				}
				num2 = num4;
			}
			vB_0024AnonymousDelegate_(num2);
			vB_0024AnonymousDelegate_(257);
			if (CS_0024_003C_003E8__locals0._0024VB_0024Local_bitCount > 0)
			{
				CS_0024_003C_003E8__locals0._0024VB_0024Local_output.Add((byte)(CS_0024_003C_003E8__locals0._0024VB_0024Local_bitBuffer & 0xFF));
			}
			return CS_0024_003C_003E8__locals0._0024VB_0024Local_output.ToArray();
		}
	}
}
