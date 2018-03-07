// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSyncLauncher
{
	static partial class Program
	{
		/// <summary>
		/// Specifies the path to sync down the stable version of UGS from (eg. //depot/UnrealGameSync/Stable/...). This is a site-specific setting.
		/// </summary>
		static readonly string StableSyncPath;

		/// <summary>
		/// Specifies the path to sync down the unstable version of UGS from (eg. //depot/UnrealGameSync/Unstable). Site-specific setting. May be null.
		/// </summary>
		static readonly string UnstableSyncPath = null;

		[STAThread]
		static int Main(string[] Args)
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			bool bFirstInstance;
			using(Mutex InstanceMutex = new Mutex(true, "UnrealGameSyncRunning", out bFirstInstance))
			{
				if(!bFirstInstance)
				{
					using(EventWaitHandle ActivateEvent = new EventWaitHandle(false, EventResetMode.AutoReset, "ActivateUnrealGameSync"))
					{
						ActivateEvent.Set();
					}
					return 0;
				}

				StringWriter LogWriter = new StringWriter();

				bool bResult = false;
				try
				{
					bResult = SyncAndRunApplication(Args, InstanceMutex, LogWriter);
				}
				catch(Exception Ex)
				{
					LogWriter.WriteLine(Ex.ToString());
				}

				if(!bResult)
				{
					UpdateErrorWindow ErrorWindow = new UpdateErrorWindow(LogWriter.ToString());
					ErrorWindow.ShowDialog();
					return 2;
				}
			}

			return 0;
		}

		static bool SyncAndRunApplication(string[] Args, Mutex InstanceMutex, TextWriter LogWriter)
		{
			// Try to find Perforce in the path
			string PerforceFileName = null;
			foreach(string PathDirectoryName in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(new char[]{ Path.PathSeparator }, StringSplitOptions.RemoveEmptyEntries))
			{
				try
				{
					string PossibleFileName = Path.Combine(PathDirectoryName, "p4.exe");
					if(File.Exists(PossibleFileName))
					{
						PerforceFileName = PossibleFileName;
						break;
					}
				}
				catch { }
			}

			// If it doesn't exist, don't continue
			if(PerforceFileName == null)
			{
				LogWriter.WriteLine("UnrealGameSync requires the Perforce command-line tools. Please download and install from http://www.perforce.com/.");
				return false;
			}

			// Get the path that we're syncing
			bool bUnstable = Args.Contains("-unstable", StringComparer.InvariantCultureIgnoreCase);
			if(!bUnstable && (Control.ModifierKeys & Keys.Shift) != 0 && UnstableSyncPath != null)
			{
				if(MessageBox.Show("Use the latest unstable build of UnrealGameSync?\n\n(This message was triggered by holding down the SHIFT key on startup).", "Use unstable build?", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					bUnstable = true;
				}
			}

			string SyncPath = bUnstable? UnstableSyncPath : StableSyncPath;
			LogWriter.WriteLine("Syncing from {0}", SyncPath);

			// Create the target folder
			string ApplicationFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnrealGameSync", "Latest");
			if(!SafeCreateDirectory(ApplicationFolder))
			{
				LogWriter.WriteLine("Couldn't create directory: {0}", ApplicationFolder);
				return false;
			}

			// Find the most recent changelist
			List<string> SubmittedLines = new List<string>();
			if(RunPerforceCommand(PerforceFileName, "changes -s submitted -m 1", SubmittedLines, LogWriter) != 0)
			{
				LogWriter.WriteLine("Couldn't find last changelist");
				return false;
			}

			// Split into tokens
			string[] ChangeTokens = SubmittedLines[0].Split(' ');

			// Parse the changelist number
			int RequiredChangeNumber;
			if(ChangeTokens.Length < 2 || ChangeTokens[0] != "Change" || !int.TryParse(ChangeTokens[1], out RequiredChangeNumber))
			{
				LogWriter.WriteLine("Couldn't parse last changelist number");
				return false;
			}

			// Read the current version
			string SyncVersionFile = Path.Combine(ApplicationFolder, "SyncVersion.txt");
			string RequiredSyncText = String.Format("{0}@{1}", SyncPath, RequiredChangeNumber.ToString());

			// Check if the version has changed
			string SyncText;
			if(!File.Exists(SyncVersionFile) || !TryReadAllText(SyncVersionFile, out SyncText) || SyncText != RequiredSyncText)
			{
				// Try to delete the directory contents. Retry for a while, in case we've been spawned by an application in this folder to do an update.
				for(int NumRetries = 0; !SafeDeleteDirectoryContents(ApplicationFolder); NumRetries++)
				{
					if(NumRetries > 20)
					{
						LogWriter.WriteLine("Couldn't delete contents of {0} (retried {1} times).", ApplicationFolder, NumRetries);
						return false;
					}
					Thread.Sleep(500);
				}

				// Find all the files in the sync path at this changelist
				List<string> FileLines = new List<string>();
				if(RunPerforceCommand(PerforceFileName, String.Format("-z tag fstat \"{0}@{1}\"", SyncPath, RequiredChangeNumber), FileLines, LogWriter) != 0)
				{
					LogWriter.WriteLine("Couldn't find matching files.");
					return false;
				}

				// Sync all the files in this list to the same directory structure under the application folder
				string DepotPathPrefix = SyncPath.Substring(0, SyncPath.LastIndexOf('/') + 1);
				foreach(string FileLine in FileLines)
				{
					const string DepotPathTag = "... depotFile ";
					if(FileLine.StartsWith(DepotPathTag))
					{
						string DepotPath = FileLine.Substring(DepotPathTag.Length).Trim();
						if(!DepotPath.StartsWith(DepotPathPrefix, StringComparison.InvariantCultureIgnoreCase))
						{
							LogWriter.WriteLine("Found file {0} which did not begin with {1}", DepotPath, DepotPathPrefix);
							return false;
						}

						string LocalPath = Path.Combine(ApplicationFolder, DepotPath.Substring(DepotPathPrefix.Length).Replace('/', Path.DirectorySeparatorChar));
						if(!SafeCreateDirectory(Path.GetDirectoryName(LocalPath)))
						{
							LogWriter.WriteLine("Couldn't create folder {0}", Path.GetDirectoryName(LocalPath));
							return false;
						}
						if(RunPerforceCommand(PerforceFileName, String.Format("print -o \"{0}\" \"{1}@{2}\"", LocalPath, DepotPath, RequiredChangeNumber), null, LogWriter) != 0)
						{
							LogWriter.WriteLine("Couldn't sync {0} to {1}", DepotPath, LocalPath);
							return false;
						}
					}
				}

				// Update the version
				if(!TryWriteAllText(SyncVersionFile, RequiredSyncText))
				{
					LogWriter.WriteLine("Couldn't write sync text to {0}", SyncVersionFile);
					return false;
				}
			}
			LogWriter.WriteLine();

			// Build the command line for the synced application, including the sync path to monitor for updates
			StringBuilder NewCommandLine = new StringBuilder(String.Format("-updatepath=\"{0}@>{1}\" -updatespawn=\"{2}\"{3}", SyncPath, RequiredChangeNumber, Assembly.GetEntryAssembly().Location, bUnstable? " -unstable" : ""));
			foreach(string Arg in Args)
			{
				if(Arg.Contains(' '))
				{
					NewCommandLine.AppendFormat( "\"{0}\"", Arg);
				}
				else
				{
					NewCommandLine.AppendFormat(" {0}", Arg);
				}
			}

			// Release the mutex now so that the new application can start up
			InstanceMutex.Close();

			// Check the application exists
			string ApplicationExe = Path.Combine(ApplicationFolder, "UnrealGameSync.exe");
			if(!File.Exists(ApplicationExe))
			{
				LogWriter.WriteLine("Application was not synced from Perforce. Check you have access to {0}", SyncPath);
				return false;
			}

			// Spawn the application
			LogWriter.WriteLine("Spawning {0} with command line: {1}", ApplicationExe, NewCommandLine.ToString());
			using(Process ChildProcess = new Process())
			{
				ChildProcess.StartInfo.FileName = ApplicationExe;
				ChildProcess.StartInfo.Arguments = NewCommandLine.ToString();
				ChildProcess.StartInfo.UseShellExecute = false;
				ChildProcess.StartInfo.CreateNoWindow = false;
				if(!ChildProcess.Start())
				{
					LogWriter.WriteLine("Failed to start process");
					return false;
				}
			}

			return true;
		}

		static int RunPerforceCommand(string PerforceFileName, string CommandLine, List<string> Lines, TextWriter LogWriter)
		{
			LogWriter.WriteLine();
			LogWriter.WriteLine("Running p4.exe {0}", CommandLine);
			using(Process ChildProcess = new Process())
			{
				DataReceivedEventHandler OutputHandler = (x, y) => HandlePerforceOutput(x, y, Lines, LogWriter);
				
				ChildProcess.StartInfo.FileName = PerforceFileName;
				ChildProcess.StartInfo.Arguments = CommandLine;
				ChildProcess.StartInfo.UseShellExecute = false;
				ChildProcess.StartInfo.RedirectStandardOutput = true;
				ChildProcess.StartInfo.RedirectStandardError = true;
				ChildProcess.OutputDataReceived += OutputHandler;
				ChildProcess.ErrorDataReceived += OutputHandler;
				ChildProcess.StartInfo.CreateNoWindow = true;
				ChildProcess.StartInfo.StandardOutputEncoding = new System.Text.UTF8Encoding(false, false);
				ChildProcess.Start();
				ChildProcess.BeginOutputReadLine();
				ChildProcess.BeginErrorReadLine();
				ChildProcess.WaitForExit();

				LogWriter.WriteLine("Finished with exit code {0}", ChildProcess.ExitCode);
				return ChildProcess.ExitCode;
			}
		}

		static void HandlePerforceOutput(object Sender, DataReceivedEventArgs Args, List<string> Lines, TextWriter LogWriter)
		{
			if(Args.Data != null)
			{
				lock(LogWriter)
				{
					if(Lines != null)
					{
						Lines.Add(Args.Data);
					}
					LogWriter.WriteLine("p4> {0}", Args.Data);
				}
			}
		}

		static bool TryReadAllText(string FileName, out string Text)
		{
			try
			{
				Text = File.ReadAllText(FileName);
				return true;
			}
			catch(Exception)
			{
				Text = null;
				return false;
			}
		}

		static bool TryWriteAllText(string FileName, string Text)
		{
			try
			{
				File.WriteAllText(FileName, Text);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeCreateDirectory(string DirectoryName)
		{
			try
			{
				Directory.CreateDirectory(DirectoryName);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeDeleteDirectory(string DirectoryName)
		{
			try
			{
				Directory.Delete(DirectoryName, true);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeDeleteDirectoryContents(string DirectoryName)
		{
			try
			{
				DirectoryInfo Directory = new DirectoryInfo(DirectoryName);
				foreach(FileInfo ChildFile in Directory.EnumerateFiles("*", SearchOption.AllDirectories))
				{
					ChildFile.Attributes = ChildFile.Attributes & ~FileAttributes.ReadOnly;
					ChildFile.Delete();
				}
				foreach(DirectoryInfo ChildDirectory in Directory.EnumerateDirectories())
				{
					SafeDeleteDirectory(ChildDirectory.FullName);
				}
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}
	}
}
