using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Log Event Type
	/// </summary>
	public enum LogEventType
	{
		/// <summary>
		/// The log event is a fatal error
		/// </summary>
		Fatal = 1,

		/// <summary>
		/// The log event is an error
		/// </summary>
		Error = 2,

		/// <summary>
		/// The log event is a warning
		/// </summary>
		Warning = 4,

		/// <summary>
		/// Output the log event to the console
		/// </summary>
		Console = 8,

		/// <summary>
		/// Output the event to the on-disk log
		/// </summary>
		Log = 16,

		/// <summary>
		/// The log event should only be displayed if verbose logging is enabled
		/// </summary>
		Verbose = 32,

		/// <summary>
		/// The log event should only be displayed if very verbose logging is enabled
		/// </summary>
		VeryVerbose = 64
	}

	/// <summary>
	/// Options for formatting messages
	/// </summary>
	[Flags]
	public enum LogFormatOptions
	{
		/// <summary>
		/// Format normally
		/// </summary>
		None = 0,

		/// <summary>
		/// Never write a severity prefix. Useful for pre-formatted messages that need to be in a particular format for, eg. the Visual Studio output window
		/// </summary>
		NoSeverityPrefix = 1,
	}

	/// <summary>
	/// UAT/UBT Custom log system.
	/// 
	/// This lets you use any TraceListeners you want, but you should only call the static 
	/// methods below, not call Trace.XXX directly, as the static methods
	/// This allows the system to enforce the formatting and filtering conventions we desire.
	///
	/// For posterity, we cannot use the Trace or TraceSource class directly because of our special log requirements:
	///   1. We possibly capture the method name of the logging event. This cannot be done as a macro, so must be done at the top level so we know how many layers of the stack to peel off to get the real function.
	///   2. We have a verbose filter we would like to apply to all logs without having to have each listener filter individually, which would require our string formatting code to run every time.
	///   3. We possibly want to ensure severity prefixes are logged, but Trace.WriteXXX does not allow any severity info to be passed down.
	/// </summary>
	static public class Log
	{
		/// <summary>
		/// Guard our initialization. Mainly used by top level exception handlers to ensure its safe to call a logging function.
		/// In general user code should not concern itself checking for this.
		/// </summary>
		private static bool bIsInitialized = false;
		/// <summary>
		/// When true, verbose logging is enabled.
		/// </summary>
		private static LogEventType LogLevel = LogEventType.VeryVerbose;
		/// <summary>
		/// When true, warnings and errors will have a WARNING: or ERROR: prexifx, respectively.
		/// </summary>
		private static bool bLogSeverity = false;
		/// <summary>
		/// When true, logs will have the calling mehod prepended to the output as MethodName:
		/// </summary>
		private static bool bLogSources = false;
		/// <summary>
		/// When true, console output will have the calling mehod prepended to the output as MethodName:
		/// </summary>
		private static bool bLogSourcesToConsole = false;
		/// <summary>
		/// When true, will detect warnings and errors and set the console output color to yellow and red.
		/// </summary>
		private static bool bColorConsoleOutput = false;
		/// <summary>
		/// When configured, this tracks time since initialization to prepend a timestamp to each log.
		/// </summary>
		private static Stopwatch Timer;

		/// <summary>
		/// Expose the log level. This is a hack for ProcessResult.LogOutput, which wants to bypass our normal formatting scheme.
		/// </summary>
		public static bool bIsVerbose { get { return LogLevel >= LogEventType.Verbose; } }

		/// <summary>
		/// A collection of strings that have been already written once
		/// </summary>
		private static List<string> WriteOnceSet = new List<string>();

		/// <summary>
		/// Indent added to every output line
		/// </summary>
		public static string Indent
		{
			get;
			set;
		}

		/// <summary>
		/// Static initializer
		/// </summary>
		static Log()
		{
			Indent = "";
		}

		/// <summary>
		/// Allows code to check if the log system is ready yet.
		/// End users should NOT need to use this. It pretty much exists
		/// to work around startup issues since this is a global singleton.
		/// </summary>
		/// <returns></returns>
		public static bool IsInitialized()
		{
			return bIsInitialized;
		}

		/// <summary>
		/// Allows us to change verbosity after initializing. This can happen since we initialize logging early, 
		/// but then read the config and command line later, which could change this value.
		/// </summary>
		public static void SetLoggingLevel(LogEventType InLogLevel)
		{
			Log.LogLevel = InLogLevel;
		}

		/// <summary>
		/// This class allows InitLogging to be called more than once to work around chicken and eggs issues with logging and parsing command lines (see UBT startup code).
		/// </summary>
		/// <param name="bLogTimestamps">If true, the timestamp from Log init time will be prepended to all logs.</param>
		/// <param name="InLogLevel"></param>
		/// <param name="bLogSeverity">If true, warnings and errors will have a WARNING: and ERROR: prefix to them. </param>
		/// <param name="bLogSources">If true, logs will have the originating method name prepended to them.</param>
		/// <param name="bLogSourcesToConsole">If true, console output will have the originating method name appended to it.</param>
		/// <param name="bColorConsoleOutput"></param>
		/// <param name="TraceListeners">Collection of trace listeners to attach to the Trace.Listeners, in addition to the Default listener. The existing listeners (except the Default listener) are cleared first.</param>
		public static void InitLogging(bool bLogTimestamps, LogEventType InLogLevel, bool bLogSeverity, bool bLogSources, bool bLogSourcesToConsole, bool bColorConsoleOutput, IEnumerable<TraceListener> TraceListeners)
		{
			bIsInitialized = true;
			Timer = (bLogTimestamps && Timer == null) ? Stopwatch.StartNew() : null;
			Log.LogLevel = InLogLevel;
			Log.bLogSeverity = bLogSeverity;
			Log.bLogSources = bLogSources;
			Log.bLogSourcesToConsole = bLogSourcesToConsole;
			Log.bColorConsoleOutput = bColorConsoleOutput;

			// ensure that if InitLogging is called more than once we don't stack listeners.
			// but always leave the default listener around.
			for (int ListenerNdx = 0; ListenerNdx < Trace.Listeners.Count;)
			{
				if (Trace.Listeners[ListenerNdx].GetType() != typeof(DefaultTraceListener))
				{
					Trace.Listeners.RemoveAt(ListenerNdx);
				}
				else
				{
					++ListenerNdx;
				}
			}
			// don't add any null listeners
			Trace.Listeners.AddRange(TraceListeners.Where(l => l != null).ToArray());
			Trace.AutoFlush = true;
		}

		/// <summary>
		/// Gets the name of the Method N levels deep in the stack frame. Used to trap what method actually made the logging call.
		/// Only used when bLogSources is true.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <returns>ClassName.MethodName</returns>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static string GetSource(int StackFramesToSkip)
		{
			StackFrame Frame = new StackFrame(2 + StackFramesToSkip);
			System.Reflection.MethodBase Method = Frame.GetMethod();
			return String.Format("{0}.{1}", Method.DeclaringType.Name, Method.Name);
		}

		/// <summary>
		/// Converts a LogEventType into a log prefix. Only used when bLogSeverity is true.
		/// </summary>
		/// <param name="Severity"></param>
		/// <returns></returns>
		private static string GetSeverityPrefix(LogEventType Severity)
		{
			switch (Severity)
			{
				case LogEventType.Fatal:
					return "FATAL: ";
				case LogEventType.Error:
					return "ERROR: ";
				case LogEventType.Warning:
					return "WARNING: ";
				case LogEventType.Console:
					return "";
				case LogEventType.Verbose:
					return "VERBOSE: ";
				case LogEventType.VeryVerbose:
					return "VVERBOSE: ";
				default:
					return "";
			}
		}

		/// <summary>
		/// Converts a LogEventType into a message code
		/// </summary>
		/// <param name="Severity"></param>
		/// <returns></returns>
		private static int GetMessageCode(LogEventType Severity)
		{
			return (int)Severity;
		}

		/// <summary>
		/// Formats message for logging. Enforces the configured options.
		/// </summary>
		/// <param name="StackFramesToSkip">Number of frames to skip to get to the originator of the log request.</param>
		/// <param name="Verbosity">Message verbosity level</param>
		/// <param name="Options">Options for formatting this string</param>
		/// <param name="bForConsole">Whether the message is intended for console output</param>
		/// <param name="Format">Message text format string</param>
		/// <param name="Args">Message text parameters</param>
		/// <returns>Formatted message</returns>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static List<string> FormatMessage(int StackFramesToSkip, LogEventType Verbosity, LogFormatOptions Options, bool bForConsole, string Format, params object[] Args)
		{
			string TimePrefix = (Timer != null) ? String.Format("[{0:hh\\:mm\\:ss\\.fff}] ", Timer.Elapsed) : "";
			string SourcePrefix = (bForConsole ? bLogSourcesToConsole : bLogSources) ? string.Format("{0}: ", GetSource(StackFramesToSkip)) : "";
			string SeverityPrefix = (bLogSeverity && ((Options & LogFormatOptions.NoSeverityPrefix) == 0)) ? GetSeverityPrefix(Verbosity) : "";

			// If there are no extra args, don't try to format the string, in case it has any format control characters in it (our LOCTEXT strings tend to).
			string[] Lines = ((Args.Length > 0) ? String.Format(Format, Args) : Format).TrimEnd(' ', '\t', '\r', '\n').Split('\n');

			List<string> FormattedLines = new List<string>();
			FormattedLines.Add(String.Format("{0}{1}{2}{3}{4}", TimePrefix, SourcePrefix, Indent, SeverityPrefix, Lines[0].TrimEnd('\r')));

			if (Lines.Length > 1)
			{
				string Padding = new string(' ', SeverityPrefix.Length);
				for (int Idx = 1; Idx < Lines.Length; Idx++)
				{
					FormattedLines.Add(String.Format("{0}{1}{2}{3}{4}", TimePrefix, SourcePrefix, Indent, Padding, Lines[Idx].TrimEnd('\r')));
				}
			}

			return FormattedLines;
		}

		/// <summary>
		/// Writes a formatted message to the console. All other functions should boil down to calling this method.
		/// </summary>
		/// <param name="StackFramesToSkip">Number of frames to skip to get to the originator of the log request.</param>
		/// <param name="bWriteOnce">If true, this message will be written only once</param>
		/// <param name="Verbosity">Message verbosity level. We only meaningfully use values up to Verbose</param>
		/// <param name="FormatOptions">Options for formatting messages</param>
		/// <param name="Format">Message format string.</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static void WriteLinePrivate(int StackFramesToSkip, bool bWriteOnce, LogEventType Verbosity, LogFormatOptions FormatOptions, string Format, params object[] Args)
		{
			if (!bIsInitialized)
			{
				throw new BuildException("Tried to using Logging system before it was ready");
			}

			// if we want this message only written one time, check if it was already written out
			if (bWriteOnce)
			{
				string Formatted = string.Format(Format, Args);
				if (WriteOnceSet.Contains(Formatted))
				{
					return;
				}

				WriteOnceSet.Add(Formatted);
			}

			if (Verbosity <= LogLevel)
			{
				// Do console color highlighting here.
				ConsoleColor DefaultColor = ConsoleColor.Gray;
				bool bIsWarning = false;
				bool bIsError = false;
				// don't try to touch the console unless we are told to color the output.
				if (bColorConsoleOutput)
				{
					DefaultColor = Console.ForegroundColor;
					bIsWarning = Verbosity == LogEventType.Warning;
					bIsError = Verbosity <= LogEventType.Error;
					// @todo mono - mono doesn't seem to initialize the ForegroundColor properly, so we can't restore it properly.
					// Avoid touching the console color unless we really need to.
					if (bIsWarning || bIsError)
					{
						Console.ForegroundColor = bIsWarning ? ConsoleColor.Yellow : ConsoleColor.Red;
					}
				}
				try
				{
					// @todo mono: mono has some kind of bug where calling mono recursively by spawning
					// a new process causes Trace.WriteLine to stop functioning (it returns, but does nothing for some reason).
					// work around this by simulating Trace.WriteLine on mono.
					// We use UAT to spawn UBT instances recursively a lot, so this bug can effectively
					// make all build output disappear outside of the top level UAT process.
					//					#if MONO
					lock (((System.Collections.ICollection)Trace.Listeners).SyncRoot)
					{
						foreach (TraceListener l in Trace.Listeners)
						{
							bool bIsConsole = l is UEConsoleTraceListener;
							if (Verbosity != LogEventType.Log || !bIsConsole || LogLevel >= LogEventType.Verbose)
							{
								List<string> Lines = FormatMessage(StackFramesToSkip + 1, Verbosity, FormatOptions, bIsConsole, Format, Args);
								foreach (string Line in Lines)
								{
									l.WriteLine(Line);
								}
							}
							l.Flush();
						}
					}
					//					#else
					// Call Trace directly here. Trace ensures that our logging is threadsafe using the GlobalLock.
					//                    	Trace.WriteLine(FormatMessage(StackFramesToSkip + 1, CustomSource, Verbosity, Format, Args));
					//					#endif
				}
				finally
				{
					// make sure we always put the console color back.
					if (bColorConsoleOutput && (bIsWarning || bIsError))
					{
						Console.ForegroundColor = DefaultColor;
					}
				}
			}
		}

		/// <summary>
		/// Similar to Trace.WriteLineIf
		/// </summary>
		/// <param name="Condition"></param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineIf(bool Condition, LogEventType Verbosity, string Format, params object[] Args)
		{
			if (Condition)
			{
				WriteLinePrivate(1, false, Verbosity, LogFormatOptions.None, Format, Args);
			}
		}

		/// <summary>
		/// Mostly an internal function, but expose StackFramesToSkip to allow UAT to use existing wrapper functions and still get proper formatting.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(int StackFramesToSkip, LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(StackFramesToSkip + 1, false, Verbosity, LogFormatOptions.None, Format, Args);
		}
		/// <summary>
		/// Mostly an internal function, but expose StackFramesToSkip to allow UAT to use existing wrapper functions and still get proper formatting.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <param name="Verbosity"></param>
		/// <param name="FormatOptions"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(int StackFramesToSkip, LogEventType Verbosity, LogFormatOptions FormatOptions, string Format, params object[] Args)
		{
			WriteLinePrivate(StackFramesToSkip + 1, false, Verbosity, FormatOptions, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLine
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, Verbosity, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLine
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="FormatOptions"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(LogEventType Verbosity, LogFormatOptions FormatOptions, string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, Verbosity, FormatOptions, Format, Args);
		}

		/// <summary>
		/// Writes an error message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceError(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Error, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVerbose(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Verbose, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceInformation(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Console, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarning(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Warning, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a very verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVeryVerbose(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.VeryVerbose, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a message to the log only.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceLog(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Log, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLin
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineOnce(LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, Verbosity, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes an error message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceErrorOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Error, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVerboseOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Verbose, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceInformationOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Console, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarningOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Warning, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a very verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVeryVerboseOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.VeryVerbose, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a message to the log only.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceLogOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Log, LogFormatOptions.None, Format, Args);
		}
	}

	/// <summary>
	/// Class to apply a log indent for the lifetime of an object 
	/// </summary>
	public class ScopedLogIndent : IDisposable
	{
		/// <summary>
		/// The previous indent
		/// </summary>
		string PrevIndent;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Indent">Indent to append to the existing indent</param>
		public ScopedLogIndent(string Indent)
		{
			PrevIndent = Log.Indent;
			Log.Indent += Indent;
		}

		/// <summary>
		/// Restore the log indent to normal
		/// </summary>
		public void Dispose()
		{
			if (PrevIndent != null)
			{
				Log.Indent = PrevIndent;
				PrevIndent = null;
			}
		}
	}
}
