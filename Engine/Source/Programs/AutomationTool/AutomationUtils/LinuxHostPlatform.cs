// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using UnrealBuildTool;

namespace AutomationTool
{
	class LinuxHostPlatform : HostPlatform
	{
		public override string GetMsBuildExe()
		{
			return "xbuild";
		}

		public override string RelativeBinariesFolder
		{
			get { return @"Engine/Binaries/Linux/"; }
		}

		public override string GetUE4ExePath(string UE4Exe)
		{
			int CmdExeIndex = UE4Exe.IndexOf("-Cmd.exe");
			if (CmdExeIndex != -1)
			{
				UE4Exe = UE4Exe.Substring (0, CmdExeIndex);
			}
			else
			{
				CmdExeIndex = UE4Exe.IndexOf (".exe");
				if (CmdExeIndex != -1)
				{
					UE4Exe = UE4Exe.Substring (0, CmdExeIndex);
				}
			}
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, RelativeBinariesFolder, UE4Exe);
		}

		public override string LocalBuildsLogFolder
		{
			// @FIXME: should use xdg-user-dir DOCUMENTS
			get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Documents/Unreal Engine/LocalBuildLogs"); }
		}

		public override string P4Exe
		{
			get { return "/usr/bin/p4"; }
		}

		public override Process CreateProcess(string AppName)
		{
			var NewProcess = new Process();
			if (AppName == "mono")
			{
				// Enable case-insensitive mode for Mono
				NewProcess.StartInfo.EnvironmentVariables.Add("MONO_IOMAP", "case");
			}
			return NewProcess;
		}

		public override void SetupOptionsForRun(ref string AppName, ref CommandUtils.ERunOptions Options, ref string CommandLine)
		{
			if (AppName == "sh" || AppName == "xbuild" || AppName == "codesign")
			{
				Options &= ~CommandUtils.ERunOptions.AppMustExist;
			}
			if (AppName == "xbuild")
			{
				AppName = "xbuild";
				CommandLine = (String.IsNullOrEmpty(CommandLine) ? "" : CommandLine) + " /verbosity:quiet /nologo";
				// For some reason AutomationScripts.Automation.csproj has ToolsVersion set
				// to 11.0, which is no available on linux, so force ToolsVersion to 4.0
				CommandLine += " /tv:4.0";
				// Pass #define MONO to all the automation scripts (see XboxOne)
				CommandLine += " /p:DefineConstants=MONO";
				// Some projects have TargetFrameworkProfile=Client which causes warnings on Linux
				// so force it to empty.
				CommandLine += " /p:TargetFrameworkProfile=";
			}
			if (AppName.EndsWith(".exe") || ((AppName.Contains("/Binaries/Win64/") || AppName.Contains("/Binaries/Linux/")) && string.IsNullOrEmpty(Path.GetExtension(AppName))))
			{
				if (AppName.Contains("/Binaries/Win64/") || AppName.Contains("/Binaries/Linux/"))
				{
					AppName = AppName.Replace("/Binaries/Win64/", "/Binaries/Linux/");
					AppName = AppName.Replace("-cmd.exe", "");
					AppName = AppName.Replace("-Cmd.exe", "");
					AppName = AppName.Replace(".exe", "");
				}
				else
				{
					// It's a C# app, so run it with Mono
					CommandLine = "\"" + AppName + "\" " + (String.IsNullOrEmpty(CommandLine) ? "" : CommandLine);
					AppName = "mono";
					Options &= ~CommandUtils.ERunOptions.AppMustExist;
				}
			}
		}

		public override void SetConsoleCtrlHandler(ProcessManager.CtrlHandlerDelegate Handler)
		{
			// @todo: add mono support
		}

		public override bool IsScriptModuleSupported(string ModuleName)
		{
			// @todo: add more unsupported modules here
			if (ModuleName.StartsWith("Gauntlet", StringComparison.OrdinalIgnoreCase))
			{
				return false;
			}
			return true;
		}

		public override string UBTProjectName
		{
			get { return "UnrealBuildTool"; }
		}

		public override UnrealTargetPlatform HostEditorPlatform
		{
			get { return UnrealTargetPlatform.Linux; }
		}

		public override string PdbExtension
		{
			get { return ".exe.mdb"; }
		}
		static string[] SystemServices = new string[]
		{
			// TODO: Add any system process names here
		};
		public override string[] DontKillProcessList
		{
			get
			{
				return SystemServices;
			}
		}
	}
}
