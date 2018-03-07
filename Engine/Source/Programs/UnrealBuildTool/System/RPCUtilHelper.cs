// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Net;
using System.Net.NetworkInformation;
using System.Reflection;
using System.Threading;
using System.Net.Sockets;
using Ionic.Zip;
using DotNETUtilities;

namespace UnrealBuildTool
{
	class RPCUtilHelper
	{
		/// <summary>
		/// The Mac we are compiling on
		/// </summary>
		private static string MacName;

		/// <summary>
		/// A socket per command thread
		/// </summary>
		private static Hashtable CommandThreadSockets = new Hashtable();

		/// <summary>
		/// Time difference between remote and local idea's of UTC time
		/// </summary>
		private static TimeSpan TimeDifferenceFromRemote = new TimeSpan(0);

		/// <summary>
		/// The number of commands the remote side should be able to run at once
		/// </summary>
		private static int MaxRemoteCommandsAllowed = 0;

		private static DateTime RemoteToLocalTime(string RemoteTime)
		{
			try
			{
				// convert string to integer
				int RemoteTimeSinceEpoch = int.Parse(RemoteTime);

				// convert the seconds into TimeSpan
				TimeSpan RemoteSpanSinceEpoch = new TimeSpan(0, 0, RemoteTimeSinceEpoch);

				// put the remote time into local time (Jan 1, 1970 was the Epoch for Mac), and adjust it for time difference between machines
				return new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc) + RemoteSpanSinceEpoch + TimeDifferenceFromRemote;

			}
			catch (Exception)
			{
				// use MinValue for any errors
				return DateTime.MinValue;
			}
		}

		static public RemoteToolChain.RemoteToolChainErrorCode Initialize(string InMacName, bool bFlushBuildDir)
		{
			MacName = InMacName;

			// when not using RPCUtil, we do NOT want to ping the host
			if (!RemoteToolChain.bUseRPCUtil || RPCCommandHelper.PingRemoteHost(MacName))
			{
				try
				{
					if (!RemoteToolChain.bUseRPCUtil)
					{
						// make sure we have SSH setup
						if (RemoteToolChain.ResolvedSSHAuthentication == null)
						{
							Log.TraceError("SSH authentication required a key, but one was not found. Use Editor to setup remote authentication!");
							return RemoteToolChain.RemoteToolChainErrorCode.MissingSSHKey;
						}
						// ask for current time, free memory and num CPUs
						string[] Commands = new string[] 
						{
							"+\"%s\"",
							"sysctl hw.memsize | awk '{print $2}'",
							"sysctl hw.logicalcpu | awk '{print $2}'",
						};
						Hashtable Results = Command("/", "date", string.Join(" && ", Commands), null);
						if ((Int64)Results["ExitCode"] != 0)
						{
							Log.TraceError("Failed to run init commands on {0}. Output = {1}", MacName, Results["CommandOutput"]);
							return RemoteToolChain.RemoteToolChainErrorCode.SSHCommandFailed;
						}

						string[] Lines = ((string)Results["CommandOutput"]).Split("\r\n".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);

						// convert it to a time, with TimeDifferenceFromRemote as 0
						DateTime RemoteTimebase = RemoteToLocalTime(Lines[0]);
						if (RemoteTimebase == DateTime.MinValue)
						{
							throw new BuildException("Failed to parse remote time on " + MacName);
						}

						// calculate the difference
						TimeDifferenceFromRemote = DateTime.UtcNow - RemoteTimebase;

						// now figure out max number of commands to run at once
						//						int PageSize = int.Parse(Lines[1]);
						Int64 AvailableMem = Int64.Parse(Lines[1].Replace(".", ""));// *PageSize;
						int NumProcesses = (int)Math.Max(1, AvailableMem / (RemoteToolChain.MemoryPerCompileMB * 1024 * 1024));

						// now, combine that with actual number of cores
						int NumCores = int.Parse(Lines[2]);
						MaxRemoteCommandsAllowed = Math.Min(NumProcesses, NumCores);

						Console.WriteLine("Remote time is {0}, difference is {1}", RemoteTimebase.ToString(), TimeDifferenceFromRemote.ToString());
					}

					if (bFlushBuildDir)
					{
						Command("/", "rm", "-rf /UE4/Builds/" + Environment.MachineName, null);
					}
				}
				catch (Exception Ex)
				{
					Log.TraceVerbose("SSH Initialize exception {0}", Ex.ToString());
					Log.TraceError("Failed to run init commands on {0}", MacName);
					return RemoteToolChain.RemoteToolChainErrorCode.SSHCommandFailed;
				}
			}
			else
			{
				Log.TraceError("Failed to ping Mac named {0}", MacName);
				return RemoteToolChain.RemoteToolChainErrorCode.ServerNotResponding;
			}

			return RemoteToolChain.RemoteToolChainErrorCode.NoError;
		}

