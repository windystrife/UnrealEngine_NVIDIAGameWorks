// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Diagnostics;

namespace AutomationTool
{
	/// <summary>
	/// Host platform abstraction
	/// </summary>
	public abstract class HostPlatform
	{
		private static HostPlatform RunningPlatform;
		/// <summary>
		/// Current running host platform.
		/// </summary>
		public static HostPlatform Current
		{
			get 
			{ 
				if (RunningPlatform == null)
				{
					throw new AutomationException("UnrealAutomationTool host platform not initialized.");
				}
				return RunningPlatform; 
			}
		}

		/// <summary>
		/// Initializes the current platform.
		/// </summary>
		public static void Initialize()
		{
			PlatformID Platform = Environment.OSVersion.Platform;
			switch (Platform)
			{
				case PlatformID.Win32NT:
				case PlatformID.Win32S:
				case PlatformID.Win32Windows:
				case PlatformID.WinCE:
					RunningPlatform = new WindowsHostPlatform();
					break;

				case PlatformID.Unix:
					if (File.Exists ("/System/Library/CoreServices/SystemVersion.plist"))
					{
						RunningPlatform = new MacHostPlatform();
					} 
					else 
					{
						RunningPlatform = new LinuxHostPlatform();
					}
					break;

				case PlatformID.MacOSX:
					RunningPlatform = new MacHostPlatform();
					break;

				default:
					throw new Exception ("Unhandled runtime platform " + Platform);
			}
		}

		/// <summary>
		/// Gets the build executable filename.
		/// </summary>
		/// <returns></returns>
		abstract public string GetMsBuildExe();

		/// <summary>
		/// Folder under UE4/ to the platform's binaries.
		/// </summary>
		abstract public string RelativeBinariesFolder { get; }

		/// <summary>
		/// Full path to the UE4 Editor executable for the current platform.
		/// </summary>
		/// <param name="UE4Exe"></param>
		/// <returns></returns>
		abstract public string GetUE4ExePath(string UE4Exe);

		/// <summary>
		/// Log folder for local builds.
		/// </summary>
		abstract public string LocalBuildsLogFolder { get; }

		/// <summary>
		/// Name of the p4 executable.
		/// </summary>
		abstract public string P4Exe { get; }

		/// <summary>
		/// Creates a process and sets it up for the current platform.
		/// </summary>
		/// <param name="LogName"></param>
		/// <returns></returns>
		abstract public Process CreateProcess(string AppName);

		/// <summary>
		/// Sets any additional options for running an executable.
		/// </summary>
		/// <param name="AppName"></param>
		/// <param name="Options"></param>
		/// <param name="CommandLine"></param>
		abstract public void SetupOptionsForRun(ref string AppName, ref CommandUtils.ERunOptions Options, ref string CommandLine);

		/// <summary>
		/// Sets the console control handler for the current platform.
		/// </summary>
		/// <param name="Handler"></param>
		abstract public void SetConsoleCtrlHandler(ProcessManager.CtrlHandlerDelegate Handler);

		/// <summary>
		/// Platform specific override to skip loading/compiling unsupported modules
		/// </summary>
		/// <param name="ModuleName">Module name</param>
		/// <returns>True if module should be compiled or loaded</returns>
		abstract public bool IsScriptModuleSupported(string ModuleName);

		/// <summary>
		/// Gets UBT project name for the current platform.
		/// </summary>
		abstract public string UBTProjectName { get; }

		/// <summary>
		/// Returns the type of the host editor platform.
		/// </summary>
		abstract public UnrealBuildTool.UnrealTargetPlatform HostEditorPlatform { get; }

		/// <summary>
		/// Returns the pdb file extenstion for the host platform.
		/// </summary>
		abstract public string PdbExtension { get; }

		/// <summary>
		/// List of processes that can't not be killed
		/// </summary>
		abstract public string[] DontKillProcessList { get; }
	}
}
