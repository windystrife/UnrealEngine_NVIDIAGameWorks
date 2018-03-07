// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Runtime.CompilerServices;
using System.Diagnostics;
using UnrealBuildTool;

namespace AutomationTool
{
	#region LogUtils


	//@todo: This class should be moved somewhere else or the file should be renamed LogUtils.cs
	public class LogUtils
	{
		private static string LogFilename;

		/// <summary>
		/// Initializes trace logging.
		/// </summary>
		/// <param name="CommandLine">Command line.</param>
		public static void InitLogging(string[] CommandLine)
		{
            // ensure UTF8Output flag is respected, since we are initializing logging early in the program.
            if (CommandLine.Any(Arg => Arg.Equals("-utf8output", StringComparison.InvariantCultureIgnoreCase)))
            {
                Console.OutputEncoding = new System.Text.UTF8Encoding(false, false);
            }

			UnrealBuildTool.Log.InitLogging(
                bLogTimestamps: CommandUtils.ParseParam(CommandLine, "-Timestamps"),
				InLogLevel: (UnrealBuildTool.LogEventType)Enum.Parse(typeof(UnrealBuildTool.LogEventType), CommandUtils.ParseParamValue(CommandLine, "-Verbose=", "Log")),
                bLogSeverity: true,
                bLogSources: true,
				bLogSourcesToConsole: false,
                bColorConsoleOutput: true,
                TraceListeners: new TraceListener[]
                {
                    new UEConsoleTraceListener(),
                    // could return null, but InitLogging handles this gracefully.
                    CreateLogFileListener(out LogFilename),
                    //@todo - this is only used by GUBP nodes. Ideally we don't waste this 20MB if we are not running GUBP.
                    new AutomationMemoryLogListener(),
                });
	    }

		public static string FinalLogFileName
		{
			get
			{
				var LogFolder = Environment.GetEnvironmentVariable(EnvVarNames.LogFolder);
				if (!String.IsNullOrEmpty(LogFolder) && Directory.Exists(LogFolder) &&
						!String.IsNullOrEmpty(LogFilename) && File.Exists(LogFilename))
				{
					return CommandUtils.CombinePaths(LogFolder, "UAT_" + Path.GetFileName(LogFilename));
				}
				else
				{
					return LogFilename;
				}
			}
		}

        /// <summary>
        /// Shuts down logging. This is a nothrow function as shutdown errors are ignored.
        /// </summary>
        public static void ShutdownLogging()
        {
            // from here we can copy the log file to its final resting place
            try
            {
                // This closes all the output streams immediately, inside the Global Lock, so it's threadsafe.
                Trace.Close();

				// Try to copy the log file to the log folder. The reason why it's done here is that
				// at the time the log file is being initialized the env var may not yet be set (this 
				// applies to local log folder in particular)
				string DestFilename = FinalLogFileName;
				if(DestFilename != LogFilename)
				{
                    SafeCopyLogFile(LogFilename, DestFilename);
                }
            }
            catch (Exception)
            {
                // Silently ignore, logging is pointless because eveything is shut down at this point
            }
        }

        /// <summary>
        /// Copies log file to the final log folder, does multiple attempts if the destination file could not be created.
        /// </summary>
        /// <param name="SourceFilename"></param>
        /// <param name="DestFilename"></param>
        private static void SafeCopyLogFile(string SourceFilename, string DestFilename)
        {
            const int MaxAttempts = 10;
            int AttemptNo = 0;
            var DestLogFilename = DestFilename;
            bool Result = false;
            do
            {
                try
                {
                    File.Copy(SourceFilename, DestLogFilename, true);
                    Result = true;
                }
                catch (Exception)
                {
                    var ModifiedFilename = String.Format("{0}_{1}{2}", Path.GetFileNameWithoutExtension(DestFilename), AttemptNo, Path.GetExtension(DestLogFilename));
                    DestLogFilename = CommandUtils.CombinePaths(Path.GetDirectoryName(DestFilename), ModifiedFilename);
                    AttemptNo++;
                }
            }
            while (Result == false && AttemptNo <= MaxAttempts);
        }

