using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

namespace PneuSim;

/// <summary>
/// Small software mixer for PneuSim's synthesised sounds (no sound files). One-shots: valve, thud, hiss.
/// Loops: hum, relief, alarm. Output goes through the Windows waveOut API; on any failure the engine
/// disables itself silently, so a missing sound device never disturbs the simulation.
/// </summary>
public static class SoundEngine
{
	[StructLayout(LayoutKind.Sequential)]
	private struct WaveFormatEx
	{
		public ushort FormatTag;

		public ushort Channels;

		public uint SamplesPerSec;

		public uint AvgBytesPerSec;

		public ushort BlockAlign;

		public ushort BitsPerSample;

		public ushort Size;
	}

	[StructLayout(LayoutKind.Sequential)]
	private struct WaveHdr
	{
		public IntPtr Data;

		public uint BufferLength;

		public uint BytesRecorded;

		public IntPtr User;

		public uint Flags;

		public uint Loops;

		public IntPtr Next;

		public IntPtr Reserved;
	}

	private sealed class Shot
	{
		public float[] Data;

		public int Pos;
	}

	private sealed class Loop
	{
		public float[] Data;

		public int Pos;

		public float Gain;

		public bool On;
	}

	public const int Rate = 22050;

	private const int BufferSamples = 882;

	private const int BufferCount = 3;

	private static readonly object Gate = new object();

	private static readonly Dictionary<string, float[]> OneShots = BuildOneShots();

	private static readonly Dictionary<string, Loop> Loops = BuildLoops();

	private static readonly List<Shot> Active = new List<Shot>();

	private static Thread _thread;

	private static volatile bool _stop;

	private static bool _failed;

	[DllImport("winmm.dll")]
	private static extern int waveOutOpen(out IntPtr handle, int device, ref WaveFormatEx format, IntPtr callback, IntPtr instance, int flags);

	[DllImport("winmm.dll")]
	private static extern int waveOutPrepareHeader(IntPtr handle, IntPtr header, int size);

	[DllImport("winmm.dll")]
	private static extern int waveOutUnprepareHeader(IntPtr handle, IntPtr header, int size);

	[DllImport("winmm.dll")]
	private static extern int waveOutWrite(IntPtr handle, IntPtr header, int size);

	[DllImport("winmm.dll")]
	private static extern int waveOutReset(IntPtr handle);

	[DllImport("winmm.dll")]
	private static extern int waveOutClose(IntPtr handle);

	/// <summary>The sample data of every sound, for tests (names: valve, thud, hiss, hum, relief, alarm).</summary>
	public static float[] Samples(string name)
	{
		if (OneShots.TryGetValue(name, out var value))
		{
			return value;
		}
		return Loops.TryGetValue(name, out var value2) ? value2.Data : null;
	}

	private static Dictionary<string, float[]> BuildOneShots()
	{
		Random random = new Random(11);
		Dictionary<string, float[]> dictionary = new Dictionary<string, float[]>();
		float[] array = new float[Rate * 30 / 1000];
		for (int i = 0; i < array.Length; i++)
		{
			double num = (double)i / Rate;
			array[i] = (float)((random.NextDouble() * 2.0 - 1.0) * Math.Exp((0.0 - num) * 220.0) * 0.5 + Math.Sin(Math.PI * 2.0 * 1800.0 * num) * Math.Exp((0.0 - num) * 160.0) * 0.5);
		}
		dictionary["valve"] = array;
		float[] array2 = new float[Rate * 130 / 1000];
		for (int j = 0; j < array2.Length; j++)
		{
			double num2 = (double)j / Rate;
			array2[j] = (float)(Math.Sin(Math.PI * 2.0 * 70.0 * num2) * Math.Exp((0.0 - num2) * 35.0) * 0.9 + (random.NextDouble() * 2.0 - 1.0) * Math.Exp((0.0 - num2) * 90.0) * 0.2);
		}
		dictionary["thud"] = array2;
		float[] array3 = new float[Rate * 400 / 1000];
		double num3 = 0.0;
		for (int k = 0; k < array3.Length; k++)
		{
			double num4 = (double)k / Rate;
			double num5 = random.NextDouble() * 2.0 - 1.0;
			double num6 = num5 - num3;
			num3 = num5;
			array3[k] = (float)(num6 * 0.5 * Math.Min(1.0, num4 / 0.01) * Math.Exp((0.0 - num4) * 5.0));
		}
		dictionary["hiss"] = array3;
		return dictionary;
	}

