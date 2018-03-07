// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading;
using System.Reflection;
using System.Linq;
using System.Net.NetworkInformation;
using System.Collections;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

/// <summary>
/// Helper command to run a game.
/// </summary>
/// <remarks>
/// Uses the following command line params:
/// -cooked
/// -cookonthefly
/// -dedicatedserver
/// -win32
/// -noclient
/// -logwindow
/// </remarks>
public partial class Project : CommandUtils
{
	#region Fields

	/// <summary>
	/// Thread used to read client log file.
	/// </summary>
	private static Thread ClientLogReaderThread = null;

	/// <summary>
	/// Process for the server, can be set by the cook command when a cook on the fly server is used
	/// </summary>
	public static IProcessResult ServerProcess;

	#endregion

	#region Run Command

	// debug commands for the engine to crash
	public static string[] CrashCommands = 
    {
        "crash",
        "CHECK",
        "GPF",
        "ASSERT",
        "ENSURE",
        "RENDERCRASH",
        "RENDERCHECK",
        "RENDERGPF",
        "THREADCRASH",
        "THREADCHECK",
        "THREADGPF",
    };

	/// <summary>
	/// For not-installed runs, returns a temp log folder to make sure it doesn't fall into sandbox paths
	/// </summary>
	/// <returns></returns>
	private static string GetLogFolderOutsideOfSandbox()
	{
		return GlobalCommandLine.Installed ?
			CmdEnv.LogFolder :
			CombinePaths(Path.GetTempPath(), CommandUtils.EscapePath(CmdEnv.LocalRoot), "Logs");
	}

	/// <summary>
	/// Fot not-installed runs, copies all logs from the temp log folder back to the UAT log folder.
	/// </summary>
	private static void CopyLogsBackToLogFolder()
	{
		if (!GlobalCommandLine.Installed)
		{
			var LogFolderOutsideOfSandbox = GetLogFolderOutsideOfSandbox();
			var TempLogFiles = FindFiles_NoExceptions("*", false, LogFolderOutsideOfSandbox);
			foreach (var LogFilename in TempLogFiles)
			{
				var DestFilename = CombinePaths(CmdEnv.LogFolder, Path.GetFileName(LogFilename));
				CopyFile_NoExceptions(LogFilename, DestFilename);
			}
		}
	}

	public static void Run(ProjectParams Params)
	{
		Params.ValidateAndLog();
		if (!Params.Run)
		{
			return;
		}

		Log("********** RUN COMMAND STARTED **********");

		var LogFolderOutsideOfSandbox = GetLogFolderOutsideOfSandbox();
		if (!GlobalCommandLine.Installed && ServerProcess == null)
		{
			// In the installed runs, this is the same folder as CmdEnv.LogFolder so delete only in not-installed
			DeleteDirectory(LogFolderOutsideOfSandbox);
			CreateDirectory(LogFolderOutsideOfSandbox);
		}
		var ServerLogFile = CombinePaths(LogFolderOutsideOfSandbox, "Server.log");
		var ClientLogFile = CombinePaths(LogFolderOutsideOfSandbox, Params.EditorTest ? "Editor.log" : "Client.log");

		try
		{
			RunInternal(Params, ServerLogFile, ClientLogFile);
		}
		catch
		{
			throw;
		}
		finally
		{
			CopyLogsBackToLogFolder();
		}

		Log("********** RUN COMMAND COMPLETED **********");
	}

	private static void RunInternal(ProjectParams Params, string ServerLogFile, string ClientLogFile)
	{
		// Setup server process if required.
		if (Params.DedicatedServer && !Params.SkipServer)
		{
			if (Params.ServerTargetPlatforms.Count > 0)
			{
				TargetPlatformDescriptor ServerPlatformDesc = Params.ServerTargetPlatforms[0];
				ServerProcess = RunDedicatedServer(Params, ServerLogFile, Params.RunCommandline);

                // With dedicated server, the client connects to local host to load a map, unless client parameters are already specified
                if (String.IsNullOrEmpty(Params.ClientCommandline))
                {
                    if (!String.IsNullOrEmpty(Params.ServerDeviceAddress))
                    {
                        Params.ClientCommandline = Params.ServerDeviceAddress;
                    }
                    else
                    {
                        Params.ClientCommandline = "127.0.0.1";
                    }
                }
			}
			else
			{
				throw new AutomationException("Failed to run, server target platform not specified");
			}
		}
		else if (Params.FileServer && !Params.SkipServer)
		{
			ServerProcess = RunFileServer(Params, ServerLogFile, Params.RunCommandline);
		}

		if (ServerProcess != null)
		{
			Log("Waiting a few seconds for the server to start...");
			Thread.Sleep(5000);
		}

		if (!Params.NoClient)
		{
			Log("Starting Client....");

			var SC = CreateDeploymentContext(Params, false);

			ERunOptions ClientRunFlags;
			string ClientApp;
			string ClientCmdLine;
			SetupClientParams(SC, Params, ClientLogFile, out ClientRunFlags, out ClientApp, out ClientCmdLine);

			// Run the client.
			if (ServerProcess != null)
			{
				RunClientWithServer(SC, ServerLogFile, ServerProcess, ClientApp, ClientCmdLine, ClientRunFlags, ClientLogFile, Params);
			}
			else
			{
				RunStandaloneClient(SC, ClientLogFile, ClientRunFlags, ClientApp, ClientCmdLine, Params);
			}
		}
	}

