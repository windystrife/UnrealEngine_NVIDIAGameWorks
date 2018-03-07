// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.Threading;

namespace MemoryProfiler2
{
	/// <summary> Managed version of FCommandLineWrapper. </summary>
	class FManagedCommandLineWrapper
	{
		private Process CmdProcess;

		/// <summary> Constructor. Creates a command line process. </summary>
		public FManagedCommandLineWrapper( string Filename, string Args )
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo( Filename, Args );

			// Don't create any window and redirect all communication.
			StartInfo.CreateNoWindow = true;
			StartInfo.RedirectStandardInput = true;
			StartInfo.RedirectStandardOutput = true;
			StartInfo.UseShellExecute = false;

			CmdProcess = Process.Start( StartInfo );
		}

		/// <summary> Terminates previously created process. </summary>
		public void Terminate()
		{
			CmdProcess.Close();
		}

		/// <summary> Reads a line from the standard output and returns the data as a string. </summary>
		public string ReadLine()
		{
			return CmdProcess.StandardOutput.ReadLine();
		}

		/// <summary> Write a string into the standard input. </summary>
		public void Write( string Str )
		{
			CmdProcess.StandardInput.Write( Str );
		}
	}
}
