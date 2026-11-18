using System.IO;
using System.IO.Compression;

namespace PneuSim;

public class PdfImage
{
	public int PixelWidth { get; set; }

	public int PixelHeight { get; set; }

	public byte[] Data { get; set; }

	public static byte[] Zlib(byte[] raw)
	{
		using MemoryStream memoryStream = new MemoryStream();
		memoryStream.WriteByte(120);
		memoryStream.WriteByte(156);
		using (DeflateStream deflateStream = new DeflateStream(memoryStream, CompressionLevel.Optimal, leaveOpen: true))
		{
			deflateStream.Write(raw, 0, raw.Length);
		}
		uint num = 1u;
		uint num2 = 0u;
		foreach (byte b in raw)
		{
			num = checked(num + b) % 65521;
			num2 = checked(num2 + num) % 65521;
		}
		uint num3 = (num2 << 16) | num;
		checked
		{
			memoryStream.WriteByte((byte)(num3 >> 24));
			memoryStream.WriteByte((byte)(unchecked((long)(num3 >> 16)) & 0xFFL));
			memoryStream.WriteByte((byte)(unchecked((long)(num3 >> 8)) & 0xFFL));
			memoryStream.WriteByte((byte)(unchecked((long)num3) & 0xFFL));
			return memoryStream.ToArray();
		}
	}
}