		/// <summary>
		/// Handle a thread ending
		/// </summary>
		public static void OnThreadComplete()
		{
			lock (CommandThreadSockets)
			{
				// close and remove the socket
				Socket ThreadSocket = CommandThreadSockets[Thread.CurrentThread] as Socket;
				if (ThreadSocket != null)
				{
					ThreadSocket.Close();
				}
				CommandThreadSockets.Remove(Thread.CurrentThread);
			}
		}

		private static Socket GetSocket()
		{
			Socket ThreadSocket = null;

			lock (CommandThreadSockets)
			{
				ThreadSocket = CommandThreadSockets[Thread.CurrentThread] as Socket;
				if (ThreadSocket == null)
				{
					try
					{
						ThreadSocket = RPCCommandHelper.ConnectToUnrealRemoteTool(MacName);
					}
					catch (Exception Ex)
					{
						Log.TraceInformation("Failed to connect to UnrealRemoteTool running on {0}.", MacName);
						throw new BuildException(Ex, "Failed to connect to UnrealRemoteTool running on {0}.", MacName);
					}
					CommandThreadSockets[Thread.CurrentThread] = ThreadSocket;
				}
			}

			return ThreadSocket;
		}

		/// <summary>
		/// This function should be used as the ActionHandler delegate method for Actions that
		/// need to run over RPCUtility. It will block until the remote command completes
		/// </summary>
		static public void RPCActionHandler(Action Action, out int ExitCode, out string Output)
		{
			Hashtable Results = RPCUtilHelper.Command(Action.WorkingDirectory, Action.CommandPath, Action.CommandArguments,
				Action.ProducedItems.Count > 0 ? Action.ProducedItems[0].AbsolutePath : null);
			if (Results == null)
			{
				ExitCode = -1;
				Output = null;
				Log.TraceInformation("Command failed to execute! {0} {1}", Action.CommandPath, Action.CommandArguments);
			}
			else
			{
				// capture the exit code
				if (Results["ExitCode"] != null)
				{
					ExitCode = (int)(Int64)Results["ExitCode"];
				}
				else
				{
					ExitCode = 0;
				}

				// pass back the string
				Output = Results["CommandOutput"] as string;
			}
		}

		/// <returns>the modification time on the remote machine, accounting for rough difference in time between the two machines</returns>
		public static bool GetRemoteFileInfo(string RemotePath, out DateTime ModificationTime, out long Length)
		{
			if (RemoteToolChain.bUseRPCUtil)
			{
				return RPCCommandHelper.GetFileInfo(GetSocket(), RemotePath, DateTime.UtcNow, out ModificationTime, out Length);
			}
			else
			{
				string CommandArgs = string.Format("-c 'if [ -e \"{0}\" ]; then eval $(stat -s \"{0}\") && echo $st_mtime,$st_size; fi'", RemotePath);
				Hashtable Results = Command("/", "bash", CommandArgs, null);

				string Output = Results["CommandOutput"] as string;
				string[] Tokens = Output.Split(",".ToCharArray());
				if (Tokens.Length == 2)
				{
					ModificationTime = RemoteToLocalTime(Tokens[0]);
					Length = long.Parse(Tokens[1]);
					return true;
				}

				// any failures will fall through to here
				ModificationTime = DateTime.MinValue;
				Length = 0;
				return false;

			}
		}

		public static void MakeDirectory(string Directory)
		{
			if (RemoteToolChain.bUseRPCUtil)
			{
				RPCCommandHelper.MakeDirectory(GetSocket(), Directory);
			}
			else
			{
				Command("/", "bash", "-c 'mkdir \"" + Directory + "\"'", null);
			}
		}

		[Flags]
		public enum ECopyOptions
		{
			None = 0,
			IsUpload = 1 << 0,
			DoNotReplace = 1 << 1, // if used, will merge a directory
			DoNotUnpack = 1 << 2
		}

