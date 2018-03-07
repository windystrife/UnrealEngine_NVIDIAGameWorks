// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// This software is provided "as-is," without any express or implied warranty. 
// In no event shall the author, nor Epic Games, Inc. be held liable for any damages arising from the use of this software.
// This software will not be supported.
// Use at your own risk.
using System;
using System.Threading;
using System.Diagnostics;
using UnrealBuildTool;
using System.Reflection;
using Tools.DotNETCommon;
using System.IO;
using System.Collections.Generic;
using System.Text;

namespace AutomationTool
{
	public class Program
	{
		/// <summary>
		/// Keep a persistent reference to the delegate for handling Ctrl-C events. Since it's passed to non-managed code, we have to prevent it from being garbage collected.
		/// </summary>
		static ProcessManager.CtrlHandlerDelegate CtrlHandlerDelegateInstance = CtrlHandler;

		[STAThread]
		public static int Main()
		{
			// Set the working directory to the UE4 root
			Environment.CurrentDirectory = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().GetOriginalLocation()), "..", "..", ".."));

            var CommandLine = SharedUtils.ParseCommandLine();
            LogUtils.InitLogging(CommandLine);

            ExitCode ReturnCode = ExitCode.Success;

			try
			{
				// ensure we can resolve any external assemblies as necessary.
				AssemblyUtils.InstallAssemblyResolver(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()));
				HostPlatform.Initialize();

				Log.TraceVerbose("{2}: Running on {0} as a {1}-bit process.", HostPlatform.Current.GetType().Name, Environment.Is64BitProcess ? 64 : 32, DateTime.UtcNow.ToString("o"));

				// Log if we're running from the launcher
				var ExecutingAssemblyLocation = Assembly.GetExecutingAssembly().Location;
				if (string.Compare(ExecutingAssemblyLocation, Assembly.GetEntryAssembly().GetOriginalLocation(), StringComparison.OrdinalIgnoreCase) != 0)
				{
					Log.TraceVerbose("Executed from AutomationToolLauncher ({0})", ExecutingAssemblyLocation);
				}
				Log.TraceVerbose("CWD={0}", Environment.CurrentDirectory);

				// Hook up exit callbacks
				var Domain = AppDomain.CurrentDomain;
				Domain.ProcessExit += Domain_ProcessExit;
				Domain.DomainUnload += Domain_ProcessExit;
				HostPlatform.Current.SetConsoleCtrlHandler(CtrlHandlerDelegateInstance);

				var Version = AssemblyUtils.ExecutableVersion;
				Log.TraceVerbose("{0} ver. {1}", Version.ProductName, Version.ProductVersion);

				// Don't allow simultaneous execution of AT (in the same branch)
				ReturnCode = InternalUtils.RunSingleInstance(MainProc, CommandLine);

			}
			catch (Exception Ex)
			{
				ExceptionUtils.PrintExceptionInfo(Ex, LogUtils.FinalLogFileName);
				ReturnCode = (Ex is AutomationException)? ((AutomationException)Ex).ErrorCode : ExitCode.Error_Unknown;
			}
            finally
            {
                // In all cases, do necessary shut down stuff, but don't let any additional exceptions leak out while trying to shut down.

                // Make sure there's no directories on the stack.
                NoThrow(() => CommandUtils.ClearDirStack(), "Clear Dir Stack");

                // Try to kill process before app domain exits to leave the other KillAll call to extreme edge cases
                NoThrow(() => { if (ShouldKillProcesses && !Utils.IsRunningOnMono) ProcessManager.KillAll(); }, "Kill All Processes");

                Log.TraceInformation("AutomationTool exiting with ExitCode={0} ({1})", (int)ReturnCode, ReturnCode);

                // Can't use NoThrow here because the code logs exceptions. We're shutting down logging!
                LogUtils.ShutdownLogging();
            }

            // STOP: No code beyond the return statement should go beyond this point!
            // Nothing should happen after the finally block above is finished. 
            return (int)ReturnCode;
        }

		/// <summary>
		/// Wraps an action in an exception block.
		/// Ensures individual actions can be performed and exceptions won't prevent further actions from being executed.
		/// Useful for shutdown code where shutdown may be in several stages and it's important that all stages get a chance to run.
		/// </summary>
		/// <param name="Action"></param>
		private static void NoThrow(System.Action Action, string ActionDesc)
        {
            try
            {
                Action();
            }
            catch (Exception Ex)
            {
                Log.TraceError("Exception performing nothrow action \"{0}\": {1}", ActionDesc, LogUtils.FormatException(Ex));
            }
        }

        static bool CtrlHandler(CtrlTypes EventType)
		{
			Domain_ProcessExit(null, null);
			if (EventType == CtrlTypes.CTRL_C_EVENT)
			{
				// Force exit
				Environment.Exit(3);
			}			
			return true;
		}

		static void Domain_ProcessExit(object sender, EventArgs e)
		{
			// Kill all spawned processes (Console instead of Log because logging is closed at this time anyway)
			if (ShouldKillProcesses && !Utils.IsRunningOnMono)
			{			
				ProcessManager.KillAll();
			}
			Trace.Close();
		}

		static ExitCode MainProc(object Param)
		{
			ExitCode Result = Automation.Process((string[])Param);
			ShouldKillProcesses = Automation.ShouldKillProcesses;
			return Result;
		}

		static bool ShouldKillProcesses = true;
	}
}