	#endregion

	#region Client

	private static void RunStandaloneClient(List<DeploymentContext> DeployContextList, string ClientLogFile, ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (Params.Unattended)
		{
			string LookFor = "Bringing up level for play took";
			bool bCommandlet = false;

			if (Params.RunAutomationTest != "")
			{
				LookFor = "Automation Test Succeeded";
			}
			else if (Params.RunAutomationTests)
			{
				LookFor = "Automation Test Queue Empty";
			}
			else if (Params.EditorTest)
			{
				LookFor = "Asset discovery search completed in";
			}
			// If running a commandlet, just detect a normal exit
			else if (ClientCmdLine.IndexOf("-run=", StringComparison.InvariantCultureIgnoreCase) >= 0)
			{
				LookFor = "Game engine shut down";
				bCommandlet = true;
			}

			{

				string AllClientOutput = "";
				int LastAutoFailIndex = -1;
				IProcessResult ClientProcess = null;
				FileStream ClientProcessLog = null;
				StreamReader ClientLogReader = null;
				Log("Starting Client for unattended test....");
				ClientProcess = Run(ClientApp, ClientCmdLine + " -testexit=\"" + LookFor + "\"", null, ClientRunFlags | ERunOptions.NoWaitForExit);
				while (!FileExists(ClientLogFile) && !ClientProcess.HasExited)
				{
					Log("Waiting for client logging process to start...{0}", ClientLogFile);
					Thread.Sleep(2000);
				}
				if (FileExists(ClientLogFile))
				{
					Thread.Sleep(2000);
					Log("Client logging process started...{0}", ClientLogFile);
					ClientProcessLog = File.Open(ClientLogFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
					ClientLogReader = new StreamReader(ClientProcessLog);
				}
				if (ClientLogReader == null)
				{
					throw new AutomationException("Client exited without creating a log file.");
				}
				bool bKeepReading = true;
				bool WelcomedCorrectly = false;
				bool bClientExited = false;
				DateTime ExitTime = DateTime.UtcNow;

				while (bKeepReading)
				{
					if (!bClientExited && ClientProcess.HasExited)
					{
						ExitTime = DateTime.UtcNow;
						bClientExited = true;
					}
					string ClientOutput = ClientLogReader.ReadToEnd();
					if (!String.IsNullOrEmpty(ClientOutput))
					{
						if (bClientExited)
						{
							ExitTime = DateTime.UtcNow; // as long as it is spewing, we reset the timer
						}
						AllClientOutput += ClientOutput;
						Console.Write(ClientOutput);

						if (AllClientOutput.LastIndexOf(LookFor) > AllClientOutput.IndexOf(LookFor))
						{
							WelcomedCorrectly = true;
							Log("Test complete...");
							bKeepReading = false;
						}
						else if (Params.RunAutomationTests)
						{
							int FailIndex = AllClientOutput.LastIndexOf("Automation Test Failed");
							int ParenIndex = AllClientOutput.LastIndexOf(")");
							if (FailIndex >= 0 && ParenIndex > FailIndex && FailIndex > LastAutoFailIndex)
							{
								string Tail = AllClientOutput.Substring(FailIndex);
								int CloseParenIndex = Tail.IndexOf(")");
								int OpenParenIndex = Tail.IndexOf("(");
								string Test = "";
								if (OpenParenIndex >= 0 && CloseParenIndex > OpenParenIndex)
								{
									Test = Tail.Substring(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1);
									LogError("Automated test failed ({0}).", Test);
									LastAutoFailIndex = FailIndex;
								}
							}
						}
						// Detect commandlet failure
						else if (bCommandlet)
						{
							const string ResultLog = "Commandlet->Main return this error code: ";

							int ResultStart = AllClientOutput.LastIndexOf(ResultLog);
							int ResultValIdx = ResultStart + ResultLog.Length;

							if (ResultStart >= 0 && ResultValIdx < AllClientOutput.Length &&
								AllClientOutput.Substring(ResultValIdx, 1) == "1")
							{
								// Parse the full commandlet warning/error summary
								string FullSummary = "";
								int SummaryStart = AllClientOutput.LastIndexOf("Warning/Error Summary");
								
								if (SummaryStart >= 0 && SummaryStart < ResultStart)
								{
									FullSummary = AllClientOutput.Substring(SummaryStart, ResultStart - SummaryStart);
								}


								if (FullSummary.Length > 0)
								{
									LogError("Commandlet failed, summary:" + Environment.NewLine +
																					FullSummary);
								}
								else
								{
									LogError("Commandlet failed.");
								}
							}
						}
					}
					else if (bClientExited && (DateTime.UtcNow - ExitTime).TotalSeconds > 30)
					{
						Log("Client exited and has been quiet for 30 seconds...exiting");
						bKeepReading = false;
					}
				}
				if (ClientProcess != null && !ClientProcess.HasExited)
				{
					Log("Client is supposed to exit, lets wait a while for it to exit naturally...");
					for (int i = 0; i < 120 && !ClientProcess.HasExited; i++)
					{
						Thread.Sleep(1000);
					}
				}
				if (ClientProcess != null && !ClientProcess.HasExited)
				{
					Log("Stopping client...");
					ClientProcess.StopProcess();
					Thread.Sleep(10000);
				}
				while (!ClientLogReader.EndOfStream)
				{
					string ClientOutput = ClientLogReader.ReadToEnd();
					if (!String.IsNullOrEmpty(ClientOutput))
					{
						Console.Write(ClientOutput);
					}
				}

				if (!WelcomedCorrectly)
				{
					throw new AutomationException("Client exited before we asked it to.");
				}
			}
		}
		else
		{
			var SC = DeployContextList[0];
			IProcessResult ClientProcess = SC.StageTargetPlatform.RunClient(ClientRunFlags, ClientApp, ClientCmdLine, Params);
			if (ClientProcess != null)
			{
				// If the client runs without StdOut redirect we're going to read the log output directly from log file on
				// a separate thread.
				if ((ClientRunFlags & ERunOptions.NoStdOutRedirect) == ERunOptions.NoStdOutRedirect)
				{
					ClientLogReaderThread = new System.Threading.Thread(ClientLogReaderProc);
					ClientLogReaderThread.Start(new object[] { ClientLogFile, ClientProcess });
				}

				do
				{
					Thread.Sleep(100);
				}
				while (ClientProcess.HasExited == false);

				SC.StageTargetPlatform.PostRunClient(ClientProcess, Params);

                // any non-zero exit code should propagate an exception. The Virtual function above may have
                // already thrown a more specific exception or given a more specific ErrorCode, but this catches the rest.
				if (ClientProcess.ExitCode != 0)
				{
					throw new AutomationException("Client exited with error code: " + ClientProcess.ExitCode);
				}
			}
		}
	}

	private static void RunClientWithServer(List<DeploymentContext> DeployContextList, string ServerLogFile, IProcessResult ServerProcess, string ClientApp, string ClientCmdLine, ERunOptions ClientRunFlags, string ClientLogFile, ProjectParams Params)
	{
		IProcessResult ClientProcess = null;
		var OtherClients = new List<IProcessResult>();

		bool WelcomedCorrectly = false;
		int NumClients = Params.NumClients;
		string AllClientOutput = "";
		int LastAutoFailIndex = -1;

		if (Params.Unattended)
		{
			string LookFor = "Bringing up level for play took";
			if (Params.DedicatedServer)
			{
				LookFor = "Welcomed by server";
			}
			else if (Params.RunAutomationTest != "")
			{
				LookFor = "Automation Test Succeeded";
			}
			else if (Params.RunAutomationTests)
			{
				LookFor = "Automation Test Queue Empty";
			}
			{
				while (!FileExists(ServerLogFile) && !ServerProcess.HasExited)
				{
					Log("Waiting for logging process to start...");
					Thread.Sleep(2000);
				}
				Thread.Sleep(1000);

				string AllServerOutput = "";
				using (FileStream ProcessLog = File.Open(ServerLogFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
				{
					StreamReader LogReader = new StreamReader(ProcessLog);
					bool bKeepReading = true;

					FileStream ClientProcessLog = null;
					StreamReader ClientLogReader = null;

					// Read until the process has exited.
					while (!ServerProcess.HasExited && bKeepReading)
					{
						while (!LogReader.EndOfStream && bKeepReading && ClientProcess == null)
						{
							string Output = LogReader.ReadToEnd();
							if (!String.IsNullOrEmpty(Output))
							{
								AllServerOutput += Output;
								if (ClientProcess == null &&
										(AllServerOutput.Contains("Game Engine Initialized") || AllServerOutput.Contains("Unreal Network File Server is ready")))
								{
									Log("Starting Client for unattended test....");
									ClientProcess = Run(ClientApp, ClientCmdLine + " -testexit=\"" + LookFor + "\"", null, ClientRunFlags | ERunOptions.NoWaitForExit);
									//@todo no testing is done on these
									if (NumClients > 1 && NumClients < 9)
									{
										for (int i = 1; i < NumClients; i++)
										{
											Log("Starting Extra Client....");
											OtherClients.Add(Run(ClientApp, ClientCmdLine, null, ClientRunFlags | ERunOptions.NoWaitForExit));
										}
									}
									while (!FileExists(ClientLogFile) && !ClientProcess.HasExited)
									{
										Log("Waiting for client logging process to start...{0}", ClientLogFile);
										Thread.Sleep(2000);
									}
									if (!ClientProcess.HasExited)
									{
										Thread.Sleep(2000);
										Log("Client logging process started...{0}", ClientLogFile);
										ClientProcessLog = File.Open(ClientLogFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
										ClientLogReader = new StreamReader(ClientProcessLog);
									}
								}
								else if (ClientProcess == null && !ServerProcess.HasExited)
								{
									Log("Waiting for server to start....");
									Thread.Sleep(2000);
								}
								if (ClientProcess != null && ClientProcess.HasExited)
								{
									ServerProcess.StopProcess();
									throw new AutomationException("Client exited before we asked it to.");
								}
							}
						}
						if (ClientLogReader != null)
						{
							if (ClientProcess.HasExited)
							{
								ServerProcess.StopProcess();
								throw new AutomationException("Client exited or closed the log before we asked it to.");
							}
							while (!ClientProcess.HasExited && !ServerProcess.HasExited && bKeepReading)
							{
								while (!ClientLogReader.EndOfStream && bKeepReading && !ServerProcess.HasExited && !ClientProcess.HasExited)
								{
									string ClientOutput = ClientLogReader.ReadToEnd();
									if (!String.IsNullOrEmpty(ClientOutput))
									{
										AllClientOutput += ClientOutput;
										Console.Write(ClientOutput);

										if (AllClientOutput.LastIndexOf(LookFor) > AllClientOutput.IndexOf(LookFor))
										{
											if (Params.FakeClient)
											{
												Log("Welcomed by server or client loaded, lets wait ten minutes...");
												Thread.Sleep(60000 * 10);
											}
											else
											{
												Log("Welcomed by server or client loaded, lets wait 30 seconds...");
												Thread.Sleep(30000);
											}
											WelcomedCorrectly = true;
											bKeepReading = false;
										}
										else if (Params.RunAutomationTests)
										{
											int FailIndex = AllClientOutput.LastIndexOf("Automation Test Failed");
											int ParenIndex = AllClientOutput.LastIndexOf(")");
											if (FailIndex >= 0 && ParenIndex > FailIndex && FailIndex > LastAutoFailIndex)
											{
												string Tail = AllClientOutput.Substring(FailIndex);
												int CloseParenIndex = Tail.IndexOf(")");
												int OpenParenIndex = Tail.IndexOf("(");
												string Test = "";
												if (OpenParenIndex >= 0 && CloseParenIndex > OpenParenIndex)
												{
													Test = Tail.Substring(OpenParenIndex + 1, CloseParenIndex - OpenParenIndex - 1);
													LogError("Automated test failed ({0}).", Test);
													LastAutoFailIndex = FailIndex;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		else
		{

			LogFileReaderProcess(ServerLogFile, ServerProcess, (string Output) =>
			{
				bool bKeepReading = true;
				if (ClientProcess == null && !String.IsNullOrEmpty(Output))
				{
					AllClientOutput += Output;
					if (AllClientOutput.Contains("Game Engine Initialized") || AllClientOutput.Contains("Unreal Network File Server is ready"))
					{
						Log("Starting Client....");
						var SC = DeployContextList[0];
						ClientProcess = SC.StageTargetPlatform.RunClient(ClientRunFlags | ERunOptions.NoWaitForExit, ClientApp, ClientCmdLine, Params);
//						ClientProcess = Run(ClientApp, ClientCmdLine, null, ClientRunFlags | ERunOptions.NoWaitForExit);
						if (NumClients > 1 && NumClients < 9)
						{
							for (int i = 1; i < NumClients; i++)
							{
								Log("Starting Extra Client....");
								IProcessResult NewClient = SC.StageTargetPlatform.RunClient(ClientRunFlags | ERunOptions.NoWaitForExit, ClientApp, ClientCmdLine, Params);
								OtherClients.Add(NewClient);
							}
						}
					}
				}
				else if (ClientProcess == null && !ServerProcess.HasExited)
				{
					Log("Waiting for server to start....");
					Thread.Sleep(2000);
				}

				if (String.IsNullOrEmpty(Output) == false)
				{
					Console.Write(Output);
				}

				if (ClientProcess != null && ClientProcess.HasExited)
				{

					Log("Client exited, stopping server....");
					if (!GlobalCommandLine.NoKill)
					{
						ServerProcess.StopProcess();
					}
					bKeepReading = false;
				}

				return bKeepReading; // Keep reading
			});
		}
		Log("Server exited....");
		if (ClientProcess != null && !ClientProcess.HasExited)
		{
			ClientProcess.StopProcess();
		}
		foreach (var OtherClient in OtherClients)
		{
			if (OtherClient != null && !OtherClient.HasExited)
			{
				OtherClient.StopProcess();
			}
		}
		if (Params.Unattended)
		{
			if (!WelcomedCorrectly)
			{
				throw new AutomationException("Server or client exited before we asked it to.");
			}
		}
	}

	private static void SetupClientParams(List<DeploymentContext> DeployContextList, ProjectParams Params, string ClientLogFile, out ERunOptions ClientRunFlags, out string ClientApp, out string ClientCmdLine)
	{
		if (Params.ClientTargetPlatforms.Count == 0)
		{
			throw new AutomationException("No ClientTargetPlatform set for SetupClientParams.");
		}

		//		var DeployContextList = CreateDeploymentContext(Params, false);

		if (DeployContextList.Count == 0)
		{
			throw new AutomationException("No DeployContextList for SetupClientParams.");
		}

		var SC = DeployContextList[0];

		// Get client app name and command line.
		ClientRunFlags = ERunOptions.AllowSpew | ERunOptions.AppMustExist;
		ClientApp = "";
		ClientCmdLine = "";
		string TempCmdLine = SC.ProjectArgForCommandLines + " ";
		var PlatformName = Params.ClientTargetPlatforms[0].ToString();
		if (Params.Cook || Params.CookOnTheFly)
		{
			List<FileReference> Exes = SC.StageTargetPlatform.GetExecutableNames(SC);
			ClientApp = Exes[0].FullName;

            if (!String.IsNullOrEmpty(Params.ClientCommandline))
            {
                TempCmdLine += Params.ClientCommandline + " ";
            }
            else
            {
                TempCmdLine += Params.MapToRun + " ";
            }

			if (Params.CookOnTheFly || Params.FileServer)
			{
				TempCmdLine += "-filehostip=";
				bool FirstParam = true;
				if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					NetworkInterface[] Interfaces = NetworkInterface.GetAllNetworkInterfaces();
					foreach (NetworkInterface adapter in Interfaces)
					{
						if (adapter.NetworkInterfaceType != NetworkInterfaceType.Loopback)
						{
							IPInterfaceProperties IP = adapter.GetIPProperties();
							for (int Index = 0; Index < IP.UnicastAddresses.Count; ++Index)
							{
								if (IP.UnicastAddresses[Index].IsDnsEligible && IP.UnicastAddresses[Index].Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
								{
									if (!IsNullOrEmpty(Params.Port))
									{
										foreach (var Port in Params.Port)
										{
											if (!FirstParam)
											{
												TempCmdLine += "+";
											}
											FirstParam = false;
											string[] PortProtocol = Port.Split(new char[] { ':' });
											if (PortProtocol.Length > 1)
											{
												TempCmdLine += String.Format("{0}://{1}:{2}", PortProtocol[0], IP.UnicastAddresses[Index].Address.ToString(), PortProtocol[1]);
											}
											else
											{
												TempCmdLine += IP.UnicastAddresses[Index].Address.ToString();
												TempCmdLine += ":";
												TempCmdLine += Params.Port;
											}
										}
									}
                                    else
                                    {
										if (!FirstParam)
										{
											TempCmdLine += "+";
										}
										FirstParam = false;
										
										// use default port
                                        TempCmdLine += IP.UnicastAddresses[Index].Address.ToString();
                                    }
								}
							}
						}
					}
				}
				else
				{
					NetworkInterface[] Interfaces = NetworkInterface.GetAllNetworkInterfaces();
					foreach (NetworkInterface adapter in Interfaces)
					{
						if (adapter.OperationalStatus == OperationalStatus.Up)
						{
							IPInterfaceProperties IP = adapter.GetIPProperties();
							for (int Index = 0; Index < IP.UnicastAddresses.Count; ++Index)
							{
								if (IP.UnicastAddresses[Index].IsDnsEligible)
								{
									if (!IsNullOrEmpty(Params.Port))
									{
										foreach (var Port in Params.Port)
										{
											if (!FirstParam)
											{
												TempCmdLine += "+";
											}
											FirstParam = false;
											string[] PortProtocol = Port.Split(new char[] { ':' });
											if (PortProtocol.Length > 1)
											{
												TempCmdLine += String.Format("{0}://{1}:{2}", PortProtocol[0], IP.UnicastAddresses[Index].Address.ToString(), PortProtocol[1]);
											}
											else
											{
												TempCmdLine += IP.UnicastAddresses[Index].Address.ToString();
												TempCmdLine += ":";
												TempCmdLine += Params.Port;
											}
										}
									}
                                    else
                                    {
										if (!FirstParam)
										{
											TempCmdLine += "+";
										}
										FirstParam = false;
										
										// use default port
                                        TempCmdLine += IP.UnicastAddresses[Index].Address.ToString();
                                    }
								}
							}
						}
					}
				}

				const string LocalHost = "127.0.0.1";

				if (!IsNullOrEmpty(Params.Port))
				{
					foreach (var Port in Params.Port)
					{
						if (!FirstParam)
						{
							TempCmdLine += "+";
						}
						FirstParam = false;
						string[] PortProtocol = Port.Split(new char[] { ':' });
						if (PortProtocol.Length > 1)
						{
							TempCmdLine += String.Format("{0}://{1}:{2}", PortProtocol[0], LocalHost, PortProtocol[1]);
						}
						else
						{
							TempCmdLine += LocalHost;
							TempCmdLine += ":";
							TempCmdLine += Params.Port;
						}

					}
				}
                else
                {
					if (!FirstParam)
					{
						TempCmdLine += "+";
					}
					FirstParam = false;
					
					// use default port
                    TempCmdLine += LocalHost;
                }
				TempCmdLine += " ";

				if (Params.CookOnTheFlyStreaming)
				{
					TempCmdLine += "-streaming ";
				}
				else if (SC.StageTargetPlatform.PlatformType != UnrealTargetPlatform.IOS)
				{
					// per josh, allowcaching is deprecated/doesn't make sense for iOS.
					TempCmdLine += "-allowcaching ";
				}
			}
			else if (Params.UsePak(SC.StageTargetPlatform))
			{
				if (Params.SignedPak)
				{
					TempCmdLine += "-signedpak ";
				}
				else
				{
					TempCmdLine += "-pak ";
				}
			}
			else if (!Params.Stage)
			{
				var SandboxPath = CombinePaths(SC.RuntimeProjectRootDir.FullName, "Saved", "Cooked", SC.CookPlatform);
				if (!SC.StageTargetPlatform.LaunchViaUFE)
				{
					TempCmdLine += "-sandbox=" + CommandUtils.MakePathSafeToUseWithCommandLine(SandboxPath) + " ";
				}
				else
				{
					TempCmdLine += "-sandbox=\'" + SandboxPath + "\' ";
				}
			}
		}
		else
		{
			ClientApp = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries", PlatformName, "UE4Editor.exe");
			if (!Params.EditorTest)
			{
				TempCmdLine += "-game " + Params.MapToRun + " ";
			}
            else
            {
                TempCmdLine += Params.MapToRun + " ";
            }
		}
		if (Params.LogWindow)
		{
			// Without NoStdOutRedirect '-log' doesn't log anything to the window
			ClientRunFlags |= ERunOptions.NoStdOutRedirect;
			TempCmdLine += "-log ";
		}
		else
		{
			TempCmdLine += "-stdout ";
		}
		if (Params.Unattended)
		{
			TempCmdLine += "-unattended ";
		}
		if (IsBuildMachine || Params.Unattended)
		{
			TempCmdLine += "-buildmachine ";
		}
		if (Params.CrashIndex > 0)
		{
			int RealIndex = Params.CrashIndex - 1;
			if (RealIndex >= CrashCommands.Count())
			{
				throw new AutomationException("CrashIndex {0} is out of range...max={1}", Params.CrashIndex, CrashCommands.Count());
			}
			TempCmdLine += String.Format("-execcmds=\"debug {0}\" ", CrashCommands[RealIndex]);
		}
		else if (Params.RunAutomationTest != "")
		{
			TempCmdLine += "-execcmds=\"automation list;runtests " + Params.RunAutomationTest + "\" ";
		}
		else if (Params.RunAutomationTests)
		{
			TempCmdLine += "-execcmds=\"automation list;runall\" ";
		}
		if (SC.StageTargetPlatform.UseAbsLog)
		{
			TempCmdLine += "-abslog=" + CommandUtils.MakePathSafeToUseWithCommandLine(ClientLogFile) + " ";
		}
		if (SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Win32 || SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.Win64)
		{
			TempCmdLine += "-Messaging -Windowed ";
		}
		else
		{
			TempCmdLine += "-Messaging ";
		}
		if (Params.NullRHI && SC.StageTargetPlatform.PlatformType != UnrealTargetPlatform.Mac) // all macs have GPUs, and currently the mac dies with nullrhi
		{
			TempCmdLine += "-nullrhi ";
		}
		if (Params.Deploy && !Params.CookOnTheFly && (SC.StageTargetPlatform.PlatformType == UnrealTargetPlatform.PS4))
		{
			TempCmdLine += "-deployedbuild ";
		}

		TempCmdLine += "-CrashForUAT ";
		TempCmdLine += Params.RunCommandline;

		// todo: move this into the platform
		if (SC.StageTargetPlatform.LaunchViaUFE)
		{
			ClientCmdLine = "-run=Launch ";
            ClientCmdLine += "-Device=" + Params.Devices[0];
            for (int DeviceIndex = 1; DeviceIndex < Params.Devices.Count; DeviceIndex++)
            {
                ClientCmdLine += "+" + Params.Devices[DeviceIndex];
            }
            ClientCmdLine += " ";
			ClientCmdLine += "-Exe=\"" + ClientApp + "\" ";
			ClientCmdLine += "-Targetplatform=" + Params.ClientTargetPlatforms[0].ToString() + " ";
			ClientCmdLine += "-Params=\"" + TempCmdLine + "\"";
			ClientApp = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealFrontend.exe");

			Log("Launching via UFE:");
			Log("\tClientCmdLine: " + ClientCmdLine + "");
		}
		else
		{
			ClientCmdLine = TempCmdLine;
		}
	}

	#endregion

	#region Client Thread

	private static void ClientLogReaderProc(object ArgsContainer)
	{
		var Args = ArgsContainer as object[];
		var ClientLogFile = (string)Args[0];
		var ClientProcess = (IProcessResult)Args[1];
		LogFileReaderProcess(ClientLogFile, ClientProcess, (string Output) =>
		{
			if (String.IsNullOrEmpty(Output) == false)
			{
				Log(Output);
			}
			return true;
		});
	}

	#endregion

	#region Servers

	private static IProcessResult RunDedicatedServer(ProjectParams Params, string ServerLogFile, string AdditionalCommandLine)
	{
		ProjectParams ServerParams = new ProjectParams(Params);
		ServerParams.Devices = new ParamList<string>(Params.ServerDevice);

		if (ServerParams.ServerTargetPlatforms.Count == 0)
		{
			throw new AutomationException("No ServerTargetPlatform set for RunDedicatedServer.");
		}

		var DeployContextList = CreateDeploymentContext(ServerParams, true);

		if (DeployContextList.Count == 0)
		{
			throw new AutomationException("No DeployContextList for RunDedicatedServer.");
		}

		var SC = DeployContextList[0];

		var ServerApp = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UE4Editor.exe");
		if (ServerParams.Cook)
		{
			List<FileReference> Exes = SC.StageTargetPlatform.GetExecutableNames(SC);
			ServerApp = Exes[0].FullName;
		}
		var Args = ServerParams.Cook ? "" : (SC.ProjectArgForCommandLines + " ");
		Console.WriteLine(Params.ServerDeviceAddress);
		TargetPlatformDescriptor ServerPlatformDesc = ServerParams.ServerTargetPlatforms[0];
		if (ServerParams.Cook && ServerPlatformDesc.Type == UnrealTargetPlatform.Linux && !String.IsNullOrEmpty(ServerParams.ServerDeviceAddress))
		{
			ServerApp = @"C:\Windows\system32\cmd.exe";

			string plinkPath = CombinePaths(Environment.GetEnvironmentVariable("LINUX_ROOT"), "bin/PLINK.exe ");
			string exePath = CombinePaths(SC.ShortProjectName, "Binaries", ServerPlatformDesc.Type.ToString(), SC.ShortProjectName + "Server");
			if (ServerParams.ServerConfigsToBuild[0] != UnrealTargetConfiguration.Development)
			{
				exePath += "-" + ServerPlatformDesc.Type.ToString() + "-" + ServerParams.ServerConfigsToBuild[0].ToString();
			}
			exePath = CombinePaths("LinuxServer", exePath.ToLower()).Replace("\\", "/");
			Args = String.Format("/k {0} -batch -ssh -t -i {1} {2}@{3} {4} {5} {6} -server -Messaging", plinkPath, ServerParams.DevicePassword, ServerParams.DeviceUsername, ServerParams.ServerDeviceAddress, exePath, Args, ServerParams.MapToRun);
		}
		else
		{
			var Map = ServerParams.MapToRun;
			if (!String.IsNullOrEmpty(ServerParams.AdditionalServerMapParams))
			{
				Map += ServerParams.AdditionalServerMapParams;
			}
			if (Params.FakeClient)
			{
				Map += "?fake";
			}
			Args += String.Format("{0} -server -abslog={1}  -log -Messaging", Map, CommandUtils.MakePathSafeToUseWithCommandLine(ServerLogFile));
			if (Params.Unattended)
			{
				Args += " -unattended";
			}
			
			if (Params.ServerCommandline.Length > 0)
			{
				Args += " " + Params.ServerCommandline;
			}
		}

		if (ServerParams.UsePak(SC.StageTargetPlatform))
		{
			if (ServerParams.SignedPak)
			{
				Args += " -signedpak";
			}
			else
			{
				Args += " -pak";
			}
		}
		if (IsBuildMachine || Params.Unattended)
		{
			Args += " -buildmachine";
		}
		Args += " -CrashForUAT";
		Args += " " + AdditionalCommandLine;


		if (ServerParams.Cook && ServerPlatformDesc.Type == UnrealTargetPlatform.Linux && !String.IsNullOrEmpty(ServerParams.ServerDeviceAddress))
		{
			Args += String.Format(" 2>&1 > {0}", ServerLogFile);
		}

		PushDir(Path.GetDirectoryName(ServerApp));
		var Result = Run(ServerApp, Args, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);
		PopDir();

		return Result;
	}

	private static IProcessResult RunCookOnTheFlyServer(FileReference ProjectName, string ServerLogFile, string TargetPlatform, string AdditionalCommandLine)
	{
		var ServerApp = HostPlatform.Current.GetUE4ExePath("UE4Editor.exe");
		var Args = String.Format("{0} -run=cook -cookonthefly -unattended -CrashForUAT -log",
			CommandUtils.MakePathSafeToUseWithCommandLine(ProjectName.FullName));
		if (!String.IsNullOrEmpty(ServerLogFile))
		{
			Args += " -abslog=" + CommandUtils.MakePathSafeToUseWithCommandLine(ServerLogFile);
		}
		if (IsBuildMachine)
		{
			Args += " -buildmachine";
		}
		Args += " " + AdditionalCommandLine;

		// Run the server (Without NoStdOutRedirect -log doesn't log anything to the window)
		PushDir(Path.GetDirectoryName(ServerApp));
		var Result = Run(ServerApp, Args, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);
		PopDir();
		return Result;
	}

	private static IProcessResult RunFileServer(ProjectParams Params, string ServerLogFile, string AdditionalCommandLine)
	{
#if false
		// this section of code would provide UFS with a more accurate file mapping
		var SC = new StagingContext(Params, false);
		CreateStagingManifest(SC);
		MaybeConvertToLowerCase(Params, SC);
		var UnrealFileServerResponseFile = new List<string>();

		foreach (var Pair in SC.UFSStagingFiles)
		{
			string Src = Pair.Key;
			string Dest = Pair.Value;

			Dest = CombinePaths(PathSeparator.Slash, SC.UnrealFileServerInternalRoot, Dest);

			UnrealFileServerResponseFile.Add("\"" + Src + "\" \"" + Dest + "\"");
		}


		string UnrealFileServerResponseFileName = CombinePaths(CmdEnv.LogFolder, "UnrealFileServerList.txt");
		File.WriteAllLines(UnrealFileServerResponseFileName, UnrealFileServerResponseFile);
#endif
		var UnrealFileServerExe = HostPlatform.Current.GetUE4ExePath("UnrealFileServer.exe");

		Log("Running UnrealFileServer *******");
		var Args = String.Format("{0} -abslog={1} -unattended -CrashForUAT -log {2}",
						CommandUtils.MakePathSafeToUseWithCommandLine(Params.RawProjectPath.FullName),
						CommandUtils.MakePathSafeToUseWithCommandLine(ServerLogFile),
						AdditionalCommandLine);
		if (IsBuildMachine)
		{
			Args += " -buildmachine";
		}
		PushDir(Path.GetDirectoryName(UnrealFileServerExe));
		var Result = Run(UnrealFileServerExe, Args, null, ERunOptions.AllowSpew | ERunOptions.NoWaitForExit | ERunOptions.AppMustExist | ERunOptions.NoStdOutRedirect);
		PopDir();
		return Result;
	}

	#endregion
}