		public static void CopyFile(string Source, string Dest, bool bIsUpload)
		{
			if (RemoteToolChain.bUseRPCUtil)
			{
				if (bIsUpload)
				{
					RPCCommandHelper.RPCUpload(GetSocket(), Source, Dest);
				}
				else
				{
					RPCCommandHelper.RPCDownload(GetSocket(), Source, Dest);
				}
			}
			else
			{
				if (bIsUpload)
				{
					RemoteToolChain.UploadFile(Source, Dest);
				}
				else
				{
					RemoteToolChain.DownloadFile(Source, Dest);
				}
			}
		}

		// @todo: use temp, random names for zip files
		public static void CopyDirectory(string Source, string Dest, ECopyOptions Options)
		{
			string SourceDirName = Path.GetFileName(Source);
			string DestDirName = Path.GetFileName(Dest);

			if (Options.HasFlag(ECopyOptions.IsUpload))
			{
				if (!Directory.Exists(Source))
				{
					return;
				}

				// Zip source directory
				string SourceZipPath = Path.Combine(Path.GetFullPath(Path.GetDirectoryName(Source)), SourceDirName + ".zip");
				File.Delete(SourceZipPath);
				using (ZipFile Zip = new ZipFile(SourceZipPath))
				{
					Zip.CompressionLevel = Ionic.Zlib.CompressionLevel.Level9;
					Zip.BufferSize = 0x10000;
					Zip.AddDirectory(Source, DestDirName);
					Zip.Save();
				}

				// Upload the zip file
				string DestWorkingDir = Path.GetDirectoryName(Dest).Replace("\\", "/");
				string DestZipName = DestDirName + ".zip";
				CopyFile(SourceZipPath, DestWorkingDir + "/" + DestZipName, true);

				if (!Options.HasFlag(ECopyOptions.DoNotReplace))
				{
					Command(DestWorkingDir, "rm -rf \"" + DestDirName + "\"", "", null);
				}

				if (!Options.HasFlag(ECopyOptions.DoNotUnpack))
				{
					// Unpack, if requested
					Command(DestWorkingDir, "unzip \"" + DestZipName + "\"", "", null);
					Command(DestWorkingDir, "rm \"" + DestZipName + "\"", "", null);
				}

				File.Delete(SourceZipPath);
			}
			else
			{
				// Zip source directory
				string SourceWorkingDir = Path.GetDirectoryName(Source).Replace("\\", "/");
				string ZipCommand = "zip -0 -r -y -T " + SourceDirName + ".zip " + SourceDirName;
				Command(SourceWorkingDir, ZipCommand, "", null);

				// Download the zip file
				string SourceZipPath = Path.Combine(Path.GetDirectoryName(Source), SourceDirName + ".zip").Replace("\\", "/");
				string DestZipPath = Path.Combine(Path.GetFullPath(Path.GetDirectoryName(Dest)), DestDirName + ".zip");
				CopyFile(SourceZipPath, DestZipPath, false);

				if (!Options.HasFlag(ECopyOptions.DoNotReplace) && Directory.Exists(Dest))
				{
					Directory.GetFiles(Dest, "*", SearchOption.AllDirectories).ToList().ForEach(Entry => { File.SetAttributes(Entry, FileAttributes.Normal); });
					Directory.Delete(Dest, true);
				}

				if (!Options.HasFlag(ECopyOptions.DoNotUnpack))
				{
					// Unpack, if requested
					using (ZipFile Zip = ZipFile.Read(DestZipPath))
					{
						Zip.ToList().ForEach(Entry =>
						{
							Entry.FileName = DestDirName + Entry.FileName.Substring(SourceDirName.Length);
							Entry.Extract(Path.GetDirectoryName(Dest), ExtractExistingFileAction.OverwriteSilently);
						});
					}

					File.Delete(DestZipPath);
				}

				Command(SourceWorkingDir, "rm \"" + SourceDirName + ".zip\"", "", null);
			}
		}

		public static void BatchUpload(string[] Commands)
		{
			// batch upload
			RPCCommandHelper.RPCBatchUpload(GetSocket(), Commands);
		}

