// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace IncludeTool.Support
{
	/// <summary>
	/// Provides for writing log messages to a log file and the display, as well as setting status messages
	/// </summary>
	class LogWriter : TextWriter
	{
		/// <summary>
		/// Output stream for log text
		/// </summary>
		StreamWriter FileWriter;

		/// <summary>
		/// Construct a log writer backed by the given file
		/// </summary>
		/// <param name="FileName">Filename to write to</param>
		public LogWriter(string FileName)
		{
			FileWriter = new StreamWriter(FileName);
			FileWriter.AutoFlush = true;
		}

		/// <summary>
		/// Dispose of the current object, and its resources
		/// </summary>
		protected override void Dispose(bool Disposing)
		{
			if(FileWriter != null)
			{
				FileWriter.Dispose();
				FileWriter = null;
			}
		}

		/// <summary>
		/// Returns the encoding of this log writer
		/// </summary>
		public override Encoding Encoding
		{
			get { return Encoding.Unicode; }
		}

		/// <summary>
		/// Writes a character to the output stream
		/// </summary>
		/// <param name="Value">Character to write</param>
		public override void Write(char Value)
		{
			FileWriter.Write(Value);
			Console.Write(Value);
		}
	}
}