        /// <summary>
        /// Creates the TraceListener used for file logging.
        /// We cannot simply use a TextWriterTraceListener because we need more flexibility when the file cannot be created.
        /// TextWriterTraceListener lazily creates the file, silently failing when it cannot.
        /// </summary>
        /// <returns>The newly created TraceListener, or null if it could not be created.</returns>
        private static TraceListener CreateLogFileListener(out string LogFilename)
        {
            const int MaxAttempts = 10;
            int Attempt = 0;
            var TempLogFolder = Path.GetTempPath();
            do
            {
                LogFilename = CommandUtils.CombinePaths(TempLogFolder, Attempt == 0 ? "Log.txt" : string.Format("Log_{0}.txt", Attempt));
                try
                {
                    // We do not need to set AutoFlush on the StreamWriter because we set Trace.AutoFlush, which calls it for us.
                    // Not only would this be redundant, StreamWriter AutoFlush does not flush the encoder, while a direct call to 
                    // StreamWriter.Flush() will, which is what the Trace system with AutoFlush = true will do.
                    // Internally, FileStream constructor opens the file with good arguments for writing to log files.
                    return new TextWriterTraceListener(new StreamWriter(LogFilename), "AutomationFileLogListener");
                }
                catch (Exception Ex)
                {
                    if (Attempt == (MaxAttempts - 1))
                    {
                        // Clear out the LogFilename to indicate we were not able to write one.
                        UnrealBuildTool.Log.TraceWarning("Unable to create log file {0}: {1}", LogFilename, Ex);
                        LogFilename = null;
                    }
                }
            } while (++Attempt < MaxAttempts);
            return null;
        }

        /// <summary>
		/// Dumps exception info to log.
		/// @todo: Remove this function as it doesn't do a good job printing the exception information.
		/// </summary>
		/// <param name="Ex">Exception</param>
		public static string FormatException(Exception Ex)
		{
			var Message = String.Format("Exception in {0}: {1}{2}Stacktrace: {3}", Ex.Source, Ex.Message, Environment.NewLine, Ex.StackTrace);
			if (Ex.InnerException != null)
			{
				Message += String.Format("InnerException in {0}: {1}{2}Stacktrace: {3}", Ex.InnerException.Source, Ex.InnerException.Message, Environment.NewLine, Ex.InnerException.StackTrace);
			}
			return Message;
		}

		/// <summary>
		/// Returns a unique logfile name.
		/// </summary>
		/// <param name="Base">Base name for the logfile</param>
		/// <returns>Unique logfile name.</returns>
		public static string GetUniqueLogName(string Base)
		{
			const int MaxAttempts = 1000;

			var Now     = DateTime.Now;
			int Attempt = 0;

			string LogFilename;
			for (;;)
			{
				var NowStr = Now.ToString("yyyy.MM.dd-HH.mm.ss");

				LogFilename = String.Format("{0}-{1}.txt", Base, NowStr);

				if (!File.Exists(LogFilename))
				{
					try
					{
						Directory.CreateDirectory(Path.GetDirectoryName(LogFilename));
						using (FileStream Stream = File.Open(LogFilename, FileMode.CreateNew, FileAccess.Write, FileShare.None))
						{
						}
						break;
					}
					catch (IOException)
					{
					}
				}

				++Attempt;
				if (Attempt == MaxAttempts)
				{
					throw new AutomationException(String.Format("Failed to create logfile {0}.", LogFilename));
				}

				Now = Now.AddSeconds(1);
			}

			return LogFilename;
		}

		public static string GetLogTail(string Filename = null, int NumLines = 250)
		{
			List<string> Lines;
			if (Filename == null)
			{
				Lines = new List<string>(AutomationMemoryLogListener.GetAccumulatedLog().Split(new string[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries));
			}
			else
			{
				Lines = new List<string>(CommandUtils.ReadAllLines(Filename));
			}

			if (Lines.Count > NumLines)
			{
				Lines.RemoveRange(0, Lines.Count - NumLines);
			}
			string Result = "";
			foreach (var Line in Lines)
			{
				Result += Line + Environment.NewLine;
			}
			return Result;
		}


	}

	#endregion

	#region AutomationMemoryLogListener

	/// <summary>
	/// Trace console listener.
	/// </summary>
	class AutomationMemoryLogListener : TraceListener
	{
		private static StringBuilder AccumulatedLog = new StringBuilder(1024 * 1024 * 20);
		private static object SyncObject = new object();

        public override bool IsThreadSafe { get { return true; } }

        /// <summary>
		/// Writes a formatted line to the console.
		/// </summary>
		private void WriteLinePrivate(string Message)
		{
			lock (SyncObject)
			{
				AccumulatedLog.AppendLine(Message);
			}
		}

		public static string GetAccumulatedLog()
		{
			lock (SyncObject)
			{
				return AccumulatedLog.ToString();
			}
		}

		#region TraceListener Interface

		public override void Write(string message)
		{
            WriteLinePrivate(message);
		}

		public override void WriteLine(string message)
		{
            WriteLinePrivate(message);
		}

        #endregion
	}

	#endregion

}
