// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading;
using System.Diagnostics;
using System.Reflection;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;

namespace AutomationTool
{
	#region UAT Internal Utils

	/// <summary>
	/// AutomationTool internal Utilities.
	/// </summary>
	public static class InternalUtils
	{
		/// <summary>
		/// Gets environment variable value.
		/// </summary>
		/// <param name="VarName">Variable name.</param>
		/// <param name="Default">Default value to be returned if the variable does not exist.</param>
		/// <returns>Variable value or the default value if the variable did not exist.</returns>
		public static string GetEnvironmentVariable(string VarName, string Default, bool bQuiet = false)
		{
			var Value = Environment.GetEnvironmentVariable(VarName);
			if (Value == null)
			{
				Value = Default;
			}
			if (!bQuiet)
			{
				Log.TraceLog("GetEnvironmentVariable {0}={1}", VarName, Value);
			}
			return Value;
		}

		/// <summary>
		/// Creates a directory.
		/// @todo: this function should not exchange exception context for error codes that can be ignored.
		/// </summary>
		/// <param name="Path">Directory name.</param>
		/// <returns>True if the directory was created, false otherwise.</returns>
		public static bool SafeCreateDirectory(string Path, bool bQuiet = false)
		{
			if( !bQuiet)
			{
				Log.TraceLog("SafeCreateDirectory {0}", Path);
			}

			bool Result = true;
			try
			{
				Result = Directory.CreateDirectory(Path).Exists;
			}
			catch (Exception)
			{
				if (Directory.Exists(Path) == false)
				{
					Result = false;
				}
			}
			return Result;
		}

		/// <summary>
		/// Deletes a file (will remove read-only flag if necessary).
		/// </summary>
		/// <param name="Path">Filename</param>
		/// <param name="bQuiet">if true, then do not print errors, also in quiet mode do not retry</param>
		/// <returns>True if the file does not exist, false otherwise.</returns>
		public static bool SafeDeleteFile(string Path, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("SafeDeleteFile {0}", Path);
			}
			int MaxAttempts = bQuiet ? 1 : 10;
			int Attempts = 0;
			bool Result = true;
			Exception LastException = null;

			do
			{
				Result = true;
				try
				{
					if (File.Exists(Path))
					{
						FileAttributes Attributes = File.GetAttributes(Path);
						if ((Attributes & FileAttributes.ReadOnly) != 0)
						{
							File.SetAttributes(Path, Attributes & ~FileAttributes.ReadOnly);
						}
						File.Delete(Path);
					}
				}
				catch (Exception Ex)
				{
					if (File.Exists(Path))
					{
						Result = false;
					}
					LastException = Ex;
				}
				if (Result == false && Attempts + 1 < MaxAttempts)
				{
					Thread.Sleep(1000);
				}
			} while (Result == false && ++Attempts < MaxAttempts);

			if (Result == false && LastException != null)
			{
				if (bQuiet)
				{
					Log.TraceLog("Failed to delete file {0} in {1} attempts.", Path, MaxAttempts);
				}
				else
				{
					Log.TraceWarning("Failed to delete file {0} in {1} attempts.", Path, MaxAttempts);
					Log.TraceWarning(LogUtils.FormatException(LastException));
				}
			}

			return Result;
		}

		/// <summary>
		/// Recursively deletes a directory and all its files and subdirectories.
		/// </summary>
		/// <param name="Path">Path to delete.</param>
		/// <returns>Whether the deletion was succesfull.</returns>
		private static bool RecursivelyDeleteDirectory(string Path, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("RecursivelyDeleteDirectory {0}", Path);
			}
			// Delete all files. This will also delete read-only files.
			var FilesInDirectory = Directory.EnumerateFiles(Path);
			foreach (string Filename in FilesInDirectory)
			{
				if (SafeDeleteFile(Filename, bQuiet) == false)
				{
					return false;
				}
			}

			// Recursively delete all files from sub-directories.
			var FoldersInDirectory = Directory.EnumerateDirectories(Path);
			foreach (string Folder in FoldersInDirectory)
			{
				if (RecursivelyDeleteDirectory(Folder, bQuiet) == false)
				{
					return false;
				}
			}