	private static Dictionary<string, Loop> BuildLoops()
	{
		Random random = new Random(23);
		Dictionary<string, Loop> dictionary = new Dictionary<string, Loop>();
		float[] array = new float[Rate];
		for (int i = 0; i < array.Length; i++)
		{
			double num = (double)i / Rate;
			array[i] = (float)(Math.Sin(Math.PI * 2.0 * 100.0 * num) * 0.25 + Math.Sin(Math.PI * 2.0 * 200.0 * num) * 0.15 + Math.Sin(Math.PI * 2.0 * 300.0 * num) * 0.07);
		}
		dictionary["hum"] = new Loop { Data = array };
		float[] array2 = new float[Rate / 2];
		int num2 = Rate / 30;
		for (int j = 0; j < array2.Length; j++)
		{
			array2[j] = ((j % num2 < num2 * 4 / 10) ? ((float)(random.NextDouble() * 2.0 - 1.0) * 0.35f) : 0f);
		}
		dictionary["relief"] = new Loop { Data = array2 };
		float[] array3 = new float[Rate];
		for (int k = 0; k < array3.Length; k++)
		{
			double num3 = (double)k / Rate;
			double num4 = num3 % 0.5;
			if (num3 < 0.7 && num4 < 0.2)
			{
				double num5 = Math.PI * 2.0 * 880.0 * num3;
				array3[k] = (float)((Math.Sin(num5) + Math.Sin(3.0 * num5) / 3.0) * 0.4);
			}
		}
		dictionary["alarm"] = new Loop { Data = array3 };
		return dictionary;
	}

	/// <summary>Plays a one-shot sound (valve, thud or hiss) if sounds are enabled.</summary>
	public static void Play(string name)
	{
		if (!AppSettings.IsSoundEnabled(name) || _failed || !OneShots.TryGetValue(name, out var value))
		{
			return;
		}
		StartIfNeeded();
		lock (Gate)
		{
			if (Active.Count < 24)
			{
				Active.Add(new Shot { Data = value });
			}
		}
	}

	/// <summary>Switches a looping sound (hum, relief or alarm) on or off.</summary>
	public static void SetLoop(string name, bool on)
	{
		if (!Loops.TryGetValue(name, out var value))
		{
			return;
		}
		on = on && AppSettings.IsSoundEnabled(name) && !_failed;
		if (on)
		{
			StartIfNeeded();
		}
		lock (Gate)
		{
			value.On = on;
		}
	}

	/// <summary>Silences everything and releases the sound device.</summary>
	public static void Shutdown()
	{
		lock (Gate)
		{
			Active.Clear();
			foreach (Loop value in Loops.Values)
			{
				value.On = false;
				value.Gain = 0f;
			}
		}
		_stop = true;
		Thread thread = _thread;
		if (thread != null && thread.IsAlive)
		{
			thread.Join(500);
		}
		_thread = null;
	}

	private static void StartIfNeeded()
	{
		if (_failed || (_thread != null && _thread.IsAlive) || Environment.OSVersion.Platform != PlatformID.Win32NT)
		{
			if (Environment.OSVersion.Platform != PlatformID.Win32NT)
			{
				_failed = true;
			}
			return;
		}
		_stop = false;
		_thread = new Thread(Run)
		{
			IsBackground = true,
			Priority = ThreadPriority.AboveNormal,
			Name = "PneuSim sound"
		};
		_thread.Start();
	}

