// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace MemoryProfiler2
{
	public abstract class FElfSymbolParser : ISymbolParser
	{
		// each platform needs to return the location to the Nm that can parse the data
		public abstract string GetNmPath();

		public virtual ulong GetSymbolOffset()
		{
			// look for some default metadata to compute an offset by comparing runtime address with nm-discovered address
			string OffsetSymbolName = null;
			string OffsetRuntimeAddressString = null; 
			if (FStreamInfo.GlobalInstance.MetaData.TryGetValue("ModuleOffsetSymbolName", out OffsetSymbolName) &&
				FStreamInfo.GlobalInstance.MetaData.TryGetValue("ModuleOffsetRuntimeAddress", out OffsetRuntimeAddressString))
			{
				// convert string to int
				ulong RuntimeAddress = ulong.Parse(OffsetRuntimeAddressString);

				// find the matching symbol
				int Index = 0;
				foreach (string NmSymbol in Symbols)
				{
					if (NmSymbol == OffsetSymbolName)
					{
						// return the offset to go from nm address to runtime address
						return RuntimeAddress - Addresses[Index];
					}
					Index++;
				}
			}

			return 0;
		}

		public virtual string GetDebugInfoPath(string ExecutableName, out bool bVerifyInfoWithUser)
		{
			// since we are only going on the path, we can't guarantee that it's the correct version (could be newer)
			// we'd have to transmit some extra data all the way through external means, like the PS4 does
			bVerifyInfoWithUser = true;

			// try to use a generic metadata value to get the debug info
			string RuntimeExecutablePath = null;
			if (FStreamInfo.GlobalInstance.MetaData.TryGetValue("ExecutablePath", out RuntimeExecutablePath))
			{
				// and change the extension based on the platforms debug info extension
				return Path.ChangeExtension(RuntimeExecutablePath, GetDebugInfoExtension());
			}

			return "";
		}
		public abstract string GetDebugInfoExtension();

		public override bool InitializeSymbolService(string ExecutableName, FUIBroker UIBroker)
		{
			// ask the platform code for the actual path
			string NmPath = GetNmPath();
			bool bVerifyInfoWithUser;
			string DebugInfoPath = GetDebugInfoPath(ExecutableName, out bVerifyInfoWithUser);

			// Nm must exist!!!
			if (!File.Exists(NmPath))
			{
				return false;
			}

			// if the platform couldn't guarantee where the debug info was, let the user select it
			if (string.IsNullOrEmpty(DebugInfoPath) || !File.Exists(DebugInfoPath) || bVerifyInfoWithUser)
			{
				var DebugInfoFileDialog = new OpenFileDialog();
				DebugInfoFileDialog.Title = "Open the debug info (or executable) that this profile was generated from";
				DebugInfoFileDialog.Filter = string.Format("Debugging Info (*.{0})|*.{0}", GetDebugInfoExtension());
				DebugInfoFileDialog.FileName = String.Format("{0}.{1}", ExecutableName, GetDebugInfoExtension());
				DebugInfoFileDialog.SupportMultiDottedExtensions = true;
				DebugInfoFileDialog.RestoreDirectory = false;

				// prepare the file if we had a valid file already
				if (!string.IsNullOrEmpty(DebugInfoPath) && File.Exists(DebugInfoPath))
				{
					DebugInfoFileDialog.Title = "Verify the expected debug info below is correct:";
					DebugInfoFileDialog.InitialDirectory = Path.GetDirectoryName(DebugInfoPath);
					DebugInfoFileDialog.FileName = Path.GetFileName(DebugInfoPath);
				}

				DebugInfoPath = UIBroker.ShowOpenFileDialog(DebugInfoFileDialog);
				
				// early out if we canceled
				if (!File.Exists(DebugInfoPath))
				{
					return false;
				}
			}

			string Args = string.Format("-n -C \"{0}\"", DebugInfoPath);
			string Output = RunLocalProcessAndReturnStdOut(NmPath, Args);

			// parse the output a line at a time
			using (StringReader sr = new StringReader(Output))
			{
				string Line;
				while ((Line = sr.ReadLine()) != null)
				{
					if (Line.StartsWith("0"))
					{
						try
						{
							// break it apart
							ulong Address = ulong.Parse(Line.Substring(0, 16), System.Globalization.NumberStyles.HexNumber);
							string Symbol = Line.Substring(19);

							// add to the lists
							Addresses.Add(Address);
							Symbols.Add(Symbol);
						}
						catch (Exception)
						{

						}
					}
				}
			}

			// now we can cache off the offset, and let the platform calculate it (could be slow)
			CachedSymbolOffset = GetSymbolOffset();

			return true;
		}

		public static string RunLocalProcessAndReturnStdOut(string Command, string Args)
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo(Command, Args);
			StartInfo.UseShellExecute = false;
			StartInfo.RedirectStandardOutput = true;
			StartInfo.CreateNoWindow = true;

			string FullOutput = "";
			using (System.Diagnostics.Process LocalProcess = System.Diagnostics.Process.Start(StartInfo))
			{
				StreamReader OutputReader = LocalProcess.StandardOutput;
				// trim off any extraneous new lines, helpful for those one-line outputs
				FullOutput = OutputReader.ReadToEnd().Trim();
			}

			return FullOutput;
		}

		public override bool ResolveAddressToSymboInfo(ESymbolResolutionMode SymbolResolutionMode, ulong Address, out string OutFileName, out string OutFunction, out int OutLineNumber)
		{
			OutFileName = null;
			OutFunction = null;
			OutLineNumber = 0;

			// subtract off a known amount for where we were loaded
			Address -= CachedSymbolOffset;

			// verify the address
			if (Address < 0 || Address > Addresses[Addresses.Count - 1])
			{
				return false;
			}

			// fast binary search of the address to symbol
			int FoundIndex = Addresses.BinarySearch(Address);
			// negative means that the result is the binary complement of the one _bigger_ than the value, we want the one lower
			if (FoundIndex < 0)
			{
				FoundIndex = ~FoundIndex;

				// if the index is Count, then we didn't find it
				if (FoundIndex == Addresses.Count)
				{
					return false;
				}

				// move to the one before it
				FoundIndex -= 1;
			}

			// return the matching symbol
			OutFunction = Symbols[FoundIndex];
			return true;
		}

		// the symbols may be relocated at runtime compared to what NM expects. this will address that offset (needs to come from mprof runtime data)
		protected ulong CachedSymbolOffset;

		protected List<ulong> Addresses = new List<ulong>();
		protected List<string> Symbols = new List<string>();

	}
}