			// At this point there should be no read-only files in any of the directories and
			// this directory should be empty too.
			return SafeDeleteEmptyDirectory(Path, bQuiet);
		}

		/// <summary>
		/// Deletes an empty directory.
		/// </summary>
		/// <param name="Path">Path to the Directory.</param>
		/// <returns>True if deletion was successful, otherwise false.</returns>
		public static bool SafeDeleteEmptyDirectory(string Path, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("SafeDeleteEmptyDirectory {0}", Path);
			}
			const int MaxAttempts = 10;
			int Attempts = 0;
			bool Result = true;
			Exception LastException = null;
			do
			{
				Result = !Directory.Exists(Path);
				if (!Result)
				{
					try
					{
						Directory.Delete(Path, true);
					}
					catch (Exception Ex)
					{
						if (Directory.Exists(Path))
						{
							Thread.Sleep(3000);
						}
						Result = !Directory.Exists(Path);
						LastException = Ex;
					}
				}
			} while (Result == false && ++Attempts < MaxAttempts);

			if (Result == false && LastException != null)
			{
				Log.TraceWarning("Failed to delete directory {0} in {1} attempts.", Path, MaxAttempts);
				Log.TraceWarning(LogUtils.FormatException(LastException));
			}

			return Result;
		}

		/// <summary>
		/// Deletes a directory and all its contents. Will delete read-only files.
		/// </summary>
		/// <param name="Path">Directory name.</param>
		/// <returns>True if the directory no longer exists, false otherwise.</returns>
		public static bool SafeDeleteDirectory(string Path, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("SafeDeleteDirectory {0}", Path);
			}
			if (Directory.Exists(Path))
			{
				return RecursivelyDeleteDirectory(Path, bQuiet);
			}
			else
			{
				return true;
			}
		}

		/// <summary>
		/// Renames/moves a file.
		/// </summary>
		/// <param name="OldName">Old name</param>
		/// <param name="NewName">New name</param>
		/// <returns>True if the operation was successful, false otherwise.</returns>
		public static bool SafeRenameFile(string OldName, string NewName, bool bQuiet = false)
		{
			if( !bQuiet )
			{
				Log.TraceLog("SafeRenameFile {0} {1}", OldName, NewName);
			}
			const int MaxAttempts = 10;
			int Attempts = 0;

			bool Result = true;
			do
			{
				Result = true;
				try
				{
					if (File.Exists(OldName))
					{
						FileAttributes Attributes = File.GetAttributes(OldName);
						if ((Attributes & FileAttributes.ReadOnly) != 0)
						{
							File.SetAttributes(OldName, Attributes & ~FileAttributes.ReadOnly);
						}
					}
					File.Move(OldName, NewName);
				}
				catch (Exception Ex)
				{
					if (File.Exists(OldName) == true || File.Exists(NewName) == false)
					{
						Log.TraceWarning("Failed to rename {0} to {1}", OldName, NewName);
						Log.TraceWarning(LogUtils.FormatException(Ex));
						Result = false;
					}
				}
			}
			while (Result == false && ++Attempts < MaxAttempts);

			return Result;
		}

		// @todo: This could be passed in from elsewhere, and this should be somehow done per ini section
		// but this will get it so that games won't ship passwords
		private static string[] LinesToFilter = new string[]
		{
			"KeyStorePassword",
			"KeyPassword",
			"rsa.privateexp",
			"rsa.modulus",
			"rsa.publicexp",
			"aes.key",
		};

		private static void FilterIniFile(string SourceName, string TargetName)
		{
			string[] Lines = File.ReadAllLines(SourceName);
			StringBuilder NewLines = new StringBuilder("");

			foreach (string Line in Lines)
			{
				// look for each filter on each line
				bool bFiltered = false;
				foreach (string Filter in LinesToFilter)
				{
					if (Line.StartsWith(Filter + "="))
					{
						bFiltered = true;
						break;
					}
				}

				// write out if it's not filtered out
				if (!bFiltered)
				{
					NewLines.AppendLine(Line);
				}
			}

			// now write out the final .ini file
			if (File.Exists(TargetName))
			{
				File.Delete(TargetName);
			}
			File.WriteAllText(TargetName, NewLines.ToString());

			// other code assumes same timestamp for source and dest
			File.SetLastWriteTimeUtc(TargetName, File.GetLastWriteTimeUtc(SourceName));
		}

		/// <summary>
		/// Copies a file.
		/// </summary>
		/// <param name="SourceName">Source name</param>
		/// <param name="TargetName">Target name</param>
		/// <returns>True if the operation was successful, false otherwise.</returns>
		public static bool SafeCopyFile(string SourceName, string TargetName, bool bQuiet = false, bool bFilterSpecialLinesFromIniFiles = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("SafeCopyFile {0} {1}", SourceName, TargetName);
			}
			const int MaxAttempts = 10;
			int Attempts = 0;

			bool Result = true;
			do
			{
				Result = true;
				bool Retry = true;
				try
				{
					bool bSkipSizeCheck = false;
					if (bFilterSpecialLinesFromIniFiles && Path.GetExtension(SourceName) == ".ini")
					{
						FilterIniFile(SourceName, TargetName);
						// ini files may change size, don't check
						bSkipSizeCheck = true;
					}
					else
					{
						File.Copy(SourceName, TargetName, overwrite: true);
					}
					Retry = !File.Exists(TargetName);
					if (!Retry)
					{
						FileInfo SourceInfo = new FileInfo(SourceName);
						FileInfo TargetInfo = new FileInfo(TargetName);
						if (!bSkipSizeCheck && SourceInfo.Length != TargetInfo.Length)
						{
							Log.TraceWarning("Size mismatch {0} = {1} to {2} = {3}", SourceName, SourceInfo.Length, TargetName, TargetInfo.Length);
							Retry = true;
						}
						// Timestamps should be no more than 2 seconds out - assuming this as exFAT filesystems store timestamps at 2 second intervals:
						// http://ntfs.com/exfat-time-stamp.htm
						if (!((SourceInfo.LastWriteTimeUtc - TargetInfo.LastWriteTimeUtc).TotalSeconds < 2 && (SourceInfo.LastWriteTimeUtc - TargetInfo.LastWriteTimeUtc).TotalSeconds > -2))
						{
							Log.TraceWarning("Date mismatch {0} = {1} to {2} = {3}", SourceName, SourceInfo.LastWriteTimeUtc, TargetName, TargetInfo.LastWriteTimeUtc);
							Retry = true;
						}
					}
				}
				catch (Exception Ex)
				{
					Log.TraceWarning("SafeCopyFile Exception was {0}", LogUtils.FormatException(Ex));
					Retry = true;
				}

				if (Retry)
				{
					if (Attempts + 1 < MaxAttempts)
					{
						Log.TraceWarning("Failed to copy {0} to {1}, deleting, waiting 10s and retrying.", SourceName, TargetName);
						if (File.Exists(TargetName))
						{
							SafeDeleteFile(TargetName);
						}
						Thread.Sleep(10000);
					}
					else
					{
						Log.TraceWarning("Failed to copy {0} to {1}", SourceName, TargetName);
					}
					Result = false;
				}
			}
			while (Result == false && ++Attempts < MaxAttempts);

			return Result;
		}


		/// <summary>
		/// Reads all lines from a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <returns>An array containing all lines read from the file or null if the file could not be read.</returns>
		public static string[] SafeReadAllLines(string Filename)
		{
			Log.TraceLog("SafeReadAllLines {0}", Filename);
			string[] Result = null;
			try
			{
				Result = File.ReadAllLines(Filename);
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Failed to load {0}", Filename);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Result;
		}

		/// <summary>
		/// Reads all text from a file.
		/// </summary>
		/// <param name="Filename">Filename</param>
		/// <returns>String containing all text read from the file or null if the file could not be read.</returns>
		public static string SafeReadAllText(string Filename)
		{
			Log.TraceLog("SafeReadAllLines {0}", Filename);
			string Result = null;
			try
			{
				Result = File.ReadAllText(Filename);
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Failed to load {0}", Filename);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Result;
		}

		/// <summary>
		/// Finds files in the specified path.
		/// </summary>
		/// <param name="Path">Path</param>
		/// <param name="SearchPattern">Search pattern</param>
		/// <param name="Recursive">Whether to search recursively or not.</param>
		/// <returns>List of all files found (can be empty) or null if the operation failed.</returns>
		public static string[] FindFiles(string Path, string SearchPattern, bool Recursive, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("FindFiles {0} {1} {2}", Path, SearchPattern, Recursive);
			}

			// On Linux, filter out symlinks since we (usually) create them to fix mispelled case-sensitive filenames in content, and if they aren't filtered, 
			// UAT picks up both the symlink and the original file and considers them duplicates when packaging (pak files are case-insensitive).
			// Windows needs the symlinks though because that's how deduplication works on Windows server, 
			// see https://answers.unrealengine.com/questions/212888/automated-buildjenkins-failing-due-to-symlink-chec.html
			// FIXME: ZFS, JFS and other fs that can be case-insensitive on Linux should use the faster path as well.
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Linux)
			{
				return Directory.GetFiles(Path, SearchPattern, Recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly);
			}
			else
			{
				List<string> FileNames = new List<string>();
				DirectoryInfo DirInfo = new DirectoryInfo(Path);
				foreach( FileInfo File in DirInfo.EnumerateFiles(SearchPattern, Recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly))
				{
					if (File.Attributes.HasFlag(FileAttributes.ReparsePoint))
					{
						if (!bQuiet)
						{
							Log.TraceWarning("Ignoring symlink {0}", File.FullName);
						}
						continue;
					}
					
					FileNames.Add(File.FullName);
				}
				
				return FileNames.ToArray();
			}
		}

		/// <summary>
		/// Finds directories in the specified path.
		/// </summary>
		/// <param name="Path">Path</param>
		/// <param name="SearchPattern">Search pattern</param>
		/// <param name="Recursive">Whether to search recursively or not.</param>
		/// <returns>List of all directories found (can be empty) or null if the operation failed.</returns>
		public static string[] FindDirectories(string Path, string SearchPattern, bool Recursive, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("FindDirectories {0} {1} {2}", Path, SearchPattern, Recursive);
			}
			return Directory.GetDirectories(Path, SearchPattern, Recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly);
		}

		/// <summary>
		/// Finds files in the specified path.
		/// </summary>
		/// <param name="Path">Path</param>
		/// <param name="SearchPattern">Search pattern</param>
		/// <param name="Recursive">Whether to search recursively or not.</param>
		/// <returns>List of all files found (can be empty) or null if the operation failed.</returns>
		public static string[] SafeFindFiles(string Path, string SearchPattern, bool Recursive, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("SafeFindFiles {0} {1} {2}", Path, SearchPattern, Recursive);
			}
			string[] Files = null;
			try
			{
				Files = FindFiles(Path, SearchPattern, Recursive, bQuiet);
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Unable to Find Files in {0}", Path);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Files;
		}

		/// <summary>
		/// Finds directories in the specified path.
		/// </summary>
		/// <param name="Path">Path</param>
		/// <param name="SearchPattern">Search pattern</param>
		/// <param name="Recursive">Whether to search recursively or not.</param>
		/// <returns>List of all files found (can be empty) or null if the operation failed.</returns>
		public static string[] SafeFindDirectories(string Path, string SearchPattern, bool Recursive, bool bQuiet = false)
		{
			if (!bQuiet)
			{
				Log.TraceLog("SafeFindDirectories {0} {1} {2}", Path, SearchPattern, Recursive);
			}
			string[] Directories = null;
			try
			{
				Directories = FindDirectories(Path, SearchPattern, Recursive, bQuiet);
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Unable to Find Directories in {0}", Path);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Directories;
		}

		/// <summary>
		/// Checks if a file exists.
		/// </summary>
		/// <param name="Path">Filename</param>
		/// <param name="bQuiet">if true, do not print a message</param>
		/// <returns>True if the file exists, false otherwise.</returns>
		public static bool SafeFileExists(string Path, bool bQuiet = false)
		{
			bool Result = false;
			try
			{
				Result = File.Exists(Path);
				if (!bQuiet)
				{
					Log.TraceLog("SafeFileExists {0}={1}", Path, Result);
				}
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Unable to check if file {0} exists.", Path);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Result;
		}


		/// <summary>
		/// Checks if a directory exists.
		/// </summary>
		/// <param name="Path">Directory</param>
		/// <param name="bQuiet">if true, no longging</param>
		/// <returns>True if the directory exists, false otherwise.</returns>
		public static bool SafeDirectoryExists(string Path, bool bQuiet = false)
		{
			bool Result = false;
			try
			{
				Result = Directory.Exists(Path);
				if (!bQuiet)
				{
					Log.TraceLog("SafeDirectoryExists {0}={1}", Path, Result);
				}
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Unable to check if directory {0} exists.", Path);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Result;
		}

		/// <summary>
		/// Writes lines to a file.
		/// </summary>
		/// <param name="Path">Filename</param>
		/// <param name="Text">Text</param>
		/// <returns>True if the operation was successful, false otherwise.</returns>
		public static bool SafeWriteAllLines(string Path, string[] Text)
		{
			Log.TraceLog("SafeWriteAllLines {0}", Path);
			bool Result = false;
			try
			{
				File.WriteAllLines(Path, Text);
				Result = true;
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Unable to write text to {0}", Path);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Result;
		}

		/// <summary>
		/// Writes text to a file.
		/// </summary>
		/// <param name="Path">Filename</param>
		/// <param name="Text">Text</param>
		/// <returns>True if the operation was successful, false otherwise.</returns>
		public static bool SafeWriteAllText(string Path, string Text)
		{
			Log.TraceLog("SafeWriteAllText {0}", Path);
			bool Result = false;
			try
			{
				File.WriteAllText(Path, Text);
				Result = true;
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Unable to write text to {0}", Path);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Result;
		}

		/// <summary>
		/// Writes text to a file.
		/// </summary>
		/// <param name="Path">Filename</param>
		/// <param name="Text">Text</param>
		/// <returns>True if the operation was successful, false otherwise.</returns>
		public static bool SafeWriteAllBytes(string Path, byte[] Bytes)
		{
			Log.TraceLog("SafeWriteAllBytes {0}", Path);
			bool Result = false;
			try
			{
				File.WriteAllBytes(Path, Bytes);
				Result = true;
			}
			catch (Exception Ex)
			{
				Log.TraceWarning("Unable to write text to {0}", Path);
				Log.TraceWarning(LogUtils.FormatException(Ex));
			}
			return Result;
		}

		/// <summary>
		/// Runs the specified delegate checking if this is the only instance of the application.
		/// </summary>
		/// <param name="Main"></param>
		/// <param name="Param"></param>
		public static ExitCode RunSingleInstance(Func<object, ExitCode> Main, object Param)
		{
			if (Environment.GetEnvironmentVariable("uebp_UATMutexNoWait") == "1")
			{
				return Main(Param);
			}
			var bCreatedMutex = false;
            var EntryAssemblyLocation = Assembly.GetEntryAssembly().GetOriginalLocation();
			var LocationHash = EntryAssemblyLocation.GetHashCode();
            var MutexName = "Global/" + Path.GetFileNameWithoutExtension(EntryAssemblyLocation) + "_" + LocationHash.ToString() + "_Mutex";
			using (Mutex SingleInstanceMutex = new Mutex(true, MutexName, out bCreatedMutex))
			{
				if (!bCreatedMutex)
				{
                    throw new AutomationException("A conflicting instance of AutomationTool is already running. Curent location: {0}. A process manager may be used to determine the conflicting process and what tool may have launched it", EntryAssemblyLocation);
				}

				ExitCode Result = Main(Param);

				SingleInstanceMutex.ReleaseMutex();

				return Result;
			}
		}

	    public static void Robust_CopyFile(string InputFileName, string OutputFileName)
	    {
	        if (OutputFileName.StartsWith("/Volumes/"))
	        {
	            int Retry = 0;
	            int NumRetries = 60;
	            bool bCopied = false;
	            while (!bCopied && Retry < NumRetries)
	            {
	                if (Retry > 0)
	                {
                        //@todo: These retries should be reported so we can track how often they are occurring.
                        CommandUtils.Log("*** Mac temp storage retry {0}", OutputFileName);
	                    System.Threading.Thread.Sleep(1000);
	                }
	                bCopied = CommandUtils.CopyFile_NoExceptions(InputFileName, OutputFileName, true);
	                Retry++;
	            }
	        }
	        else
	        {
	            CommandUtils.CopyFile(InputFileName, OutputFileName);
	        }
	    }

	    public static void Robust_FileExists(string Filename, string Message)
	    {
	        Robust_FileExists(false, Filename, Message);
	    }

	    public static void Robust_FileExists(bool bQuiet, string Filename, string Message)
	    {
	        if (!CommandUtils.FileExists_NoExceptions(bQuiet, Filename))
	        {
	            bool bFound = false;
	            // mac is terrible on shares, this isn't a solution, but a stop gap
	            if (Filename.StartsWith("/Volumes/"))
	            {
	                int Retry = 0;
	                while (!bFound && Retry < 60)
	                {
                        //@todo: These retries should be reported so we can track how often they are occurring.
                        CommandUtils.Log("*** Mac temp storage retry {0}", Filename);
	                    System.Threading.Thread.Sleep(10000);
	                    bFound = CommandUtils.FileExists_NoExceptions(bQuiet, Filename);
	                    Retry++;
	                }
	            }
	            if (!bFound)
	            {
	                throw new AutomationException(Message, Filename);
	            }
	        }
	    }

	    public static bool Robust_DirectoryExists_NoExceptions(string Directoryname, string Message)
	    {
	        bool bFound = false;
	        if (!CommandUtils.DirectoryExists_NoExceptions(Directoryname))
	        {				
	            // mac is terrible on shares, this isn't a solution, but a stop gap
	            if (Directoryname.StartsWith("/Volumes/"))
	            {
	                int Retry = 0;
	                while (!bFound && Retry < 60)
	                {
                        //@todo: These retries should be reported so we can track how often they are occurring.
                        CommandUtils.Log("*** Mac temp storage retry {0}", Directoryname);
	                    System.Threading.Thread.Sleep(10000);
	                    bFound = CommandUtils.DirectoryExists_NoExceptions(Directoryname);
	                    Retry++;
	                }
	            }				
	        }
	        else
	        {
	            bFound = true;
	        }
	        return bFound;
	    }

	    public static bool Robust_DirectoryExistsAndIsWritable_NoExceptions(string Directoryname)
	    {
	        bool bFound = false;
	        if (!CommandUtils.DirectoryExistsAndIsWritable_NoExceptions(Directoryname))
	        {
	            // mac is terrible on shares, this isn't a solution, but a stop gap
	            if (Directoryname.StartsWith("/Volumes/"))
	            {
	                int Retry = 0;
	                int NumRetries = 60;
	                if(!Directoryname.Contains("UE4"))
	                {
	                    NumRetries = 2;
	                }
	                while (!bFound && Retry < NumRetries)
	                {
                        //@todo: These retries should be reported so we can track how often they are occurring.
                        CommandUtils.Log("*** Mac temp storage retry {0}", Directoryname);
	                    System.Threading.Thread.Sleep(1000);
	                    bFound = CommandUtils.DirectoryExistsAndIsWritable_NoExceptions(Directoryname);
	                    Retry++;
	                }
	            }
	        }
	        else
	        {
	            bFound = true;
	        }
	        return bFound;
	    }

		/// <summary>
		/// Efficient iterator for walking over a string line by line.
		/// </summary>
		/// <param name="Str">string to walk over</param>
		/// <returns>enumerable of each line in the string.</returns>
		public static IEnumerable<string> EnumerateLines(this string Str)
		{
			if (Str == null)
			{
				yield break;
			}

			using (var Reader = new StringReader(Str))
			{
				string line = Reader.ReadLine();
				while (line != null)
				{
					yield return line;
					line = Reader.ReadLine();
				}
			}
		}

	}

	#endregion


	#region VersionFileReader


	/// <summary>
	/// This is to ensure that UAT can produce version strings precisely compatible
	/// with FEngineVersion.
	/// 
	/// WARNING: If FEngineVersion compatibility changes, this code needs to be updated.
	/// </summary>
	public class FEngineVersionSupport
    {
        /// <summary>
        /// The version info read from the Version header. The populated fields will be Major, Minor, and Build from the MAJOR, MINOR, and PATCH lines, respectively.
        /// Expects lines like:
        /// #define APP_MAJOR_VERSION 0
        /// #define APP_MINOR_VERSION 0
        /// #define APP_PATCH_VERSION 0
        /// </summary>
        public readonly Version Version;

        /// <summary>
        /// The changelist this version is associated with
        /// </summary>
        public readonly int Changelist;

        /// <summary>
        /// The Branch name associated with the version
        /// </summary>
        public readonly string BranchName;

        /// <summary>
        /// Reads a Version.h file, looking for macros that define the MAJOR/MINOR/PATCH version fields. Expected to match the Version.h in the engine.
        /// </summary>
        /// <param name="Filename">Version.h file to read.</param>
        /// <returns>Version that puts the Major/Minor/Patch fields in the Major/Minor/Build fields, respectively.</returns>
        public static Version ReadVersionFromFile(string Filename)
        {
            var regex = new Regex(@"#define.+_(?<Type>MAJOR|MINOR|PATCH)_VERSION\s+(?<Value>.+)", RegexOptions.ExplicitCapture | RegexOptions.IgnoreCase);
            var foundElements = new Dictionary<string, int>(3);
            foreach (var line in File.ReadLines(Filename))
            {
                try
                {
                    var match = regex.Match(line);
                    if (match.Success)
                    {
                        foundElements.Add(match.Groups["Type"].Value, int.Parse(match.Groups["Value"].Value));
                    }
                }
                catch (Exception ex)
                {
                    throw new AutomationException(ex, "Failed to parse line {0} in version file {1}", line, Filename);
                }
            }

            // must find all three parts to accept the version file.
            if (foundElements.Keys.Intersect(new[] { "MAJOR", "MINOR", "PATCH" }).Count() != 3)
            {
                throw new AutomationException("Failed to find MAJOR, MINOR, and PATCH fields from version file {0}", Filename);
            }
			CommandUtils.Log("Read {0}.{1}.{2} from {3}", foundElements["MAJOR"], foundElements["MINOR"], foundElements["PATCH"], Filename);
            return new Version(foundElements["MAJOR"], foundElements["MINOR"], foundElements["PATCH"]);
        }

        /// <summary>
        /// Ctor that takes a pre-determined Version. Gets the Changelist and BranchName from the current <see cref="CommandUtils.P4Env"/>.
        /// </summary>
		/// <param name="InVersion">Predetermined version.</param>
		/// <param name="InChangelist">Predetermined changelist (optional)</param>
		/// <param name="InBranchName">Predetermined branch name (optional)</param>
		public FEngineVersionSupport(Version InVersion, int InChangelist = -1, string InBranchName = null)
        {
			Version = InVersion;
			if (InChangelist <= 0)
			{
				Changelist = CommandUtils.P4Enabled ? CommandUtils.P4Env.Changelist : 0;
			}
			else
			{
				Changelist = InChangelist;
			}
			if (String.IsNullOrEmpty(InBranchName))
			{
				BranchName = CommandUtils.P4Enabled ? CommandUtils.EscapePath(CommandUtils.P4Env.Branch) : "UnknownBranch";
			}
			else
			{
				BranchName = InBranchName;
			}
        }

        /// <summary>
        /// Gets a version string compatible with FEngineVersion's native parsing code.
        /// </summary>
        /// <remarks>
        /// The format looks like: Major.Minor.Build-Changelist+BranchName.
        /// </remarks>
        /// <returns></returns>
        public override string ToString()
        {
			return String.Format("{0}.{1}.{2}-{3}+{4}",
				Version.Major,
				Version.Minor,
				Version.Build,
                Changelist.ToString("0000000"),
                BranchName);
        }

		public override bool Equals(object obj)
		{
			var that = obj as FEngineVersionSupport;
			if (that == null)
				return false;
			return this.ToString() == that.ToString();
		}

		public override int GetHashCode()
		{
			return this.ToString().GetHashCode();
		}

        /// <summary>
        /// Ctor initializes with the values from the supplied Version file. The BranchName and CL are also taken from the current <see cref="CommandUtils.P4Env"/>.
        /// </summary>
        /// <param name="Filename">Full path to the file with the version info.</param>
		/// <param name="InChangelist">Predetermined changelist (optional)</param>
		/// <param name="InBranchName">Predetermined branch name (optional)</param>
        public static FEngineVersionSupport FromVersionFile(string Filename, int InChangelist = -1, string InBranchName = null)
        {
			return new FEngineVersionSupport(ReadVersionFromFile(Filename), InChangelist, InBranchName);
        }

        /// <summary>
        /// Creates a <see cref="FEngineVersionSupport"/> from a string that matches the format given in <see cref="ToString"/>.
        /// </summary>
        /// <param name="versionString">Version string that should match the FEngineVersion::ToString() format.</param>
		/// <param name="bAllowVersion">Optional parameter which if set to true, allows version strings with no version number specified.</param>
        /// <returns>a new instance with fields initialized to the match those given in the string.</returns>
        public static FEngineVersionSupport FromString(string versionString, bool bAllowNoVersion = false)
        {
            try
            {
				if (bAllowNoVersion && versionString.StartsWith("++"))
				{
					// This form of version is used when a product has no major.minor.patch version
					// E.g. ++depot+UE4-ProdName-CL-12345678
					var clSplit = versionString.Split(new string[] { "-CL-" }, 2, StringSplitOptions.None);
					var dashSplit = clSplit[1].Split(new[] { '-' }, 2);
					var changelist = int.Parse(dashSplit[0]);
					var branchName = clSplit[0];
					return new FEngineVersionSupport(new Version(0, 0, 0), changelist, branchName);
				}
				else
				{
					// This is the standard Launcher versioning scheme, e.g. "4.5.0-12345678+++depot+UE4"
					var dotSplit = versionString.Split(new[] { '.' }, 3);
					var dashSplit = dotSplit[2].Split(new[] { '-' }, 2);
					var plusSplit = dashSplit[1].Split(new[] { '+' }, 2);
					var major = int.Parse(dotSplit[0]);
					var minor = int.Parse(dotSplit[1]);
					var patch = int.Parse(dashSplit[0]);
					var changelist = int.Parse(plusSplit[0]);
					var branchName = plusSplit[1];
					return new FEngineVersionSupport(new Version(major, minor, patch), changelist, branchName);
				}
            }
            catch (Exception ex)
            {
                throw new AutomationException(ex, "Failed to parse {0} as an FEngineVersion compatible string", versionString);
            }
        }
    }

    #endregion

    #region VersionFileUpdater

    /// <summary>
	/// VersionFileUpdater.
	/// </summary>
	public class VersionFileUpdater
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public VersionFileUpdater(FileReference Filename)
		{
			MyFile = new FileInfo(Filename.FullName);
			OriginalLines = new List<string>(InternalUtils.SafeReadAllLines(Filename.FullName));
			Lines = new List<string>(OriginalLines);

            if (CommandUtils.IsNullOrEmpty(Lines))
            {
                throw new AutomationException("Version file {0} was empty or not found!", Filename);
            }
		}

		/// <summary>
		/// Finds the line containing the same tokens as the given prefix, and change the suffix to be the given string
		/// </summary>
		/// <param name="Prefix">Prefix to search for</param>
		/// <param name="Suffix">Suffix to replace the rest of the line with</param>
		/// <param name="OccurenceIdx">Index of the matching occurrence to replace</param>
		public void ReplaceLine(string Prefix, string Suffix, int OccurenceIdx = 0)
		{
			int MatchIdx = 0;
			for(int LineIdx = 0; LineIdx < Lines.Count; LineIdx++)
			{
				int LineOffset = 0;
				if(MatchCppTokens(Lines[LineIdx], ref LineOffset, Prefix))
				{
					if(OccurenceIdx == MatchIdx)
					{
						while(LineOffset < Lines[LineIdx].Length && Char.IsWhiteSpace(Lines[LineIdx][LineOffset]))
						{
							LineOffset++;
						}
						Lines[LineIdx] = Lines[LineIdx].Substring(0, LineOffset) + Suffix;
						return;
					}
					MatchIdx++;
				}
			}
            throw new AutomationException("Unable to find line {0} in {1}", Prefix, MyFile.FullName);
		}

		/// <summary>
		/// Tries to read a sequence of tokens from the given line of text, updating a position within it.
		/// </summary>
		/// <param name="Line">Line to read tokens from</param>
		/// <param name="LineOffset">Position within </param>
		/// <param name="Other">The tokens to attempt to match</param>
		/// <returns>True if the tokens are matched</returns>
		static bool MatchCppTokens(string Line, ref int LineOffset, string Other)
		{
			int OtherOffset = 0;
			for(;;)
			{
				// Read the next token from the prefix text. If we've reached the end, return that we've found a match.
				string OtherToken = ReadCppToken(Other, ref OtherOffset);
				if(OtherToken == null)
				{
					return true;
				}

				// Otherwise read the next token from the line and check it matches
				string LineToken = ReadCppToken(Line, ref LineOffset);
				if(LineToken != OtherToken)
				{
					return false;
				}
			}
		}

		/// <summary>
		/// Reads a single token (using a very rough approximation of a C++ token) from the given line
		/// </summary>
		/// <param name="Line">The line to read from</param>
		/// <param name="Offset">Position within the line to start reading. Initial whitespace will be skipped.</param>
		/// <returns>The token that was read, or null if the end of the string</returns>
		static string ReadCppToken(string Line, ref int Offset)
		{
			// Skip any leading whitespace
			while(Offset < Line.Length && Char.IsWhiteSpace(Line[Offset]))
			{
				Offset++;
			}
			if(Offset == Line.Length)
			{
				return null;
			}

			// Find the length of the token
			int StartOffset = Offset++;
			if(Char.IsLetterOrDigit(Line[StartOffset]))
			{
				while(Offset < Line.Length)
				{
					char Character = Line[Offset];
					if(!Char.IsLetterOrDigit(Character) && Character != '_')
					{
						break;
					}
					Offset++;
				}
			}
			return Line.Substring(StartOffset, Offset - StartOffset);
		}

		/// <summary>
		/// Doc
		/// </summary>
        public void SetAssemblyInformationalVersion(string NewInformationalVersion)
		{
            // This searches for the AssemblyInformationalVersion string. Most the mess is to allow whitespace in places that are possible.
            // Captures the string into a group called "Ver" for replacement.
            var regex = new Regex(@"\[assembly:\s+AssemblyInformationalVersion\s*\(\s*""(?<Ver>.*)""\s*\)\s*]", RegexOptions.IgnoreCase | RegexOptions.ExplicitCapture);
            foreach (var Index in Enumerable.Range(0, Lines.Count))
            {
                var line = Lines[Index];
                var match = regex.Match(line);
                if (match.Success)
                {
                    var verGroup = match.Groups["Ver"];
                    var sb = new StringBuilder(line);
                    sb.Remove(verGroup.Index, verGroup.Length);
                    sb.Insert(verGroup.Index, NewInformationalVersion);
                    Lines[Index] = sb.ToString();
                    return;
                }
            }
            throw new AutomationException("Failed to find the AssemblyInformationalVersion attribute in {1}", MyFile.FullName);
		}

		/// <summary>
		/// Doc
		/// </summary>
		public void Commit()
		{
			bool bDifferent = Lines.Count != OriginalLines.Count;

			if (!bDifferent)
			{
				for (int i = 0; i < Lines.Count; ++i)
				{
					if (Lines[i] != OriginalLines[i])
					{
						bDifferent = true;
						break;
					}
				}
			}

			if (bDifferent)
			{
				MakeFileWriteable(MyFile.FullName);
				if (!InternalUtils.SafeWriteAllLines(MyFile.FullName, Lines.ToArray()))
				{
					throw new AutomationException("Unable to update version info in {0}", MyFile.FullName);
				}
				OriginalLines = Lines;
			}
		}

		/// <summary>
		/// Makes a version file writeable. If P4 is enabled, syncs it to revision 0 to prevent conflicts with the P4 have table rather than checking it out, since we never intend to 
		/// submit it to Perforce. For existing writeable files, P4V prompts the user to overwrite the next time they sync.
		/// </summary>
		/// <param name="FileName">The file to make writeable</param>
		public static void MakeFileWriteable(string FileName)
		{
			if(CommandUtils.IsReadOnly(FileName))
			{
				if(CommandUtils.P4Enabled)
				{
					CommandUtils.P4.Sync(String.Format("\"{0}#0\"", FileName), false, false);
				}
				if(CommandUtils.FileExists_NoExceptions(FileName) && CommandUtils.IsReadOnly(FileName))
				{
					throw new AutomationException("Cannot write to {0}; file is read-only", FileName);
				}
			}
		}

		/// <summary>
		/// Doc
		/// </summary>
		protected FileInfo MyFile;
		/// <summary>
		/// Doc
		/// </summary>
		protected List<string> Lines;
		/// <summary>
		/// Doc
		/// </summary>
		protected List<string> OriginalLines;
	}

	#endregion
}
