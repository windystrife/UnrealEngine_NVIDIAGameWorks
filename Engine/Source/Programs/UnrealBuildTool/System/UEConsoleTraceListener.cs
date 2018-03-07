using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Replacement class for System.Diagnostics.ConsoleTracerListener, which doesn't exist in .NET Core
	/// </summary>
	public class UEConsoleTraceListener : TraceListener
	{
		/// <summary>
		/// Destination stream writer
		/// </summary>
		private TextWriter Writer; 

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="bUseStandardError">TRUE if we are writing to the standard error stream, FALSE for standard output</param>
		public UEConsoleTraceListener(bool bUseStandardError = false)
		{
			Writer = bUseStandardError ? System.Console.Out : System.Console.Error;
		}

		/// <summary>
		/// Writes a string of text to the output stream
		/// </summary>
		/// <param name="message">Message to write</param>
		public override void Write(string message)
		{
			Writer.Write(message);
		}


		/// <summary>
		/// Writes a line of text to the output stream
		/// </summary>
		/// <param name="message">MEssage to write</param>
		public override void WriteLine(string message)
		{
			Writer.WriteLine(message);
		}
	}
}
