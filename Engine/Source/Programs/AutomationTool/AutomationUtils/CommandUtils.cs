// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Threading;
using UnrealBuildTool;
using System.Runtime.CompilerServices;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using System.Xml;
using System.Xml.Serialization;

namespace AutomationTool
{
	#region ParamList

	/// <summary>
	/// Wrapper around List with support for multi parameter constructor, i.e:
	///   var Maps = new ParamList<string>("Map1", "Map2");
	/// </summary>
	/// <typeparam name="T"></typeparam>
	public class ParamList<T> : List<T>
	{
		public ParamList(params T[] Args)
		{
			AddRange(Args);
		}

		public ParamList(ICollection<T> Collection)
			: base(Collection != null ? Collection : new T[] {})
		{
		}

		public override string ToString()
		{
			var Text = "";
			for (int Index = 0; Index < Count; ++Index)
			{
				if (Index > 0)
				{
					Text += ", ";
				}
				Text += this[Index].ToString();				
			}
			return Text;
		}
	}

	#endregion

	#region PathSeparator

	public enum PathSeparator
	{
		Default = 0,
		Slash,
		Backslash,
		Depot,
		Local
	}

	#endregion

	/// <summary>
	/// Base utility function for script commands.
	/// </summary>
	public partial class CommandUtils
	{
		#region Environment Setup

		static private CommandEnvironment CmdEnvironment;

		/// <summary>
		/// BuildEnvironment to use for this buildcommand. This is initialized by InitBuildEnvironment. As soon
		/// as the script execution in ExecuteBuild begins, the BuildEnv is set up and ready to use.
		/// </summary>
		static public CommandEnvironment CmdEnv
		{
			get
			{
				if (CmdEnvironment == null)
				{
					throw new AutomationException("Attempt to use CommandEnvironment before it was initialized.");
				}
				return CmdEnvironment;
			}
		}

		/// <summary>
		/// Initializes build environment. If the build command needs a specific env-var mapping or
		/// has an extended BuildEnvironment, it must implement this method accordingly.
		/// </summary>
		/// <returns>Initialized and ready to use BuildEnvironment</returns>
		static internal void InitCommandEnvironment()
		{
			CmdEnvironment = new CommandEnvironment();
		}

		#endregion