		public static void BatchFileInfo(FileItem[] Files)
		{
			if (RemoteToolChain.bUseRPCUtil)
			{
				// build a list of file paths to get info about
				StringBuilder FileList = new StringBuilder();
				foreach (FileItem File in Files)
				{
					FileList.AppendFormat("{0}\n", File.AbsolutePath);
				}

				DateTime Now = DateTime.Now;

				// execute the command!
				Int64[] FileSizeAndDates = RPCCommandHelper.RPCBatchFileInfo(GetSocket(), FileList.ToString());

				Console.WriteLine("BatchFileInfo version 1 took {0}", (DateTime.Now - Now).ToString());

				// now update the source times
				for (int Index = 0; Index < Files.Length; Index++)
				{
					Files[Index].Length = FileSizeAndDates[Index * 2 + 0];
					Files[Index].LastWriteTime = new DateTimeOffset(RPCCommandHelper.FromRemoteTime(FileSizeAndDates[Index * 2 + 1]));
					Files[Index].bExists = FileSizeAndDates[Index * 2 + 0] >= 0;
				}
			}
			else
			{
				// build a list of file paths to get info about
				StringBuilder Commands = new StringBuilder();
				Commands.Append("#!/bin/bash\n");
				foreach (FileItem File in Files)
				{
					Commands.AppendFormat("if [ -e \"{0}\" ]; then eval $(stat -s \"{0}\") && echo $st_mtime && echo $st_size; else echo 0 && echo -1; fi\n", File.AbsolutePath);
				}

				// write out locally
				string LocalCommandsFile = Path.GetTempFileName();
				System.IO.File.WriteAllText(LocalCommandsFile, Commands.ToString());

				string RemoteDir = "/var/tmp/" + Environment.MachineName;
				string RemoteCommandsFile = Path.GetFileName(LocalCommandsFile) + ".sh";

				DateTime Now = DateTime.Now;
				RemoteToolChain.UploadFile(LocalCommandsFile, RemoteDir + "/" + RemoteCommandsFile);

				// execute the file, not a commandline
				Hashtable Results = Command(RemoteDir, "sh", RemoteCommandsFile + " && rm " + RemoteCommandsFile, null);

				Console.WriteLine("BatchFileInfo took {0}", (DateTime.Now - Now).ToString());

				string[] Lines = ((string)Results["CommandOutput"]).Split("\r\n".ToCharArray(), StringSplitOptions.RemoveEmptyEntries);
				if (Lines.Length != Files.Length * 2)
				{
					throw new BuildException("Received the wrong number of results from BatchFileInfo");
				}

				for (int Index = 0; Index < Files.Length; Index++)
				{
					Files[Index].LastWriteTime = new DateTimeOffset(RemoteToLocalTime(Lines[Index * 2 + 0]));
					Files[Index].Length = long.Parse(Lines[Index * 2 + 1]);
					Files[Index].bExists = Files[Index].Length >= 0;
				}
			}
		}

		public static int GetCommandSlots()
		{
			if (RemoteToolChain.bUseRPCUtil)
			{
				return RPCCommandHelper.GetCommandSlots(GetSocket());
			}
			else
			{
				return MaxRemoteCommandsAllowed;
			}
		}

		public static Hashtable Command(string WorkingDirectory, string CommandWithArgs, string RemoteOutputPath)
		{
			int FirstSpace = CommandWithArgs.IndexOf(' ');
			if (FirstSpace == -1)
			{
				return Command(WorkingDirectory, CommandWithArgs, " ", RemoteOutputPath);
			}

			return Command(WorkingDirectory, CommandWithArgs.Substring(0, FirstSpace), CommandWithArgs.Substring(FirstSpace + 1), RemoteOutputPath);

		}

		public static Hashtable Command(string WorkingDirectory, string Command, string CommandArgs, string RemoteOutputPath)
		{
			if (RemoteToolChain.bUseRPCUtil)
			{
				int RetriesRemaining = 6;
				do
				{
					// a $ on the commandline will actually be converted, so we need to quote it
					CommandArgs = CommandArgs.Replace("$", "\\$");

					try
					{
						Hashtable Results = RPCCommandHelper.RPCCommand(GetSocket(), WorkingDirectory, Command, CommandArgs, RemoteOutputPath);
						return Results;
					}
					catch (Exception Ex)
					{
						if (RetriesRemaining > 0)
						{
							Int32 RetryTimeoutMS = 1000;
							Debug.WriteLine("Retrying command after sleeping for " + RetryTimeoutMS + " milliseconds. Command is:" + Command + " " + CommandArgs);
							Thread.Sleep(RetryTimeoutMS);
						}
						else
						{
							Log.TraceInformation("Out of retries, too many exceptions:" + Ex.ToString());
							// We've tried enough times, just throw the error
							throw new Exception("Deep Exception, retries exhausted... ", Ex);
						}
						RetriesRemaining--;
					}
				}
				while (RetriesRemaining > 0);

				return null;

			}
			else
			{
				return RemoteToolChain.SSHCommand(WorkingDirectory, Command + " " + CommandArgs, RemoteOutputPath);
			}
		}
	}
}