	private static void Mix(short[] output)
	{
		float num = (float)AppSettings.SoundVolume / 100f;
		num *= num;
		float num2 = 1f / ((float)Rate * 0.05f);
		lock (Gate)
		{
			for (int i = 0; i < output.Length; i++)
			{
				float num3 = 0f;
				for (int num4 = Active.Count - 1; num4 >= 0; num4--)
				{
					Shot shot = Active[num4];
					num3 += shot.Data[shot.Pos++];
					if (shot.Pos >= shot.Data.Length)
					{
						Active.RemoveAt(num4);
					}
				}
				foreach (Loop value in Loops.Values)
				{
					value.Gain = (value.On ? Math.Min(1f, value.Gain + num2) : Math.Max(0f, value.Gain - num2));
					if (value.Gain > 0f)
					{
						num3 += value.Data[value.Pos] * value.Gain;
						value.Pos = (value.Pos + 1) % value.Data.Length;
					}
				}
				num3 *= num;
				output[i] = (short)(Math.Max(-1f, Math.Min(1f, num3)) * 32000f);
			}
		}
	}

	private static void Run()
	{
		IntPtr handle = IntPtr.Zero;
		IntPtr[] headers = new IntPtr[BufferCount];
		IntPtr[] buffers = new IntPtr[BufferCount];
		int headerSize = Marshal.SizeOf(typeof(WaveHdr));
		int prepared = 0;
		try
		{
			WaveFormatEx format = new WaveFormatEx
			{
				FormatTag = 1,
				Channels = 1,
				SamplesPerSec = Rate,
				AvgBytesPerSec = Rate * 2,
				BlockAlign = 2,
				BitsPerSample = 16,
				Size = 0
			};
			if (waveOutOpen(out handle, -1, ref format, IntPtr.Zero, IntPtr.Zero, 0) != 0)
			{
				_failed = true;
				return;
			}
			short[] array = new short[BufferSamples];
			for (int i = 0; i < BufferCount; i++)
			{
				buffers[i] = Marshal.AllocHGlobal(BufferSamples * 2);
				headers[i] = Marshal.AllocHGlobal(headerSize);
				WaveHdr structure = new WaveHdr
				{
					Data = buffers[i],
					BufferLength = (uint)(BufferSamples * 2)
				};
				Marshal.StructureToPtr(structure, headers[i], fDeleteOld: false);
				waveOutPrepareHeader(handle, headers[i], headerSize);
				prepared++;
				Mix(array);
				Marshal.Copy(array, 0, buffers[i], BufferSamples);
				waveOutWrite(handle, headers[i], headerSize);
			}
			while (!_stop)
			{
				for (int j = 0; j < BufferCount; j++)
				{
					WaveHdr waveHdr = (WaveHdr)Marshal.PtrToStructure(headers[j], typeof(WaveHdr));
					if ((waveHdr.Flags & 1) != 0)
					{
						Mix(array);
						Marshal.Copy(array, 0, buffers[j], BufferSamples);
						waveOutWrite(handle, headers[j], headerSize);
					}
				}
				Thread.Sleep(8);
			}
		}
		catch (Exception)
		{
			_failed = true;
		}
		finally
		{
			try
			{
				if (handle != IntPtr.Zero)
				{
					waveOutReset(handle);
				}
				for (int k = 0; k < BufferCount; k++)
				{
					if (headers[k] != IntPtr.Zero)
					{
						if (handle != IntPtr.Zero && k < prepared)
						{
							waveOutUnprepareHeader(handle, headers[k], headerSize);
						}
						Marshal.FreeHGlobal(headers[k]);
					}
					if (buffers[k] != IntPtr.Zero)
					{
						Marshal.FreeHGlobal(buffers[k]);
					}
				}
				if (handle != IntPtr.Zero)
				{
					waveOutClose(handle);
				}
			}
			catch (Exception)
			{
			}
		}
	}
}