		#region Logging

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Console).
		/// </summary>
		/// <param name="Format">Format string</param>
		/// <param name="Args">Parameters</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void Log(string Format, params object[] Args)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Console, Format, Args);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Console).
		/// </summary>
		/// <param name="Message">Text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void Log(string Message)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Console, Message);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Error).
		/// </summary>
		/// <param name="Format">Format string</param>
		/// <param name="Args">Parameters</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogError(string Format, params object[] Args)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Error, Format, Args);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Error).
		/// </summary>
		/// <param name="Message">Text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogError(string Message)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Error, Message);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Warning).
		/// </summary>
		/// <param name="Format">Format string</param>
		/// <param name="Args">Parameters</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogWarning(string Format, params object[] Args)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Warning, Format, Args);
		}

		/// <summary>
		/// Writes a message to log (with LogEventType.Warning).
		/// </summary>
		/// <param name="Message">Text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogWarning(string Message)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Warning, Message);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Verbose).
		/// </summary>
		/// <param name="Foramt">Format string</param>
		/// <param name="Args">Arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogVerbose(string Format, params object[] Args)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Verbose, Format, Args);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Verbose).
		/// </summary>
		/// <param name="Message">Text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogVerbose(string Message)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Verbose, Message);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.VeryVerbose).
		/// </summary>
		/// <param name="Foramt">Format string</param>
		/// <param name="Args">Arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogVeryVerbose(string Format, params object[] Args)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.VeryVerbose, Format, Args);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.VeryVerbose).
		/// </summary>
		/// <param name="Message">Text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogVeryVerbose(string Message)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.VeryVerbose, Message);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Log).
		/// </summary>
		/// <param name="Foramt">Format string</param>
		/// <param name="Args">Arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogLog(string Format, params object[] Args)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Log, Format, Args);
		}

		/// <summary>
		/// Writes formatted text to log (with LogEventType.Log).
		/// </summary>
		/// <param name="Message">Text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogLog(string Message)
		{
			UnrealBuildTool.Log.WriteLine(1, UnrealBuildTool.LogEventType.Log, Message);
		}

		/// <summary>
		/// Writes formatted text to log.
		/// </summary>
		/// <param name="Verbosity">Verbosity</param>
		/// <param name="Format">Format string</param>
		/// <param name="Args">Arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogWithVerbosity(UnrealBuildTool.LogEventType Verbosity, string Format, params object[] Args)
		{
            UnrealBuildTool.Log.WriteLine(1, Verbosity, Format, Args);
		}

		/// <summary>
		/// Writes formatted text to log.
		/// </summary>
		/// <param name="Verbosity">Verbosity</param>
		/// <param name="Message">Text</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogWithVerbosity(UnrealBuildTool.LogEventType Verbosity, string Message)
		{
            UnrealBuildTool.Log.WriteLine(1, Verbosity, Message);
		}

		/// <summary>
		/// Dumps exception to log.
		/// </summary>
		/// <param name="Verbosity">Verbosity</param>
		/// <param name="Ex">Exception</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void LogWithVerbosity(UnrealBuildTool.LogEventType Verbosity, Exception Ex)
		{
            UnrealBuildTool.Log.WriteLine(1, Verbosity, LogUtils.FormatException(Ex));
		}

		#endregion

		#region Progress Logging

		public static void LogPushProgress(bool bShowProgress, int Numerator, int Denominator)
		{
			if(bShowProgress)
			{
				Log("[@progress push {0}/{1} skipline]", Numerator, Denominator);
			}
		}

		public static void LogPopProgress(bool bShowProgress)
		{
			if(bShowProgress)
			{
				Log("[@progress pop skipline]");
			}
		}

		public static void LogIncrementProgress(bool bShowProgress, int Numerator, int Denominator)
		{
			if(bShowProgress)
			{
				Log("[@progress increment {0}/{1} skipline]", Numerator, Denominator);
			}
		}

		public static void LogSetProgress(bool bShowProgress, string Format, params string[] Args)
		{
			if(bShowProgress)
			{
				Log("[@progress '{0}' skipline]", String.Format(Format, Args));
			}
		}

		public static void LogSetProgress(bool bShowProgress, int Numerator, int Denominator, string Format, params string[] Args)
		{
			if(bShowProgress)
			{
				Log("[@progress {0}/{1} '{2}' skipline]", Numerator, Denominator, String.Format(Format, Args));
			}
		}

		#endregion

		#region IO

		/// <summary>
		/// Finds files in specified paths. 
		/// </summary>
		/// <param name="SearchPattern">Pattern</param>
		/// <param name="Recursive">Recursive search</param>
		/// <param name="Paths">Paths to search</param>
		/// <returns>An array of files found in the specified paths</returns>
		public static string[] FindFiles(string SearchPattern, bool Recursive, string PathToSearch)
		{
			List<string> FoundFiles = new List<string>();

			var NormalizedPath = ConvertSeparators(PathSeparator.Default, PathToSearch);
			if (DirectoryExists(NormalizedPath))
			{
				var FoundInPath = InternalUtils.SafeFindFiles(NormalizedPath, SearchPattern, Recursive);
				if (FoundInPath == null)
				{
					throw new AutomationException(String.Format("Failed to find files in '{0}'", NormalizedPath));
				}
				FoundFiles.AddRange(FoundInPath);
			}

			return FoundFiles.ToArray();
		}

		/// <summary>
		/// Finds files in specified paths. 
		/// </summary>
		/// <param name="SearchPattern">Pattern</param>
		/// <param name="Recursive">Recursive search</param>
		/// <param name="Paths">Paths to search</param>
		/// <returns>An array of files found in the specified paths</returns>
		public static FileReference[] FindFiles(string SearchPattern, bool Recursive, DirectoryReference PathToSearch)
		{
			return FindFiles(SearchPattern, Recursive, PathToSearch.FullName).Select(x => new FileReference(x)).ToArray();
		}

		/// <summary>
		/// Finds files in specified paths. 
		/// </summary>
		/// <param name="SearchPattern">Pattern</param>
		/// <param name="Recursive">Recursive search</param>
		/// <param name="Paths">Paths to search</param>
		/// <returns>An array of files found in the specified paths</returns>
		public static string[] FindFiles_NoExceptions(string SearchPattern, bool Recursive, string PathToSearch)
		{
			List<string> FoundFiles = new List<string>();

			var NormalizedPath = ConvertSeparators(PathSeparator.Default, PathToSearch);
			if (DirectoryExists(NormalizedPath))
			{
				var FoundInPath = InternalUtils.SafeFindFiles(NormalizedPath, SearchPattern, Recursive);
				if (FoundInPath != null)
				{
					FoundFiles.AddRange(FoundInPath);
				}
			}

			return FoundFiles.ToArray();
		}
        /// <summary>
        /// Finds files in specified paths. 
        /// </summary>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <param name="SearchPattern">Pattern</param>
        /// <param name="Recursive">Recursive search</param>
        /// <param name="Paths">Paths to search</param>
        /// <returns>An array of files found in the specified paths</returns>
        public static string[] FindFiles_NoExceptions(bool bQuiet, string SearchPattern, bool Recursive, string PathToSearch)
        {
            List<string> FoundFiles = new List<string>();

			var NormalizedPath = ConvertSeparators(PathSeparator.Default, PathToSearch);
            if (DirectoryExists(NormalizedPath))
            {
                var FoundInPath = InternalUtils.SafeFindFiles(NormalizedPath, SearchPattern, Recursive, bQuiet);
                if (FoundInPath != null)
                {
                    FoundFiles.AddRange(FoundInPath);
                }
            }

			return FoundFiles.ToArray();
        }
		/// <summary>
		/// Finds files in specified paths. 
		/// </summary>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <param name="SearchPattern">Pattern</param>
		/// <param name="Recursive">Recursive search</param>
		/// <param name="Paths">Paths to search</param>
		/// <returns>An array of files found in the specified paths</returns>
		public static string[] FindDirectories(bool bQuiet, string SearchPattern, bool Recursive, string PathToSearch)
		{
			List<string> FoundDirs = new List<string>();

			var NormalizedPath = ConvertSeparators(PathSeparator.Default, PathToSearch);
			if (DirectoryExists(NormalizedPath))
			{
				var FoundInPath = InternalUtils.SafeFindDirectories(NormalizedPath, SearchPattern, Recursive, bQuiet);
				if (FoundInPath == null)
				{
					throw new AutomationException(String.Format("Failed to find directories in '{0}'", NormalizedPath));
				}
				FoundDirs.AddRange(FoundInPath);
			}

			return FoundDirs.ToArray();
		}
		/// <summary>
		/// Finds Directories in specified paths. 
		/// </summary>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <param name="SearchPattern">Pattern</param>
		/// <param name="Recursive">Recursive search</param>
		/// <param name="Paths">Paths to search</param>
		/// <returns>An array of files found in the specified paths</returns>
		public static string[] FindDirectories_NoExceptions(bool bQuiet, string SearchPattern, bool Recursive, string PathToSearch)
		{
			List<string> FoundDirs = new List<string>();

			var NormalizedPath = ConvertSeparators(PathSeparator.Default, PathToSearch);
			if (DirectoryExists(NormalizedPath))
			{
				var FoundInPath = InternalUtils.SafeFindDirectories(NormalizedPath, SearchPattern, Recursive, bQuiet);
				if (FoundInPath != null)
				{
					FoundDirs.AddRange(FoundInPath);
				}
			}

			return FoundDirs.ToArray();
		}
		/// <summary>
		/// Deletes a file(s). 
		/// If the file does not exist, silently succeeds.
		/// If the deletion of the file fails, this function throws an Exception.
		/// </summary>
		/// <param name="Filenames">Filename</param>
		public static void DeleteFile(string FileName)
		{
			var NormalizedFilename = ConvertSeparators(PathSeparator.Default, FileName);
			if (!InternalUtils.SafeDeleteFile(NormalizedFilename))
			{
				throw new AutomationException(String.Format("Failed to delete file '{0}'", NormalizedFilename));
			}
		}
        /// <summary>
        /// Deletes a file(s). 
        /// If the file does not exist, silently succeeds.
        /// If the deletion of the file fails, this function throws an Exception.
        /// </summary>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <param name="Filenames">Filename</param>
        public static void DeleteFile(bool bQuiet, string FileName)
        {
            var NormalizedFilename = ConvertSeparators(PathSeparator.Default, FileName);
            if (!InternalUtils.SafeDeleteFile(NormalizedFilename, bQuiet))
            {
                throw new AutomationException(String.Format("Failed to delete file '{0}'", NormalizedFilename));
            }
        }

		/// <summary>
		/// Deletes a file(s). 
		/// If the deletion of the file fails, prints a warning.
		/// </summary>
		/// <param name="Filenames">Filename</param>
        public static bool DeleteFile_NoExceptions(string FileName)
		{
			bool Result = true;

			var NormalizedFilename = ConvertSeparators(PathSeparator.Default, FileName);
			if (!InternalUtils.SafeDeleteFile(NormalizedFilename))
			{
				LogWarning("Failed to delete file '{0}'", NormalizedFilename);
				Result = false;
			}
			return Result;
		}
		/// <summary>
		/// Deletes a file(s). 
		/// If the deletion of the file fails, prints a warning.
		/// </summary>
		/// <param name="Filename">Filename</param>
        /// <param name="bQuiet">if true, then don't retry and don't print much.</param>
        public static bool DeleteFile_NoExceptions(string Filename, bool bQuiet = false)
		{
			bool Result = true;
			var NormalizedFilename = ConvertSeparators(PathSeparator.Default, Filename);
			if (!InternalUtils.SafeDeleteFile(NormalizedFilename, bQuiet))
			{
				LogWithVerbosity(bQuiet ? LogEventType.Log : LogEventType.Warning, "Failed to delete file '{0}'", NormalizedFilename);
				Result = false;
			}
			return Result;
		}

		/// <summary>
		/// Deletes a directory(or directories) including its contents (recursively, will delete read-only files).
		/// If the deletion of the directory fails, this function throws an Exception.
		/// </summary>
		/// <param name="bQuiet">Suppresses log output if true</param>
		/// <param name="Directories">Directories</param>
		public static void DeleteDirectory(bool bQuiet, string Directory)
		{
			var NormalizedDirectory = ConvertSeparators(PathSeparator.Default, Directory);
			if (!InternalUtils.SafeDeleteDirectory(NormalizedDirectory, bQuiet))
			{
				throw new AutomationException(String.Format("Failed to delete directory '{0}'", NormalizedDirectory));
			}
		}

		/// <summary>
		/// Deletes a directory(or directories) including its contents (recursively, will delete read-only files).
		/// If the deletion of the directory fails, this function throws an Exception.
		/// </summary>
        /// <param name="Directories">Directories</param>
        public static void DeleteDirectory(string Directory)
		{
			DeleteDirectory(false, Directory);
		}

		/// <summary>
		/// Deletes a directory(or directories) including its contents (recursively, will delete read-only files).
		/// If the deletion of the directory fails, prints a warning.
		/// </summary>
		/// <param name="bQuiet">Suppresses log output if true</param>
        /// <param name="Directories">Directories</param>
        public static bool DeleteDirectory_NoExceptions(bool bQuiet, string Directory)
		{
			bool Result = true;

			var NormalizedDirectory = ConvertSeparators(PathSeparator.Default, Directory);
            try
            {
                if (!InternalUtils.SafeDeleteDirectory(NormalizedDirectory, bQuiet))
                {
                    LogWarning("Failed to delete directory '{0}'", NormalizedDirectory);
                    Result = false;
                }
            }
            catch (Exception Ex)
            {
				if (!bQuiet)
				{
					LogWarning("Failed to delete directory, exception '{0}'", NormalizedDirectory);
					LogWarning(Ex.Message);
				}
                Result = false;
            }

			return Result;
		}

		/// <summary>
		/// Deletes a directory(or directories) including its contents (recursively, will delete read-only files).
		/// If the deletion of the directory fails, prints a warning.
		/// </summary>
        /// <param name="Directories">Directories</param>
        public static bool DeleteDirectory_NoExceptions(string DirectoryName)
		{
			return DeleteDirectory_NoExceptions(false, DirectoryName);
		}


		/// <summary>
		/// Attempts to delete a directory, if that fails deletes all files and folder from the specified directory.
		/// This works around the issue when the user has a file open in a notepad from that directory. Somehow deleting the file works but
		/// deleting the directory with the file that's open, doesn't.
		/// </summary>
		/// <param name="DirectoryName"></param>
		public static void DeleteDirectoryContents(string DirectoryName)
		{
			LogVerbose("DeleteDirectoryContents({0})", DirectoryName);
			const bool bQuiet = true;
			var Files = CommandUtils.FindFiles_NoExceptions(bQuiet, "*", false, DirectoryName);
			foreach (var Filename in Files)
			{
				CommandUtils.DeleteFile_NoExceptions(Filename);
			}
			var Directories = CommandUtils.FindDirectories_NoExceptions(bQuiet, "*", false, DirectoryName);
			foreach (var SubDirectoryName in Directories)
			{
				CommandUtils.DeleteDirectory_NoExceptions(bQuiet, SubDirectoryName);
			}
		}

		/// <summary>
		/// Checks if a directory(or directories) exists.
		/// </summary>
        /// <param name="Directories">Directories</param>
        /// <returns>True if the directory exists, false otherwise.</returns>
		public static bool DirectoryExists(string DirectoryName)
		{
			var NormalizedDirectory = ConvertSeparators(PathSeparator.Default, DirectoryName);
			return System.IO.Directory.Exists(NormalizedDirectory);
		}

		/// <summary>
		/// Checks if a directory(or directories) exists.
		/// </summary>
        /// <param name="Directories">Directories</param>
        /// <returns>True if the directory exists, false otherwise.</returns>
		public static bool DirectoryExists_NoExceptions(string DirectoryName)
		{
			var NormalizedDirectory = ConvertSeparators(PathSeparator.Default, DirectoryName);
			try
			{
				return System.IO.Directory.Exists(NormalizedDirectory);
			}
			catch (Exception Ex)
			{
				LogWarning("Unable to check if directory exists: {0}", NormalizedDirectory);
				LogWarning(Ex.Message);
				return false;
			}
		}

		/// <summary>
		/// Creates a directory(or directories).
		/// If the creation of the directory fails, this function throws an Exception.
		/// </summary>
        /// <param name="Directories">Directories</param>
        public static void CreateDirectory(string DirectoryName)
		{
			var NormalizedDirectory = ConvertSeparators(PathSeparator.Default, DirectoryName);
			if (!InternalUtils.SafeCreateDirectory(NormalizedDirectory))
			{
				throw new AutomationException(String.Format("Failed to create directory '{0}'", NormalizedDirectory));
			}
		}

        /// <summary>
        /// Creates a directory(or directories).
        /// If the creation of the directory fails, this function throws an Exception.
        /// </summary>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <param name="Directories">Directories</param>
        public static void CreateDirectory(bool bQuiet, string DirectoryName)
        {
            var NormalizedDirectory = ConvertSeparators(PathSeparator.Default, DirectoryName);
            if (!InternalUtils.SafeCreateDirectory(NormalizedDirectory, bQuiet))
            {
                throw new AutomationException(String.Format("Failed to create directory '{0}'", NormalizedDirectory));
            }
        }

		/// <summary>
		/// Creates a directory (or directories).
		/// If the creation of the directory fails, this function prints a warning.
		/// </summary>
        /// <param name="Directories">Directories</param>
        public static bool CreateDirectory_NoExceptions(string DirectoryName)
		{
			bool Result = true;
			var NormalizedDirectory = ConvertSeparators(PathSeparator.Default, DirectoryName);
			if (!InternalUtils.SafeCreateDirectory(NormalizedDirectory))
			{
				LogWarning("Failed to create directory '{0}'", NormalizedDirectory);
				Result = false;
			}
			return Result;
		}

		/// <summary>
		/// Renames/moves a file.
		/// If the rename of the file fails, this function throws an Exception.
		/// </summary>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <param name="OldName">Old name</param>
		/// <param name="NewName">new name</param>
        public static void RenameFile(string OldName, string NewName, bool bQuiet = false)
		{
			var OldNormalized = ConvertSeparators(PathSeparator.Default, OldName);
			var NewNormalized = ConvertSeparators(PathSeparator.Default, NewName);
			if (!InternalUtils.SafeRenameFile(OldNormalized, NewNormalized, bQuiet))
			{
				throw new AutomationException(String.Format("Failed to rename/move file '{0}' to '{1}'", OldNormalized, NewNormalized));
			}
		}

		/// <summary>
		/// Renames/moves a file.
		/// If the rename of the file fails, this function prints a warning.
		/// </summary>
		/// <param name="OldName">Old name</param>
		/// <param name="NewName">new name</param>
		public static bool RenameFile_NoExceptions(string OldName, string NewName)
		{
			var OldNormalized = ConvertSeparators(PathSeparator.Default, OldName);
			var NewNormalized = ConvertSeparators(PathSeparator.Default, NewName);
			var Result = InternalUtils.SafeRenameFile(OldNormalized, NewNormalized);
			if (!Result)
			{
				LogWarning("Failed to rename/move file '{0}' to '{1}'", OldName, NewName);
			}
			return Result;
		}

		/// <summary>
		/// Checks if a file(s) exists.
		/// </summary>
		/// <param name="Filenames">Filename.</param>
		/// <returns>True if the file exists, false otherwise.</returns>
		public static bool FileExists(string FileName)
		{
			var NormalizedFilename = ConvertSeparators(PathSeparator.Default, FileName);
			return InternalUtils.SafeFileExists(NormalizedFilename);
		}

		/// <summary>
		/// Checks if a file(s) exists.
		/// </summary>
		/// <param name="Filenames">Filename.</param>
		/// <returns>True if the file exists, false otherwise.</returns>
		public static bool FileExists_NoExceptions(string FileName)
		{
			// Standard version doesn't throw, but keep this function for consistency.
			return FileExists(FileName);
		}

        /// <summary>
        /// Checks if a file(s) exists.
        /// </summary>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <param name="Filenames">Filename.</param>
        /// <returns>True if the file exists, false otherwise.</returns>
        public static bool FileExists(bool bQuiet, string FileName)
        {
			var NormalizedFilename = ConvertSeparators(PathSeparator.Default, FileName);
			return InternalUtils.SafeFileExists(NormalizedFilename, bQuiet);
        }

        /// <summary>
        /// Checks if a file(s) exists.
        /// </summary>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <param name="Filenames">Filename.</param>
        /// <returns>True if the file exists, false otherwise.</returns>
        public static bool FileExists_NoExceptions(bool bQuiet, string FileName)
        {
            // Standard version doesn't throw, but keep this function for consistency.
            return FileExists(bQuiet, FileName);
        }

		static Stack<string> WorkingDirectoryStack = new Stack<string>();

		/// <summary>
		/// Pushes the current working directory onto a stack and sets CWD to a new value.
		/// </summary>
		/// <param name="WorkingDirectory">New working direcotry.</param>
		public static void PushDir(string WorkingDirectory)
		{
			string OrigCurrentDirectory = Environment.CurrentDirectory;
			WorkingDirectory = ConvertSeparators(PathSeparator.Default, WorkingDirectory);
			try
			{
				Environment.CurrentDirectory = WorkingDirectory;
			}
			catch (Exception Ex)
			{
				throw new AutomationException(Ex, "Unable to change current directory to {0}", WorkingDirectory);
			}

			WorkingDirectoryStack.Push(OrigCurrentDirectory);
		}

		/// <summary>
		/// Pushes the current working directory onto a stack and sets CWD to a new value.
		/// </summary>
		/// <param name="WorkingDirectory">New working direcotry.</param>
		public static bool PushDir_NoExceptions(string WorkingDirectory)
		{
			bool Result = true;
			string OrigCurrentDirectory = Environment.CurrentDirectory;
			WorkingDirectory = ConvertSeparators(PathSeparator.Default, WorkingDirectory);
			try
			{
				Environment.CurrentDirectory = WorkingDirectory;
				WorkingDirectoryStack.Push(OrigCurrentDirectory);
			}
			catch
			{
				LogWarning("Unable to change current directory to {0}", WorkingDirectory);
				Result = false;
			}
			return Result;
		}

		/// <summary>
		/// Pops the last working directory from a stack and sets it as the current working directory.
		/// </summary>
		public static void PopDir()
		{
			if (WorkingDirectoryStack.Count > 0)
			{
				Environment.CurrentDirectory = WorkingDirectoryStack.Pop();
			}
			else
			{
				throw new AutomationException("Unable to PopDir. WorkingDirectoryStack is empty.");
			}
		}

		/// <summary>
		/// Pops the last working directory from a stack and sets it as the current working directory.
		/// </summary>
		public static bool PopDir_NoExceptions()
		{
			bool Result = true;
			if (WorkingDirectoryStack.Count > 0)
			{
				Environment.CurrentDirectory = WorkingDirectoryStack.Pop();
			}
			else
			{
				LogWarning("Unable to PopDir. WorkingDirectoryStack is empty.");
				Result = false;
			}
			return Result;
		}

		/// <summary>
		/// Clears the directory stack
		/// </summary>
		public static void ClearDirStack()
		{
			while (WorkingDirectoryStack.Count > 0)
			{
				PopDir();
			}
		}

		/// <summary>
		/// Changes the current working directory.
		/// </summary>
		/// <param name="WorkingDirectory">New working directory.</param>
		public static void ChDir(string WorkingDirectory)
		{
			WorkingDirectory = ConvertSeparators(PathSeparator.Default, WorkingDirectory);
			try
			{
				Environment.CurrentDirectory = WorkingDirectory;
			}
			catch (Exception Ex)
			{
				throw new ArgumentException(String.Format("Unable to change current directory to {0}", WorkingDirectory), Ex);
			}
		}

		/// <summary>
		/// Changes the current working directory.
		/// </summary>
		/// <param name="WorkingDirectory">New working directory.</param>
		public static bool ChDir_NoExceptions(string WorkingDirectory)
		{
			bool Result = true;
			WorkingDirectory = ConvertSeparators(PathSeparator.Default, WorkingDirectory);
			try
			{
				Environment.CurrentDirectory = WorkingDirectory;
			}
			catch
			{
				LogWarning("Unable to change current directory to {0}", WorkingDirectory);
				Result = false;
			}
			return Result;
		}

		/// <summary>
		/// Updates a file with the specified modified and access date, creating the file if it does not already exist.
		/// An exception will be thrown if the directory does not already exist.
		/// </summary>
		/// <param name="Filename">The filename to touch, will be created if it does not exist.</param>
		/// <param name="UtcDate">The accessed and modified date to set.  If not specified, defaults to the current date and time.</param>
		public static void TouchFile(string Filename, DateTime? UtcDate = null)
		{
			var Date = UtcDate ?? DateTime.UtcNow;
			Filename = ConvertSeparators(PathSeparator.Slash, Filename);
			if (!File.Exists(Filename))
			{
				var Dir = GetDirectoryName(Filename);
				if (!DirectoryExists_NoExceptions(Dir))
				{
					throw new AutomationException(new DirectoryNotFoundException("Directory not found: " + Dir), "Unable to create file {0} as directory does not exist.", Filename);
				}
				File.Create(Filename).Dispose();
			}
			File.SetLastAccessTimeUtc(Filename, Date);
			File.SetLastWriteTimeUtc(Filename, Date);
		}

		/// <summary>
		/// Determines whether the given file is read-only
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <returns>True if the file is read-only</returns>
		public static bool IsReadOnly(string Filename)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			if (!File.Exists(Filename))
			{
				throw new AutomationException(new FileNotFoundException("File not found.", Filename), "Unable to set attributes for a non-existing file.");
			}

			FileAttributes Attributes = File.GetAttributes(Filename);
			return (Attributes & FileAttributes.ReadOnly) != 0;
		}

		/// <summary>
		/// Sets file attributes. Will not change attributes that have not been specified.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="ReadOnly">Read-only attribute</param>
		/// <param name="Hidden">Hidden attribute.</param>
		/// <param name="Archive">Archive attribute.</param>
		public static void SetFileAttributes(string Filename, bool? ReadOnly = null, bool? Hidden = null, bool? Archive = null)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			if (!File.Exists(Filename))
			{
				throw new AutomationException(new FileNotFoundException("File not found.", Filename), "Unable to set attributes for a non-existing file.");
			}

			FileAttributes Attributes = File.GetAttributes(Filename);
			Attributes = InternalSetAttributes(ReadOnly, Hidden, Archive, Attributes);
			File.SetAttributes(Filename, Attributes);
		}

		/// <summary>
		/// Sets file attributes. Will not change attributes that have not been specified.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="ReadOnly">Read-only attribute</param>
		/// <param name="Hidden">Hidden attribute.</param>
		/// <param name="Archive">Archive attribute.</param>
		public static bool SetFileAttributes_NoExceptions(string Filename, bool? ReadOnly = null, bool? Hidden = null, bool? Archive = null)
		{			
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			if (!File.Exists(Filename))
			{
				LogWarning("Unable to set attributes for a non-exisiting file ({0})", Filename);
				return false;
			}

			bool Result = true;
			try
			{
				FileAttributes Attributes = File.GetAttributes(Filename);
				Attributes = InternalSetAttributes(ReadOnly, Hidden, Archive, Attributes);
				File.SetAttributes(Filename, Attributes);
			}
			catch (Exception Ex)
			{
				LogWarning("Error trying to set file attributes for: {0}", Filename);
				LogWarning(Ex.Message);
				Result = false;
			}
			return Result;
		}

		private static FileAttributes InternalSetAttributes(bool? ReadOnly, bool? Hidden, bool? Archive, FileAttributes Attributes)
		{
			if (ReadOnly != null)
			{
				if ((bool)ReadOnly)
				{
					Attributes |= FileAttributes.ReadOnly;
				}
				else
				{
					Attributes &= ~FileAttributes.ReadOnly;
				}
			}
			if (Hidden != null)
			{
				if ((bool)Hidden)
				{
					Attributes |= FileAttributes.Hidden;
				}
				else
				{
					Attributes &= ~FileAttributes.Hidden;
				}
			}
			if (Archive != null)
			{
				if ((bool)Archive)
				{
					Attributes |= FileAttributes.Archive;
				}
				else
				{
					Attributes &= ~FileAttributes.Archive;
				}
			}
			return Attributes;
		}

		/// <summary>
		/// Writes a line of formatted string to a file. Creates the file if it does not exists.
		/// If the file does exists, appends a new line.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="Text">Text to write</param>
		public static void WriteToFile(string Filename, string Text)
		{
            Filename = ConvertSeparators(PathSeparator.Default, Filename);
            try
			{
        		        File.AppendAllText(Filename, Text + Environment.NewLine);
			}
			catch (Exception Ex)
			{
				throw new AutomationException(Ex, "Failed to Write to file {0}", Filename);
			}
		}

		/// <summary>
		/// Reads all text lines from a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <returns>Array of lines of text read from the file. null if the file did not exist or could not be read.</returns>
		public static string[] ReadAllLines(string Filename)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			return InternalUtils.SafeReadAllLines(Filename);
		}

		/// <summary>
		/// Reads all text from a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <returns>All text read from the file. null if the file did not exist or could not be read.</returns>
		public static string ReadAllText(string Filename)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			return InternalUtils.SafeReadAllText(Filename);
		}

		/// <summary>
		/// Writes lines to a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="Lines">Text</param>
		public static void WriteAllLines(string Filename, string[] Lines)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			if (!InternalUtils.SafeWriteAllLines(Filename, Lines))
			{
				throw new AutomationException("Unable to write to file: {0}", Filename);
			}
		}

		/// <summary>
		/// Writes lines to a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="Lines">Text</param>
		public static bool WriteAllLines_NoExceptions(string Filename, string[] Lines)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			return InternalUtils.SafeWriteAllLines(Filename, Lines);
		}

		/// <summary>
		/// Writes text to a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="Text">Text</param>
		public static void WriteAllText(string Filename, string Text)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			if (!InternalUtils.SafeWriteAllText(Filename, Text))
			{
				throw new AutomationException("Unable to write to file: {0}", Filename);
			}
		}

		/// <summary>
		/// Writes text to a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="Text">Text</param>
		public static bool WriteAllText_NoExceptions(string Filename, string Text)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			return InternalUtils.SafeWriteAllText(Filename, Text);
		}


		/// <summary>
		/// Writes byte array to a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="Bytes">Byte array</param>
		public static void WriteAllBytes(string Filename, byte[] Bytes)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			if (!InternalUtils.SafeWriteAllBytes(Filename, Bytes))
			{
				throw new AutomationException("Unable to write to file: {0}", Filename);
			}
		}

		/// <summary>
		/// Writes byte array to a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <param name="Bytes">Byte array</param>
		public static bool WriteAllBytes_NoExceptions(string Filename, byte[] Bytes)
		{
			Filename = ConvertSeparators(PathSeparator.Default, Filename);
			return InternalUtils.SafeWriteAllBytes(Filename, Bytes);
		}

		/// <summary>
		/// Gets a character representing the specified separator type.
		/// </summary>
		/// <param name="SeparatorType">Separator type.</param>
		/// <returns>Separator character</returns>
		public static char GetPathSeparatorChar(PathSeparator SeparatorType)
		{
			char Separator;
			switch (SeparatorType)
			{
				case PathSeparator.Slash:
				case PathSeparator.Depot:
					Separator = '/';
					break;
				case PathSeparator.Backslash:
					Separator = '\\';
					break;
				default:
					Separator = Path.DirectorySeparatorChar;
					break;
			}
			return Separator;
		}

		/// <summary>
		/// Checks if the character is one of the two sperator types ('\' or '/')
		/// </summary>
		/// <param name="Character">Character to check.</param>
		/// <returns>True if the character is a separator, false otherwise.</returns>
		public static bool IsPathSeparator(char Character)
		{
			return (Character == '/' || Character == '\\');
		}

		/// <summary>
		/// Combines paths and replaces all path separators with the system default separator.
		/// </summary>
		/// <param name="Paths"></param>
		/// <returns>Combined Path</returns>
		public static string CombinePaths(params string[] Paths)
		{
			return CombinePaths(PathSeparator.Default, Paths);
		}

		/// <summary>
		/// Combines paths and replaces all path separators wth the system specified separator.
		/// </summary>
		/// <param name="SeparatorType">Type of separartor to use when combining paths.</param>
		/// <param name="Paths"></param>
		/// <returns>Combined Path</returns>
		public static string CombinePaths(PathSeparator SeparatorType, params string[] Paths)
		{
			// Pick a separator to use.
			var SeparatorToUse = GetPathSeparatorChar(SeparatorType);
			var SeparatorToReplace = SeparatorToUse == '/' ? '\\' : '/';

			// Allocate string builder
			int CombinePathMaxLength = 0;
			foreach (var PathPart in Paths)
			{
				CombinePathMaxLength += (PathPart != null) ? PathPart.Length : 0;
			}
			CombinePathMaxLength += Paths.Length;
			var CombinedPath = new StringBuilder(CombinePathMaxLength);

			// Combine all paths
			CombinedPath.Append(Paths[0]);
			for (int PathIndex = 1; PathIndex < Paths.Length; ++PathIndex)
			{
				var NextPath = Paths[PathIndex];
				if (String.IsNullOrEmpty(NextPath) == false)
				{
					int NextPathStartIndex = 0;
					if (CombinedPath.Length != 0)
					{
						var LastChar = CombinedPath[CombinedPath.Length - 1];
						var NextChar = NextPath[0];
						var IsLastCharPathSeparator = IsPathSeparator(LastChar);
						var IsNextCharPathSeparator = IsPathSeparator(NextChar);
						// Check if a separator between paths is required
						if (!IsLastCharPathSeparator && !IsNextCharPathSeparator)
						{
							CombinedPath.Append(SeparatorToUse);
						}
						// Check if one of the saprators needs to be skipped.
						else if (IsLastCharPathSeparator && IsNextCharPathSeparator)
						{
							NextPathStartIndex = 1;
						}
					}
					CombinedPath.Append(NextPath, NextPathStartIndex, NextPath.Length - NextPathStartIndex);
				}
			}
			// Make sure there's only one separator type used.
			CombinedPath.Replace(SeparatorToReplace, SeparatorToUse);
			return CombinedPath.ToString();
		}

		/// <summary>
		/// Converts all separators in path to the specified separator type.
		/// </summary>
		/// <param name="ToSperatorType">Desired separator type.</param>
		/// <param name="PathToConvert">Path</param>
		/// <returns>Path where all separators have been converted to the specified type.</returns>
		public static string ConvertSeparators(PathSeparator ToSperatorType, string PathToConvert)
		{
			return CombinePaths(ToSperatorType, PathToConvert);
		}

		/// <summary>
		/// Copies a file, throwing an exception on failure.
		/// </summary>
		/// <param name="Source"></param>
		/// <param name="Dest"></param>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        public static void CopyFile(string Source, string Dest, bool bQuiet = false)
		{
			Source = ConvertSeparators(PathSeparator.Default, Source);
			Dest = ConvertSeparators(PathSeparator.Default, Dest);

			String DestDirName = "";
			try
			{
				DestDirName = Path.GetDirectoryName(Dest);
			}
			catch (Exception Ex)
			{
				throw new AutomationException(String.Format("Failed to get directory name for dest: {0}, {1}", Dest, Ex.Message));
			}

			if (InternalUtils.SafeFileExists(Dest, true))
			{
				InternalUtils.SafeDeleteFile(Dest, bQuiet);
			}
			else if (!InternalUtils.SafeDirectoryExists(DestDirName, true))
			{
				if (!InternalUtils.SafeCreateDirectory(DestDirName, bQuiet))
				{
					throw new AutomationException("Failed to create directory {0} for copy", DestDirName);
				}
			}
			if (InternalUtils.SafeFileExists(Dest, true))
			{
				throw new AutomationException("Failed to delete {0} for copy", Dest);
			}
			if (!InternalUtils.SafeCopyFile(Source, Dest, bQuiet))
			{
				throw new AutomationException("Failed to copy {0} to {1}", Source, Dest);
			}
		}

		/// <summary>
		/// Copies a file. Does not throw exceptions.
		/// </summary>
		/// <param name="Source"></param>
		/// <param name="Dest"></param>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <returns>True if the operation was successful, false otherwise.</returns>
        public static bool CopyFile_NoExceptions(string Source, string Dest, bool bQuiet = false)
		{
			Source = ConvertSeparators(PathSeparator.Default, Source);
			Dest = ConvertSeparators(PathSeparator.Default, Dest);
            if (InternalUtils.SafeFileExists(Dest, true))
			{
                InternalUtils.SafeDeleteFile(Dest, bQuiet);
			}
			else if (!InternalUtils.SafeDirectoryExists(Path.GetDirectoryName(Dest), true))
			{
				if (!InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(Dest)))
				{
					return false;
				}
			}
			if (InternalUtils.SafeFileExists(Dest, true))
			{
				return false;
			}
			return InternalUtils.SafeCopyFile(Source, Dest, bQuiet);
		}

		/// <summary>
		/// Copies a file if the dest doesn't exist or (optionally) if the dest timestamp is different; after a copy, copies the timestamp
		/// </summary>
		/// <param name="Source">The full path to the source file</param>
		/// <param name="Dest">The full path to the destination file</param>
		/// <param name="bAllowDifferingTimestamps">If true, will always skip a file if the destination exists, even if timestamp differs; defaults to false</param>
		/// <returns>True if the operation was successful, false otherwise.</returns>
		public static void CopyFileIncremental(FileReference Source, FileReference Dest, bool bAllowDifferingTimestamps = false, bool bFilterSpecialLinesFromIniFiles = false)
		{
			if (InternalUtils.SafeFileExists(Dest.FullName, true))
			{
				if (bAllowDifferingTimestamps == true)
				{
					LogVerbose("CopyFileIncremental Skipping {0}, already exists", Dest);
					return;
				}
				TimeSpan Diff = File.GetLastWriteTimeUtc(Dest.FullName) - File.GetLastWriteTimeUtc(Source.FullName);
				if (Diff.TotalSeconds > -1 && Diff.TotalSeconds < 1)
				{
					LogVerbose("CopyFileIncremental Skipping {0}, up to date.", Dest);
					return;
				}
				InternalUtils.SafeDeleteFile(Dest.FullName);
			}
			else if (!InternalUtils.SafeDirectoryExists(Path.GetDirectoryName(Dest.FullName), true))
			{
				if (!InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(Dest.FullName)))
				{
					throw new AutomationException("Failed to create directory {0} for copy", Path.GetDirectoryName(Dest.FullName));
				}
			}
			if (InternalUtils.SafeFileExists(Dest.FullName, true))
			{
				throw new AutomationException("Failed to delete {0} for copy", Dest);
			}
			if (!InternalUtils.SafeCopyFile(Source.FullName, Dest.FullName, bFilterSpecialLinesFromIniFiles:bFilterSpecialLinesFromIniFiles))
			{
				throw new AutomationException("Failed to copy {0} to {1}", Source, Dest);
			}
			FileAttributes Attributes = File.GetAttributes(Dest.FullName);
			if ((Attributes & FileAttributes.ReadOnly) != 0)
			{
				File.SetAttributes(Dest.FullName, Attributes & ~FileAttributes.ReadOnly);
			}
			File.SetLastWriteTimeUtc(Dest.FullName, File.GetLastWriteTimeUtc(Source.FullName));
		}

		/// <summary>
		/// Copies a directory and all of it's contents recursively. Does not throw exceptions.
		/// </summary>
		/// <param name="Source"></param>
		/// <param name="Dest"></param>
        /// <param name="bQuiet">When true, logging is suppressed.</param>
        /// <returns>True if the operation was successful, false otherwise.</returns>
		public static bool CopyDirectory_NoExceptions(string Source, string Dest, bool bQuiet = false)
		{
			Source = ConvertSeparators(PathSeparator.Default, Source);
			Dest = ConvertSeparators(PathSeparator.Default, Dest);
			Dest = Dest.TrimEnd(new char[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar });

			if (InternalUtils.SafeDirectoryExists(Dest))
			{
				InternalUtils.SafeDeleteDirectory(Dest, bQuiet);
				if (InternalUtils.SafeDirectoryExists(Dest, true))
				{
					return false;
				}
			}

			if (!InternalUtils.SafeCreateDirectory(Dest, bQuiet))
			{
				return false;
			}
			foreach (var SourceSubDirectory in Directory.GetDirectories(Source))
			{
				string DestPath = Dest + GetPathSeparatorChar(PathSeparator.Default) + GetLastDirectoryName(SourceSubDirectory + GetPathSeparatorChar(PathSeparator.Default));
				if (!CopyDirectory_NoExceptions(SourceSubDirectory, DestPath, bQuiet))
				{
					return false;
				}
			}
			foreach (var SourceFile in Directory.GetFiles(Source))
			{
				int FilenameStart = SourceFile.LastIndexOf(GetPathSeparatorChar(PathSeparator.Default));
				string DestPath = Dest + SourceFile.Substring(FilenameStart);
				if (!CopyFile_NoExceptions(SourceFile, DestPath, bQuiet))
				{
					return false;
				}
			}

			return true;
		}

		/// <summary>
		/// Returns directory name without filename. 
		/// The difference between this and Path.GetDirectoryName is that this
		/// function will not throw away the last name if it doesn't have an extension, for example:
		/// D:\Project\Data\Asset -> D:\Project\Data\Asset
		/// D:\Project\Data\Asset.ussset -> D:\Project\Data
		/// </summary>
		/// <param name="FilePath"></param>
		/// <returns></returns>
		public static string GetDirectoryName(string FilePath)
		{
			var LastSeparatorIndex = Math.Max(FilePath.LastIndexOf('/'), FilePath.LastIndexOf('\\'));
			var ExtensionIndex = FilePath.LastIndexOf('.');
			if (ExtensionIndex > LastSeparatorIndex || LastSeparatorIndex == (FilePath.Length - 1))
			{
				return FilePath.Substring(0, LastSeparatorIndex);
			}
			else
			{
				return FilePath;
			}
		}

		/// <summary>
		/// Returns the last directory name in the path string.
		/// For example: D:\Temp\Project\File.txt -> Project, Data\Samples -> Samples
		/// </summary>
		/// <param name="FilePath"></param>
		/// <returns></returns>
		public static string GetLastDirectoryName(string FilePath)
		{
			var LastDir = GetDirectoryName(FilePath);
			var LastSeparatorIndex = Math.Max(LastDir.LastIndexOf('/'), LastDir.LastIndexOf('\\'));
			if (LastSeparatorIndex >= 0)
			{
				LastDir = LastDir.Substring(LastSeparatorIndex + 1);
			}
			return LastDir;
		}

		/// <summary>
		/// Removes multi-dot extensions from a filename (i.e. *.automation.csproj)
		/// </summary>
		/// <param name="Filename">Filename to remove the extensions from</param>
		/// <returns>Clean filename.</returns>
		public static string GetFilenameWithoutAnyExtensions(string Filename)
		{
			do
			{
				Filename = Path.GetFileNameWithoutExtension(Filename);
			}
			while (Filename.IndexOf('.') >= 0);
			return Filename;
		}

		/// <summary>
		/// Reads a file manifest and returns it
		/// </summary>
		/// <param name="ManifestName">ManifestName</param>
		/// <returns></returns>
		public static UnrealBuildTool.BuildManifest ReadManifest(string ManifestName)
		{
			// Create a new default instance of the type
			UnrealBuildTool.BuildManifest Instance = new UnrealBuildTool.BuildManifest();
			XmlReader XmlStream = null;
			try
			{
				// Use the default reader settings if none are passed in
				XmlReaderSettings ReaderSettings = new XmlReaderSettings();
				ReaderSettings.CloseInput = true;
				ReaderSettings.IgnoreComments = true;

				// Get the xml data stream to read from
				XmlStream = XmlReader.Create( ManifestName, ReaderSettings );

				// Creates an instance of the XmlSerializer class so we can read the settings object
				XmlSerializer ObjectReader = new XmlSerializer( typeof( UnrealBuildTool.BuildManifest ) );

				// Create an object from the xml data
				Instance = ( UnrealBuildTool.BuildManifest )ObjectReader.Deserialize( XmlStream );
			}
			catch( Exception Ex )
			{
				Debug.WriteLine( Ex.Message );
			}
			finally
			{
				if( XmlStream != null )
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}

			return Instance;
		}

		private static void CloneDirectoryRecursiveWorker(string SourcePathBase, string TargetPathBase, List<string> ClonedFiles, bool bIncremental = false)
		{
			bool bDirectoryCreated = InternalUtils.SafeCreateDirectory(TargetPathBase);
			if (!bIncremental && !bDirectoryCreated)
            {
                throw new AutomationException("Failed to create directory {0} for copy", TargetPathBase);
            }
			else if (bIncremental && !CommandUtils.DirectoryExists_NoExceptions(TargetPathBase))
			{
				throw new AutomationException("Target directory {0} does not exist", TargetPathBase);
			}

			DirectoryInfo SourceDirectory = new DirectoryInfo(SourcePathBase);
			DirectoryInfo[] SourceSubdirectories = SourceDirectory.GetDirectories();

			// Copy the files
			FileInfo[] SourceFiles = SourceDirectory.GetFiles();
			foreach (FileInfo SourceFI in SourceFiles)
			{
				string TargetFilename = CommandUtils.CombinePaths(TargetPathBase, SourceFI.Name);

				if (!bIncremental || !CommandUtils.FileExists(TargetFilename))
				{
				SourceFI.CopyTo(TargetFilename);

				if (ClonedFiles != null)
				{
					ClonedFiles.Add(TargetFilename);
				}
			}
			}

			// Recurse into subfolders
			foreach (DirectoryInfo SourceSubdir in SourceSubdirectories)
			{
				string NewSourcePath = CommandUtils.CombinePaths(SourcePathBase, SourceSubdir.Name);
				string NewTargetPath = CommandUtils.CombinePaths(TargetPathBase, SourceSubdir.Name);
				CloneDirectoryRecursiveWorker(NewSourcePath, NewTargetPath, ClonedFiles, bIncremental);
			}
		}

		/// <summary>
		/// Clones a directory.
		/// Warning: Will delete all of the existing files in TargetPath
		/// This is recursive, copying subfolders too.
		/// </summary>
		/// <param name="SourcePath">Source directory.</param>
		/// <param name="TargetPath">Target directory.</param>
		/// <param name="ClonedFiles">List of cloned files.</param>
		public static void CloneDirectory(string SourcePath, string TargetPath, List<string> ClonedFiles = null)
		{
			DeleteDirectory_NoExceptions(TargetPath);
			CloneDirectoryRecursiveWorker(SourcePath, TargetPath, ClonedFiles);
		}

		/// <summary>
		/// Clones a directory, skipping any files which already exist in the destination.
		/// This is recursive, copying subfolders too.
		/// </summary>
		/// <param name="SourcePath">Source directory.</param>
		/// <param name="TargetPath">Target directory.</param>
		/// <param name="ClonedFiles">List of cloned files.</param>
		public static void CloneDirectoryIncremental(string SourcePath, string TargetPath, List<string> ClonedFiles = null)
		{
			CloneDirectoryRecursiveWorker(SourcePath, TargetPath, ClonedFiles, bIncremental: true);
		}

		#endregion

		#region Threaded Copy

        /// <summary>
		/// Copies files using multiple threads
		/// </summary>
		/// <param name="SourceDirectory"></param>
		/// <param name="DestDirectory"></param>
		/// <param name="MaxThreads"></param>
		public static void ThreadedCopyFiles(string SourceDirectory, string DestDirectory, int MaxThreads = 64)
		{
			CreateDirectory(DestDirectory);
            var SourceFiles = Directory.EnumerateFiles(SourceDirectory, "*", SearchOption.AllDirectories).ToList();
            var DestFiles = SourceFiles.Select(SourceFile => CommandUtils.MakeRerootedFilePath(SourceFile, SourceDirectory, DestDirectory)).ToList();
			ThreadedCopyFiles(SourceFiles, DestFiles, MaxThreads);
		}

		/// <summary>
		/// Copies files using multiple threads
        /// </summary>
		/// <param name="Source"></param>
		/// <param name="Dest"></param>
		/// <param name="MaxThreads"></param>
		public static void ThreadedCopyFiles(List<string> Source, List<string> Dest, int MaxThreads = 64, bool bQuiet = false)
		{
			if(!bQuiet)
			{
				Log("Copying {0} file(s) using max {1} thread(s)", Source.Count, MaxThreads);
			}

            if (Source.Count != Dest.Count)
			{
                throw new AutomationException("Source count ({0}) does not match Dest count ({1})", Source.Count, Dest.Count);
			}
			Parallel.ForEach(Source.Zip(Dest, (Src, Dst) => new { SourceFile = Src, DestFile = Dst }), new ParallelOptions { MaxDegreeOfParallelism = MaxThreads }, (Pair) =>
			{
				CommandUtils.CopyFile(Pair.SourceFile, Pair.DestFile, true);
			});
        }

		/// <summary>
		/// Copies a set of files from one folder to another
		/// </summary>
		/// <param name="SourceDir">Source directory</param>
		/// <param name="TargetDir">Target directory</param>
		/// <param name="RelativePaths">Paths relative to the source directory to copy</param>
		/// <param name="MaxThreads">Maximum number of threads to create</param>
		/// <returns>List of filenames copied to the target directory</returns>
		public static List<string> ThreadedCopyFiles(string SourceDir, string TargetDir, List<string> RelativePaths, int MaxThreads = 64)
		{
            var SourceFileNames = RelativePaths.Select(RelativePath => CommandUtils.CombinePaths(SourceDir, RelativePath)).ToList();
            var TargetFileNames = RelativePaths.Select(RelativePath => CommandUtils.CombinePaths(TargetDir, RelativePath)).ToList();
			CommandUtils.ThreadedCopyFiles(SourceFileNames, TargetFileNames, MaxThreads);
			return TargetFileNames;
		}

		/// <summary>
		/// Copies a set of files from one folder to another
		/// </summary>
		/// <param name="SourceDir">Source directory</param>
		/// <param name="TargetDir">Target directory</param>
		/// <param name="Filter">Filter which selects files from the source directory to copy</param>
		/// <param name="bIgnoreSymlinks">Whether to ignore symlinks during the copy</param>
		/// <param name="MaxThreads">Maximum number of threads to create</param>
		/// <returns>List of filenames copied to the target directory</returns>
		public static List<string> ThreadedCopyFiles(string SourceDir, string TargetDir, FileFilter Filter, bool bIgnoreSymlinks, int MaxThreads = 64)
		{
			// Filter all the relative paths
			Log("Applying filter to {0}...", SourceDir);
			DirectoryReference SourceDirRef = new DirectoryReference(SourceDir);
			var RelativePaths = Filter.ApplyToDirectory(SourceDirRef, bIgnoreSymlinks).Select(x => x.MakeRelativeTo(SourceDirRef)).ToList();
			return ThreadedCopyFiles(SourceDir, TargetDir, RelativePaths);
		}

		/// <summary>
		/// Moves files in parallel
        /// </summary>
		/// <param
		/// <param name="SourceAndTargetPairs">Pairs of source and target files</param>
		public static void ParallelMoveFiles(IEnumerable<KeyValuePair<FileReference, FileReference>> SourceAndTargetPairs)
		{
            try
            {
                Parallel.ForEach(SourceAndTargetPairs, x => MoveFile(x.Key, x.Value));
            }
            catch (AggregateException Ex)
            {
                throw new AutomationException(Ex, "Failed to thread-copy files.");
            }
        }

		/// <summary>
		/// Move a file from one place to another 
		/// </summary>
		/// <param name="SourceAndTarget">Source and target file</param>
		public static void MoveFile(FileReference SourceFile, FileReference TargetFile)
		{
			// Create the directory for the target file
			try
			{
				Directory.CreateDirectory(TargetFile.Directory.FullName);
			}
			catch(Exception Ex)
			{
				throw new AutomationException(Ex, "Unable to create directory {0} while moving {1} to {2}", TargetFile.Directory, SourceFile, TargetFile);
			}

			// Move the file
			try
			{
				File.Move(SourceFile.FullName, TargetFile.FullName);
			}
			catch(Exception Ex)
			{
				throw new AutomationException(Ex, "Unable to move {0} to {1}", SourceFile, TargetFile);
			}
		}

		#endregion

		#region Environment variables

		/// <summary>
		/// Gets environment variable value.
		/// </summary>
		/// <param name="Name">Name of the environment variable</param>
		/// <returns>Environment variable value as string.</returns>
		public static string GetEnvVar(string Name)
		{
			return InternalUtils.GetEnvironmentVariable(Name, "");
		}

		/// <summary>
		/// Gets environment variable value.
		/// </summary>
		/// <param name="Name">Name of the environment variable</param>
		/// <param name="DefaultValue">Default value of the environment variable if the variable is not set.</param>
		/// <returns>Environment variable value as string.</returns>
		public static string GetEnvVar(string Name, string DefaultValue)
		{
			return InternalUtils.GetEnvironmentVariable(Name, DefaultValue);
		}

		/// <summary>
		/// Sets environment variable.
		/// </summary>
		/// <param name="Name">Variable name.</param>
		/// <param name="Value">Variable value.</param>
		/// <returns>True if the value has been set, false otherwise.</returns>
		public static void SetEnvVar(string Name, object Value)
		{
			try
			{
				LogLog("SetEnvVar {0}={1}", Name, Value);
				Environment.SetEnvironmentVariable(Name, Value.ToString());
			}
			catch (Exception Ex)
			{
				throw new AutomationException(Ex, "Failed to set environment variable {0} to {1}", Name, Value);
			}
		}

		/// <summary>
		/// Sets the environment variable if it hasn't been already.
		/// </summary>
		/// <param name="VarName">Environment variable name</param>
		/// <param name="Value">New value</param>
		public static void ConditionallySetEnvVar(string VarName, string Value)
		{
			if (String.IsNullOrEmpty(CommandUtils.GetEnvVar(VarName)))
			{
				Environment.SetEnvironmentVariable(VarName, Value);
			}
		}

		#endregion

		#region CommandLine

		/// <summary>
		/// Converts a list of arguments to a string where each argument is separated with a space character.
		/// </summary>
		/// <param name="Args">Arguments</param>
		/// <returns>Single string containing all arguments separated with a space.</returns>
		public static string FormatCommandLine(IEnumerable<string> Arguments)
		{
			StringBuilder Result = new StringBuilder();
			foreach(string Argument in Arguments)
			{
				if(Result.Length > 0)
				{
					Result.Append(" ");
				}
				Result.Append(FormatArgumentForCommandLine(Argument));
			}
			return Result.ToString();
		}

		/// <summary>
		/// Format a single argument for passing on the command line, inserting quotes as necessary.
		/// </summary>
		/// <param name="Argument">The argument to quote</param>
		/// <returns>The argument, with quotes if necessary</returns>
		public static string FormatArgumentForCommandLine(string Argument)
		{
			// Check if the argument contains a space. If not, we can just pass it directly.
			int SpaceIdx = Argument.IndexOf(' ');
			if(SpaceIdx == -1)
			{
				return Argument;
			}

			// If it does have a space, and it's formatted as an option (ie. -Something=), try to insert quotes after the equals character
			int EqualsIdx = Argument.IndexOf('=');
			if(Argument.StartsWith("-") && EqualsIdx != -1 && EqualsIdx < SpaceIdx)
			{
				return String.Format("{0}=\"{1}\"", Argument.Substring(0, EqualsIdx), Argument.Substring(EqualsIdx + 1));
			}
			else
			{
				return String.Format("\"{0}\"", Argument);
			}
		}

		/// <summary>
		/// Parses the argument list for a parameter and returns whether it is defined or not.
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to check for.</param>
		/// <returns>True if param was found, false otherwise.</returns>
		public static bool ParseParam(object[] ArgList, string Param)
		{
            string ValueParam = Param;
            if (!ValueParam.EndsWith("="))
            {
                ValueParam += "=";
            }

            foreach (object Arg in ArgList)
			{
                string ArgStr = Arg.ToString();
                if (ArgStr.Equals(Param, StringComparison.InvariantCultureIgnoreCase) || ArgStr.StartsWith(ValueParam, StringComparison.InvariantCultureIgnoreCase))
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Parses the argument list for a parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public static string ParseParamValue(object[] ArgList, string Param, string Default = null)
		{
			if (!Param.EndsWith("="))
			{
				Param += "=";
			}

			foreach (object Arg in ArgList)
			{
				string ArgStr = Arg.ToString();
				if (ArgStr.StartsWith(Param, StringComparison.InvariantCultureIgnoreCase))
				{
					return ArgStr.Substring(Param.Length);
				}
			}
			return Default;
		}

		/// <summary>
		/// Parses the argument list for any number of parameters.
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns an array of values for this parameter (or an empty array if one was not found.</returns>
		public static string[] ParseParamValues(object[] ArgList, string Param)
		{
			string ParamEquals = Param;
			if (!ParamEquals.EndsWith("="))
			{
				ParamEquals += "=";
			}

			List<string> Values = new List<string>();
			foreach (object Arg in ArgList)
			{
				string ArgStr = Arg.ToString();
				if (ArgStr.StartsWith(ParamEquals, StringComparison.InvariantCultureIgnoreCase))
				{
					Values.Add(ArgStr.Substring(ParamEquals.Length));
				}
			}
			return Values.ToArray();
		}

		/// <summary>
		/// Makes sure path can be used as a command line param (adds quotes if it contains spaces)
		/// </summary>
		/// <param name="InPath">Path to convert</param>
		/// <returns></returns>
		public static string MakePathSafeToUseWithCommandLine(string InPath)
		{
			if (InPath.Contains(' ') && InPath[0] != '\"')
			{
				InPath = "\"" + InPath + "\"";
			}
			return InPath;
		}

		#endregion

		#region Other

		public static string EscapePath(string InPath)
		{
			return InPath.Replace(":", "").Replace("/", "+").Replace("\\", "+").Replace(" ", "+");
		}

		/// <summary>
		/// Checks if collection is either null or empty.
		/// </summary>
		/// <param name="Collection">Collection to check.</param>
		/// <returns>True if the collection is either nur or empty.</returns>
		public static bool IsNullOrEmpty(ICollection Collection)
		{
			return Collection == null || Collection.Count == 0;
		}

	    #endregion

		#region Properties

		/// <summary>
		/// Checks if this command is running on a build machine.
		/// </summary>
		public static bool IsBuildMachine
		{
			get { return Automation.IsBuildMachine; }
		}

		/// <summary>
		/// Path to the root directory
		/// </summary>
		public static readonly DirectoryReference RootDirectory = new DirectoryReference(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().GetOriginalLocation()), "..", "..", ".."));

		/// <summary>
		/// Path to the engine directory
		/// </summary>
		public static readonly DirectoryReference EngineDirectory = DirectoryReference.Combine(RootDirectory, "Engine");

		/// <summary>
		/// Telemetry data for the current run. Add -WriteTelemetry=<Path> to the command line to export to disk.
		/// </summary>
		public static TelemetryData Telemetry = new TelemetryData();

		#endregion

        /// <summary>
        /// Cached location of the build root storage because the logic to compute it is a little non-trivial.
        /// </summary>
        private static string CachedRootBuildStorageDirectory;

        /// <summary>
        /// "P:\Builds" or "/Volumes/Builds". Root Folder for all build storage.
        /// </summary>
        /// <returns>"P:\Builds" or "/Volumes/Builds" unless overridden by -UseLocalBuildStorage from the commandline, where is uses Engine\Saved\LocalBuilds\.</returns>
        public static string RootBuildStorageDirectory()
        {
            if (string.IsNullOrEmpty(CachedRootBuildStorageDirectory))
            {
                if (GlobalCommandLine.UseLocalBuildStorage)
                {
                    CachedRootBuildStorageDirectory = CombinePaths(CmdEnv.LocalRoot, "Engine", "Saved", "LocalBuilds");
                    // Must create the directory because much of the system assumes it is already there.
                    CreateDirectory(CombinePaths(CachedRootBuildStorageDirectory, "UE4"));
                }
                else
                {
                    CachedRootBuildStorageDirectory = Utils.IsRunningOnMono ? "/Volumes/Builds" : CombinePaths("P:", "Builds");
                }
            }
            return CachedRootBuildStorageDirectory;
        }

        public static bool DirectoryExistsAndIsWritable_NoExceptions(string Dir)
        {
            if (!DirectoryExists_NoExceptions(Dir))
            {
				LogLog("Directory {0} does not exist", Dir);
				return false;
			}

            try
            {
				string Filename = CombinePaths(Dir, Guid.NewGuid().ToString() + ".Temp.txt");
				string NativeFilename = ConvertSeparators(PathSeparator.Default, Filename);
				using(StreamWriter Writer = new StreamWriter(NativeFilename))
				{
					Writer.Write("Test");
				}
				if(File.Exists(NativeFilename))
				{
		            DeleteFile_NoExceptions(Filename, true);
		            LogLog("Directory {0} is writable", Dir);
					return true;
				}
			}
			catch(Exception)
			{
			}

			LogLog("Directory {0} is not writable", Dir);
			return false;
		}

		public static void CleanFormalBuilds(string ParentDir, string SearchPattern, int MaximumDaysToKeepTempStorage = 4)
        {
            if (!IsBuildMachine || !ParentDir.StartsWith(RootBuildStorageDirectory()))
            {
                return;
            }
            try
            {
                DirectoryInfo DirInfo = new DirectoryInfo(ParentDir);
				Log("Looking for directories to delete in {0}", ParentDir);
                foreach (DirectoryInfo ThisDirInfo in DirInfo.EnumerateDirectories(SearchPattern))
                {
					double AgeDays = (DateTime.UtcNow - ThisDirInfo.CreationTimeUtc).TotalDays;
					if (AgeDays > MaximumDaysToKeepTempStorage)
                    {
                        Log("Deleting formal build directory {0}, because it is {1} days old (maximum {2}).", ThisDirInfo.FullName, (int)AgeDays, MaximumDaysToKeepTempStorage);
                        DeleteDirectory_NoExceptions(true, ThisDirInfo.FullName);
                    }
                    else
                    {
						LogVerbose("Not deleting formal build directory {0}, because it is {1} days old (maximum {2}).", ThisDirInfo.FullName, (int)AgeDays, MaximumDaysToKeepTempStorage);
                    }
                }
            }
            catch (Exception Ex)
            {
                LogWarning("Unable to clean formal builds from directory: {0}", ParentDir);
                LogWarning(" Exception was {0}", LogUtils.FormatException(Ex));
            }
        }


		/// <summary>
		/// Returns the generic name for a given platform (eg. "Windows" for Win32/Win64)
		/// </summary>
		/// <param name="Platform">Specific platform</param>
		public static string GetGenericPlatformName(UnrealBuildTool.UnrealTargetPlatform Platform)
		{
			if(Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64)
			{
				return "Windows";
			}
			else
			{
				return Enum.GetName(typeof(UnrealBuildTool.UnrealTargetPlatform), Platform);
			}
		}

		/// <summary>
		/// Creates a zip file containing the given input files
		/// </summary>
		/// <param name="ZipFileName">Filename for the zip</param>
		/// <param name="Filter">Filter which selects files to be included in the zip</param>
		/// <param name="BaseDirectory">Base directory to store relative paths in the zip file to</param>
		public static void ZipFiles(FileReference ZipFileName, DirectoryReference BaseDirectory, FileFilter Filter)
		{
			// Ionic.Zip.Zip64Option.Always option produces broken archives on Mono, so we use system zip tool instead
			if (Utils.IsRunningOnMono)
			{
				CommandUtils.CreateDirectory(Path.GetDirectoryName(ZipFileName.FullName));
				CommandUtils.PushDir(BaseDirectory.FullName);
				string FilesList = "";
				foreach (FileReference FilteredFile in Filter.ApplyToDirectory(BaseDirectory, true))
				{
					FilesList += " \"" + FilteredFile.MakeRelativeTo(BaseDirectory) + "\"";
					if (FilesList.Length > 32000)
					{
						CommandUtils.RunAndLog(CommandUtils.CmdEnv, "zip", "-g -q \"" + ZipFileName + "\"" + FilesList);
						FilesList = "";
					}
				}
				if (FilesList.Length > 0)
				{
					CommandUtils.RunAndLog(CommandUtils.CmdEnv, "zip", "-g -q \"" + ZipFileName + "\"" + FilesList);
				}
				CommandUtils.PopDir();
			}
			else
			{
				using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile())
				{
					Zip.UseZip64WhenSaving = Ionic.Zip.Zip64Option.Always;
					foreach (FileReference FilteredFile in Filter.ApplyToDirectory(BaseDirectory, true))
					{
						Zip.AddFile(FilteredFile.FullName, Path.GetDirectoryName(FilteredFile.MakeRelativeTo(BaseDirectory)));
					}
					CommandUtils.CreateDirectory(Path.GetDirectoryName(ZipFileName.FullName));
					Zip.Save(ZipFileName.FullName);
				}
			}
		}

		/// <summary>
		/// Creates a zip file containing the given input files
		/// </summary>
		/// <param name="ZipFile">Filename for the zip</param>
		/// <param name="BaseDirectory">Base directory to store relative paths in the zip file to</param>
		/// <param name="Files">Files to include in the archive</param>
		public static void ZipFiles(FileReference ZipFile, DirectoryReference BaseDirectory, IEnumerable<FileReference> Files)
		{
			// Ionic.Zip.Zip64Option.Always option produces broken archives on Mono, so we use system zip tool instead
			if (Utils.IsRunningOnMono)
			{
				CommandUtils.CreateDirectory(ZipFile.Directory.FullName);
 				CommandUtils.PushDir(BaseDirectory.FullName);
 				string FilesList = "";
				foreach(FileReference File in Files)
				{
					FilesList += " \"" + File.MakeRelativeTo(BaseDirectory) + "\"";
					if (FilesList.Length > 32000)
					{
						CommandUtils.RunAndLog(CommandUtils.CmdEnv, "zip", "-g -q \"" + ZipFile.FullName + "\"" + FilesList);
						FilesList = "";
					}
				}
				if (FilesList.Length > 0)
				{
					CommandUtils.RunAndLog(CommandUtils.CmdEnv, "zip", "-g -q \"" + ZipFile.FullName + "\"" + FilesList);
				}
				CommandUtils.PopDir();
			}
			else
			{
				Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile();
				Zip.UseZip64WhenSaving = Ionic.Zip.Zip64Option.Always;
				foreach(FileReference File in Files)
				{
					Zip.AddFile(File.FullName, Path.GetDirectoryName(File.MakeRelativeTo(BaseDirectory)));
				}
				CommandUtils.CreateDirectory(ZipFile.Directory.FullName);
				Zip.Save(ZipFile.FullName);
			}
		}

		/// <summary>
		/// Extracts the contents of a zip file
		/// </summary>
		/// <param name="ZipFileName">Name of the zip file</param>
		/// <param name="BaseDirectory">Output directory</param>
		/// <returns>List of files written</returns>
		public static IEnumerable<string> UnzipFiles(string ZipFileName, string BaseDirectory)
		{
			List<string> OutputFileNames = new List<string>();
			if (Utils.IsRunningOnMono)
			{
				CommandUtils.CreateDirectory(BaseDirectory);

				// Use system unzip tool as there have been instances of Ionic not being able to open zips created with Mac zip tool
				string Output = CommandUtils.RunAndLog("unzip", "\"" + ZipFileName + "\" -d \"" + BaseDirectory + "\"", Options: ERunOptions.Default | ERunOptions.SpewIsVerbose);

				// Split log output into lines
				string[] Lines = Output.Split(new char[] { '\n', '\r' });

				foreach (string LogLine in Lines)
				{
					CommandUtils.Log(LogLine);

					// Split each line into two by whitespace
					string[] SplitLine = LogLine.Split(new char[] { ' ', '\t' }, 2, StringSplitOptions.RemoveEmptyEntries);
					if (SplitLine.Length == 2)
					{
						// Second part of line should be a path
						string FilePath = SplitLine[1].Trim();
						CommandUtils.Log(FilePath);
						if (File.Exists(FilePath) && !OutputFileNames.Contains(FilePath) && FilePath != ZipFileName)
						{
							if (CommandUtils.IsProbablyAMacOrIOSExe(FilePath))
							{
								FixUnixFilePermissions(FilePath);
							}
							OutputFileNames.Add(FilePath);
						}
					}
				}
				if (OutputFileNames.Count == 0)
				{
					CommandUtils.LogWarning("Unable to parse unzipped files from {0}", ZipFileName);
				}
			}
			else
			{
				// manually extract the files. There was a problem with the Ionic.Zip library that required this on non-PC at one point,
				// but that problem is now fixed. Leaving this code as is as we need to return the list of created files anyway.
				using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile(ZipFileName))
				{
					
					foreach (Ionic.Zip.ZipEntry Entry in Zip.Entries.Where(x => !x.IsDirectory))
					{
						string OutputFileName = Path.Combine(BaseDirectory, Entry.FileName);
						Directory.CreateDirectory(Path.GetDirectoryName(OutputFileName));
						using (FileStream OutputStream = new FileStream(OutputFileName, FileMode.Create, FileAccess.Write))
						{
							Entry.Extract(OutputStream);
						}
						OutputFileNames.Add(OutputFileName);
					}
				}
			}
			return OutputFileNames;
		}

		/// <summary>
		/// Resolve an arbitrary file specification against a directory. May contain any number of p4 wildcard operators (?, *, ...).
		/// </summary>
		/// <param name="DefaultDir">Base directory for relative paths</param>
		/// <param name="Pattern">Pattern to match</param>
		/// <param name="ExcludePatterns">List of patterns to be excluded. May be null.</param>
		/// <returns>Sequence of file references matching the given pattern</returns>
		public static IEnumerable<FileReference> ResolveFilespec(DirectoryReference DefaultDir, string Pattern, IEnumerable<string> ExcludePatterns)
		{
			List<FileReference> Files = new List<FileReference>();

			// Check if it contains any wildcards. If not, we can just add the pattern directly without searching.
			int WildcardIdx = FileFilter.FindWildcardIndex(Pattern);
			if(WildcardIdx == -1)
			{
				// Construct a filter which removes all the excluded filetypes
				FileFilter Filter = new FileFilter(FileFilterType.Include);
				if(ExcludePatterns != null)
				{
					Filter.AddRules(ExcludePatterns, FileFilterType.Exclude);
				}

				// Match it against the given file
				FileReference File = FileReference.Combine(DefaultDir, Pattern);
				if(Filter.Matches(File.FullName))
				{
					Files.Add(File);
				}
			}
			else
			{
				// Find the base directory for the search. We construct this in a very deliberate way including the directory separator itself, so matches
				// against the OS root directory will resolve correctly both on Mac (where / is the filesystem root) and Windows (where / refers to the current drive).
				int LastDirectoryIdx = Pattern.LastIndexOfAny(new char[]{ Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar }, WildcardIdx);
				DirectoryReference BaseDir = DirectoryReference.Combine(DefaultDir, Pattern.Substring(0, LastDirectoryIdx + 1));

				// Construct the absolute include pattern to match against, re-inserting the resolved base directory to construct a canonical path.
				string IncludePattern = BaseDir.FullName.TrimEnd(new char[]{ Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar }) + "/" + Pattern.Substring(LastDirectoryIdx + 1);

				// Construct a filter and apply it to the directory
				if(DirectoryReference.Exists(BaseDir))
				{
					FileFilter Filter = new FileFilter();
					Filter.AddRule(IncludePattern, FileFilterType.Include);
					if(ExcludePatterns != null)
					{
						Filter.AddRules(ExcludePatterns, FileFilterType.Exclude);
					}
					Files.AddRange(Filter.ApplyToDirectory(BaseDir, BaseDir.FullName, true));
				}
			}
			return Files;
		}
	}

	/// <summary>
	/// Valid units for telemetry data
	/// </summary>
	public enum TelemetryUnits
	{
		Count,
		Milliseconds,
		Seconds,
		Minutes,
		Bytes,
		Megabytes,
	}

	/// <summary>
	/// Sample for telemetry data
	/// </summary>
	public class TelemetrySample
	{
		/// <summary>
		/// Name of this sample
		/// </summary>
		public string Name;

		/// <summary>
		/// The value for this sample
		/// </summary>
		public double Value;

		/// <summary>
		/// Units for this sample
		/// </summary>
		public TelemetryUnits Units;
	}

	/// <summary>
	/// Stores a set of key/value telemetry samples which can be read and written to a JSON file
	/// </summary>
	public class TelemetryData
	{
		/// <summary>
		/// The current file version
		/// </summary>
		const int CurrentVersion = 1;

		/// <summary>
		/// Maps from a sample name to its value
		/// </summary>
		public List<TelemetrySample> Samples = new List<TelemetrySample>();

		/// <summary>
		/// Adds a telemetry sample
		/// </summary>
		/// <param name="Name">Name of the sample</param>
		/// <param name="Value">Value of the sample</param>
		/// <param name="Units">Units for the sample value</param>
		public void Add(string Name, double Value, TelemetryUnits Units)
		{
			Samples.RemoveAll(x => x.Name == Name);
			Samples.Add(new TelemetrySample() { Name = Name, Value = Value, Units = Units });
		}

		/// <summary>
		/// Add samples from another telemetry block into this one
		/// </summary>
		/// <param name="Prefix">Prefix for the telemetry data</param>
		/// <param name="Other">The other telemetry data</param>
		public void Merge(string Prefix, TelemetryData Other)
		{
			foreach(TelemetrySample Sample in Other.Samples)
			{
				Add(Sample.Name, Sample.Value, Sample.Units);
			}
		}

		/// <summary>
		/// Tries to read the telemetry data from the given file
		/// </summary>
		/// <param name="FileName">The file to read from</param>
		/// <param name="Telemetry">On success, the read telemetry data</param>
		/// <returns>True if a telemetry object was read</returns>
		public static bool TryRead(FileReference FileName, out TelemetryData Telemetry)
		{
			// Try to read the raw json object
			JsonObject RawObject;
			if (!JsonObject.TryRead(FileName, out RawObject))
			{
				Telemetry = null;
				return false;
			}

			// Check the version is valid
			int Version;
			if(!RawObject.TryGetIntegerField("Version", out Version) || Version != CurrentVersion)
			{
				Telemetry = null;
				return false;
			}

			// Read all the samples
			JsonObject[] RawSamples;
			if (!RawObject.TryGetObjectArrayField("Samples", out RawSamples))
			{
				Telemetry = null;
				return false;
			}

			// Parse out all the samples
			Telemetry = new TelemetryData();
			foreach (JsonObject RawSample in RawSamples)
			{
				Telemetry.Add(RawSample.GetStringField("Name"), RawSample.GetDoubleField("Value"), RawSample.GetEnumField<TelemetryUnits>("Units"));
			}
			return true;
		}

		/// <summary>
		/// Writes out the telemetry data to a file
		/// </summary>
		/// <param name="FileName"></param>
		public void Write(string FileName)
		{
			using (JsonWriter Writer = new JsonWriter(FileName))
			{
				Writer.WriteObjectStart();
				Writer.WriteValue("Version", CurrentVersion);
				Writer.WriteArrayStart("Samples");
				foreach(TelemetrySample Sample in Samples)
				{
					Writer.WriteObjectStart();
					Writer.WriteValue("Name", Sample.Name);
					Writer.WriteValue("Value", Sample.Value);
					Writer.WriteValue("Units", Sample.Units.ToString());
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();
				Writer.WriteObjectEnd();
			}
		}
	}

	/// <summary>
	/// Timer class used for telemetry reporting.
	/// </summary>
	public class TelemetryStopwatch : IDisposable
    {
        string Name;
        DateTime StartTime;
        bool bFinished;

        public TelemetryStopwatch(string Format, params object[] Args)
        {
            Name = String.Format(Format, Args);
            StartTime = DateTime.Now;
        }

        public void Cancel()
        {
            bFinished = true;
        }

        /// <summary>
        /// Flushes the time to <see cref="CmdEnv.CSVFile"/> if we are the build machine and that environment variable is specified.
        /// Call Finish manually with an alternate name to use that one instead. Useful for dynamically generated names that you can't specify at construction.
        /// </summary>
        /// <param name="AlternateName">Used in place of the Name specified during construction.</param>
        public void Finish(string AlternateName = null)
        {
            if(!bFinished)
            {
                if (!String.IsNullOrEmpty(AlternateName))
                {
                    Name = AlternateName;
                }

                var OutputStr = String.Format("UAT,{0},{1},{2}" + Environment.NewLine, Name, StartTime, DateTime.Now);
                CommandUtils.LogVerbose(OutputStr);
                if (CommandUtils.IsBuildMachine && !String.IsNullOrEmpty(CommandUtils.CmdEnv.CSVFile) && CommandUtils.CmdEnv.CSVFile != "nul")
                {
                    try
                    {
                        File.AppendAllText(CommandUtils.CmdEnv.CSVFile, OutputStr);
                    }
                    catch (Exception Ex)
                    {
                        CommandUtils.LogWarning("Could not append to csv file ({0}) : {1}", CommandUtils.CmdEnv.CSVFile, Ex.ToString());
                    }
                }
            }
            bFinished = true;
        }

        public void Dispose()
        {
            Finish();
        }
    }

    /// <summary>
    /// Stopwatch that uses DateTime.UtcNow for timing. Not hi-res, but also not subject to short time limitations of System.Diagnostics.Stopwatch.
    /// </summary>
    public class DateTimeStopwatch
    {
        public static DateTimeStopwatch Start()
        {
            return new DateTimeStopwatch();
        }

        /// <summary>
        /// Hide public ctor.
        /// </summary>
        private DateTimeStopwatch() { }

        readonly DateTime StartTime = DateTime.UtcNow;

        public TimeSpan ElapsedTime { get { return DateTime.UtcNow - StartTime; } }
    }

    /// <summary>
	/// Use with "using" syntax to push and pop directories in a convenient, exception-safe way
	/// </summary>
	public class PushedDirectory : IDisposable
	{
		public PushedDirectory(string DirectoryName)
		{
			CommandUtils.PushDir(DirectoryName);
		}

		public void Dispose()
		{
			CommandUtils.PopDir();
			GC.SuppressFinalize(this);
		}
	}

	/// <summary>
	/// Use with "using" syntax to temporarily set and environment variable  in a convenient, exception-safe way
	/// </summary>
	public class ScopedEnvVar : IDisposable
	{
		private string StoredEnvVar = null;

		public ScopedEnvVar(string EnvVar, string Value)
		{
			StoredEnvVar = EnvVar;
			CommandUtils.SetEnvVar(StoredEnvVar, Value);
		}

		public void Dispose()
		{
			CommandUtils.SetEnvVar(StoredEnvVar, "");
			GC.SuppressFinalize(this);
		}
	}

    /// <summary>
    /// Helper class to associate a file and its contents
    /// </summary>
    public class EMSFileInfo
    {
        public string FileName { get; set; }
        public byte[] Bytes { get; set; }
    }

    /// <summary>
    /// Wrapper class for the enumerate files JSON response from MCP
    /// </summary>
    [DataContract]
    public sealed class EnumerationResponse
    {
        [DataMember(Name = "doNotCache", IsRequired = true)]
        public Boolean DoNotCache { get; set; }

        [DataMember(Name = "uniqueFilename", IsRequired = true)]
        public string UniqueFilename { get; set; }

        [DataMember(Name = "filename", IsRequired = true)]
        public string Filename { get; set; }

        [DataMember(Name = "hash", IsRequired = true)]
        public string Hash { get; set; }

        [DataMember(Name = "length", IsRequired = true)]
        public long Length { get; set; }

        [DataMember(Name = "uploaded", IsRequired = true)]
        public string Uploaded { get; set; }
    }

	/// <summary>
	/// Code signing
	/// </summary>
	[Help("NoSign", "Skips signing of code/content files.")]
	public class CodeSign
	{
		/// <summary>
		/// If so, what is the signing identity to search for?
		/// </summary>
		public static string SigningIdentity = "Epic Games";

		/// <summary>
		/// Should we use the machine store?
		/// </summary>
		public static bool bUseMachineStoreInsteadOfUserStore = false;

		/// <summary>
		/// How long to keep re-trying code signing for
		/// </summary>
		public static TimeSpan CodeSignTimeOut = new TimeSpan(0, 3, 0); // Keep trying to sign one file for up to 3 minutes

		/// <summary>
		/// Finds the path to SignTool.exe, or throws an exception.
		/// </summary>
		/// <returns>Path to signtool.exe</returns>
		public static string GetSignToolPath()
		{
			string[] PossibleSignToolNames =
			{
				"C:/Program Files (x86)/Windows Kits/8.1/bin/x86/SignTool.exe",
				"C:/Program Files (x86)/Windows Kits/10/bin/x86/SignTool.exe"
			};

			foreach(string PossibleSignToolName in PossibleSignToolNames)
			{
				if(File.Exists(PossibleSignToolName))
				{
					return PossibleSignToolName;
				}
			}

			throw new AutomationException("SignTool not found at '{0}' (are you missing the Windows SDK?)", String.Join("' or '", PossibleSignToolNames));
		}

		/// <summary>
		/// Code signs the specified file
		/// </summary>
		public static void SignSingleExecutableIfEXEOrDLL(string Filename, bool bIgnoreExtension = false)
		{
            if (UnrealBuildTool.Utils.IsRunningOnMono)
            {
                CommandUtils.LogLog(String.Format("Can't sign '{0}', we are running under mono.", Filename));
                return;
            }
            if (!CommandUtils.FileExists(Filename))
			{
				throw new AutomationException("Can't sign '{0}', file does not exist.", Filename);
			}
			// Make sure the file isn't read-only
			FileInfo TargetFileInfo = new FileInfo(Filename);

			// Executable extensions
			List<string> Extensions = new List<string>();
			Extensions.Add(".dll");
			Extensions.Add(".exe");

			bool IsExecutable = bIgnoreExtension;

			foreach (var Ext in Extensions)
			{
				if (TargetFileInfo.FullName.EndsWith(Ext, StringComparison.InvariantCultureIgnoreCase))
				{
					IsExecutable = true;
					break;
				}
			}
			if (!IsExecutable)
			{
				CommandUtils.LogLog(String.Format("Won't sign '{0}', not an executable.", TargetFileInfo.FullName));
				return;
			}

			string SignToolName = GetSignToolPath();

			TargetFileInfo.IsReadOnly = false;

			// Code sign the executable
			string[] TimestampServer = { "http://timestamp.verisign.com/scripts/timestamp.dll",
									     "http://timestamp.globalsign.com/scripts/timstamp.dll",
										 "http://timestamp.comodoca.com/authenticode",
										 "http://www.startssl.com/timestamp"
									   };
			int TimestampServerIndex = 0;

			string SpecificStoreArg = bUseMachineStoreInsteadOfUserStore ? " /sm" : "";

			DateTime StartTime = DateTime.Now;

			int NumTrials = 0;
			for (; ; )
			{
				//@TODO: Verbosity choosing
				//  /v will spew lots of info
				//  /q does nothing on success and minimal output on failure
				string CodeSignArgs = String.Format("sign{0} /a /n \"{1}\" /t {2} /d \"{3}\" /v \"{4}\"", SpecificStoreArg, SigningIdentity, TimestampServer[TimestampServerIndex], TargetFileInfo.Name, TargetFileInfo.FullName);

				IProcessResult Result = CommandUtils.Run(SignToolName, CodeSignArgs, null, CommandUtils.ERunOptions.AllowSpew);
				++NumTrials;

				if (Result.ExitCode != 1)
				{
					if (Result.ExitCode == 2)
					{
						CommandUtils.LogError(String.Format("Signtool returned a warning."));
					}
					// Success!
					break;
				}
				else
				{
					// try another timestamp server on the next iteration
					TimestampServerIndex++;
					if (TimestampServerIndex >= TimestampServer.Count())
					{
						// loop back to the first timestamp server
						TimestampServerIndex = 0;
					}

					// Keep retrying until we run out of time
					TimeSpan RunTime = DateTime.Now - StartTime;
					if (RunTime > CodeSignTimeOut)
					{
						throw new AutomationException("Failed to sign executable '{0}' {1} times over a period of {2}", TargetFileInfo.FullName, NumTrials, RunTime);
					}
				}
			}
		}

		/// <summary>
		/// Code signs the specified file or folder
		/// </summary>
		public static void SignMacFileOrFolder(string InPath, bool bIgnoreExtension = false)
		{
			bool bExists = CommandUtils.FileExists(InPath) || CommandUtils.DirectoryExists(InPath);
			if (!bExists)
			{
				throw new AutomationException("Can't sign '{0}', file or folder does not exist.", InPath);
			}

			// Executable extensions
			List<string> Extensions = new List<string>();
			Extensions.Add(".dylib");
			Extensions.Add(".app");
			Extensions.Add(".framework");

			bool IsExecutable = bIgnoreExtension || (Path.GetExtension(InPath) == "" && !InPath.EndsWith("PkgInfo"));

			foreach (var Ext in Extensions)
			{
				if (InPath.EndsWith(Ext, StringComparison.InvariantCultureIgnoreCase))
				{
					IsExecutable = true;
					break;
				}
			}
			if (!IsExecutable)
			{
				CommandUtils.LogLog(String.Format("Won't sign '{0}', not an executable.", InPath));
				return;
			}

			// Use the old codesigning tool after the upgrade due to segmentation fault on Sierra
			string SignToolName = "/usr/local/bin/codesign_old";
			
			// unless it doesn't exist, then use the Sierra one.
			if(!File.Exists(SignToolName))
			{
				SignToolName = "/usr/bin/codesign";
			}

			string CodeSignArgs = String.Format("-f --deep -s \"{0}\" -v \"{1}\" --no-strict", "Developer ID Application", InPath);

			DateTime StartTime = DateTime.Now;

			int NumTrials = 0;
			for (; ; )
			{
				IProcessResult Result = CommandUtils.Run(SignToolName, CodeSignArgs, null, CommandUtils.ERunOptions.AllowSpew);
				int ExitCode = Result.ExitCode;
				++NumTrials;

				if (ExitCode == 0)
				{
					// Success!
					break;
				}
				else
				{
					// Keep retrying until we run out of time
					TimeSpan RunTime = DateTime.Now - StartTime;
					if (RunTime > CodeSignTimeOut)
					{
						throw new AutomationException("Failed to sign '{0}' {1} times over a period of {2}", InPath, NumTrials, RunTime);
					}
				}
			}
		}

		/// <summary>
		/// Codesigns multiple files, but skips anything that's not an EXE or DLL file
		/// Will automatically skip signing if -NoSign is specified in the command line.
		/// </summary>
		/// <param name="Files">List of files to sign</param>
		public static void SignMultipleIfEXEOrDLL(BuildCommand Command, IEnumerable<string> Files)
		{
			if (!Command.ParseParam("NoSign"))
			{
				CommandUtils.Log("Signing up to {0} files...", Files.Count());
				UnrealBuildTool.UnrealTargetPlatform TargetPlatform = UnrealBuildTool.BuildHostPlatform.Current.Platform;
				if (TargetPlatform == UnrealBuildTool.UnrealTargetPlatform.Mac)
				{
					foreach (var File in Files)
					{
						SignMacFileOrFolder(File);
					}
				}
				else
				{
					List<FileReference> FilesToSign = new List<FileReference>();
					foreach (string File in Files)
					{
						if (!(Path.GetDirectoryName(File).Replace("\\", "/")).Contains("Binaries/XboxOne"))
						{
							FilesToSign.Add(new FileReference(File));
						}						
					}
					SignMultipleFilesIfEXEOrDLL(FilesToSign);
				}
			}
			else
			{
				CommandUtils.LogLog("Skipping signing {0} files due to -nosign.", Files.Count());
			}
		}

		public static void SignListFilesIfEXEOrDLL(string FilesToSign)
		{
			string SignToolName = GetSignToolPath();

			// nothing to sign
			if (String.IsNullOrEmpty(FilesToSign))
			{
				return;
			}

			// Code sign the executable
			string[] TimestampServer = { "http://timestamp.verisign.com/scripts/timestamp.dll",
									     "http://timestamp.globalsign.com/scripts/timstamp.dll",
										 "http://timestamp.comodoca.com/authenticode",
										 "http://www.startssl.com/timestamp"
									   };

			string SpecificStoreArg = bUseMachineStoreInsteadOfUserStore ? " /sm" : "";	
			
			Stopwatch Stopwatch = Stopwatch.StartNew();

			int NumTrials = 0;
			for (; ; )
			{
				//@TODO: Verbosity choosing
				//  /v will spew lots of info
				//  /q does nothing on success and minimal output on failure
				string CodeSignArgs = String.Format("sign{0} /a /n \"{1}\" /t {2} /debug {3}", SpecificStoreArg, SigningIdentity, TimestampServer[NumTrials % TimestampServer.Length], FilesToSign);

				IProcessResult Result = CommandUtils.Run(SignToolName, CodeSignArgs, null, CommandUtils.ERunOptions.AllowSpew);
				++NumTrials;

				if (Result.ExitCode != 1)
				{
					if (Result.ExitCode == 2)
					{
						CommandUtils.LogError(String.Format("Signtool returned a warning."));
					}
					// Success!
					break;
				}
				else
				{
					// Keep retrying until we run out of time
					TimeSpan RunTime = Stopwatch.Elapsed;
					if (RunTime > CodeSignTimeOut && NumTrials >= TimestampServer.Length)
					{
						throw new AutomationException("Failed to sign executables {0} times over a period of {1}", NumTrials, RunTime);
					}
				}
			}
		}

		public static void SignMultipleFilesIfEXEOrDLL(List<FileReference> Files, bool bIgnoreExtension = false)
		{
			if (UnrealBuildTool.Utils.IsRunningOnMono)
			{
				CommandUtils.LogLog(String.Format("Can't sign we are running under mono."));
				return;
			}
			List<string> FinalFiles = new List<string>();
			foreach (string Filename in Files.Select(x => x.FullName))
			{
				// Make sure the file isn't read-only
				FileInfo TargetFileInfo = new FileInfo(Filename);

				// Executable extensions
				List<string> Extensions = new List<string>();
				Extensions.Add(".dll");
				Extensions.Add(".exe");

				bool IsExecutable = bIgnoreExtension;

				foreach (var Ext in Extensions)
				{
					if (TargetFileInfo.FullName.EndsWith(Ext, StringComparison.InvariantCultureIgnoreCase))
					{
						IsExecutable = true;
						break;
					}
				}
				if (IsExecutable && CommandUtils.FileExists(Filename))
				{
					FinalFiles.Add(Filename);
				}
			}			

			StringBuilder FilesToSignBuilder = new StringBuilder();
			List<string> FinalListSignStrings = new List<string>();
			foreach(string File in FinalFiles)
			{
				FilesToSignBuilder.Append("\"" + File + "\" ");				
				if(FilesToSignBuilder.Length > 1900)
				{
					string AddFilesToFinalList = FilesToSignBuilder.ToString();
					FinalListSignStrings.Add(AddFilesToFinalList);
					FilesToSignBuilder.Clear();
				}
			}
			FinalListSignStrings.Add(FilesToSignBuilder.ToString());
			foreach(string FilesToSign in FinalListSignStrings)
			{
				SignListFilesIfEXEOrDLL(FilesToSign);
			}

		}
	}

}
