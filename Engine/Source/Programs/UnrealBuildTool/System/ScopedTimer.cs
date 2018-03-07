using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Scoped timer, start is in the constructor, end in Dispose. Best used with using(var Timer = new ScopedTimer()). Suports nesting.
	/// </summary>
	public class ScopedTimer : IDisposable
	{
		DateTime StartTime;
		string TimerName;
		LogEventType Verbosity;
		static int Indent = 0;
		static object IndentLock = new object();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Name">Name of the block being measured</param>
		/// <param name="InVerbosity">Verbosity for output messages</param>
		public ScopedTimer(string Name, LogEventType InVerbosity = LogEventType.Verbose)
		{
			TimerName = Name;
			lock (IndentLock)
			{
				Indent++;
			}
			Verbosity = InVerbosity;
			StartTime = DateTime.UtcNow;
		}

		/// <summary>
		/// Prints out the timing message
		/// </summary>
		public void Dispose()
		{
			double TotalSeconds = (DateTime.UtcNow - StartTime).TotalSeconds;
			int LogIndent = 0;
			lock (IndentLock)
			{
				LogIndent = --Indent;
			}
			StringBuilder IndentText = new StringBuilder(LogIndent * 2);
			IndentText.Append(' ', LogIndent * 2);

			Log.WriteLine(Verbosity, "{0}{1} took {2}s", IndentText.ToString(), TimerName, TotalSeconds);
		}
	}
}
