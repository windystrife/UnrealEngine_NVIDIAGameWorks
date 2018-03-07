// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Threading;
using AutomationTool;
using UnrealBuildTool;

public class AllDesktopPlatform : Platform
{
	public AllDesktopPlatform()
		: base(UnrealTargetPlatform.AllDesktop)
	{
	}

	public override bool CanBeCompiled()
	{
		return false;
	}


	public override UnrealTargetPlatform[] GetStagePlatforms()
	{
		return new UnrealTargetPlatform[] 
		{
			UnrealTargetPlatform.Win32,
//			UnrealTargetPlatform.Win64,
			UnrealTargetPlatform.Mac,
			UnrealTargetPlatform.Linux,
		};
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		SC.bIsCombiningMultiplePlatforms = true;
		string SavedPlatformDir = SC.PlatformDir;
		foreach (UnrealTargetPlatform DesktopPlatform in GetStagePlatforms())
		{
			Platform SubPlatform = Platform.GetPlatform(DesktopPlatform);
			SC.PlatformDir = DesktopPlatform.ToString();
			SubPlatform.Package(Params, SC, WorkingCL);
		}
		SC.PlatformDir = SavedPlatformDir;
		SC.bIsCombiningMultiplePlatforms = false;
	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		SC.bIsCombiningMultiplePlatforms = true;
		string SavedPlatformDir = SC.PlatformDir;
		foreach (UnrealTargetPlatform DesktopPlatform in GetStagePlatforms())
		{
			Platform SubPlatform = Platform.GetPlatform(DesktopPlatform);
			SC.PlatformDir = DesktopPlatform.ToString();
			SubPlatform.GetFilesToArchive(Params, SC);
		}
		SC.PlatformDir = SavedPlatformDir;
		SC.bIsCombiningMultiplePlatforms = false;
	}

	public override void GetConnectedDevices(ProjectParams Params, out List<string> Devices)
	{
		Devices = new List<string>();
	}

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		return null;
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		SC.bIsCombiningMultiplePlatforms = true;
		string SavedPlatformDir = SC.PlatformDir;
		foreach (UnrealTargetPlatform DesktopPlatform in GetStagePlatforms())
		{
			Platform SubPlatform = Platform.GetPlatform(DesktopPlatform);
			SC.PlatformDir = DesktopPlatform.ToString();
			SubPlatform.GetFilesToDeployOrStage(Params, SC);
		}
		SC.PlatformDir = SavedPlatformDir;
		SC.bIsCombiningMultiplePlatforms = false;
	}

	public override void ProcessArchivedProject(ProjectParams Params, DeploymentContext SC)
	{
		Console.WriteLine("***************************** PROCESSING ARCHIVED PROJECT ****************");

		SC.bIsCombiningMultiplePlatforms = true;
		string SavedPlatformDir = SC.PlatformDir;
		foreach (UnrealTargetPlatform DesktopPlatform in GetStagePlatforms())
		{
			Platform SubPlatform = Platform.GetPlatform(DesktopPlatform);
			SC.PlatformDir = DesktopPlatform.ToString();
			SubPlatform.ProcessArchivedProject(Params, SC);
		}
		SC.PlatformDir = SavedPlatformDir;
		SC.bIsCombiningMultiplePlatforms = false;

		Console.WriteLine("***************************** DONE PROCESSING ARCHIVED PROJECT ****************");
	}

	/// <summary>
	/// Gets cook platform name for this platform.
	/// </summary>
	/// <param name="CookFlavor">Additional parameter used to indicate special sub-target platform.</param>
	/// <returns>Cook platform string.</returns>
	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return "AllDesktop";
	}

	public override bool DeployLowerCaseFilenames()
	{
		return false;
	}

	public override bool IsSupported { get { return true; } }

	public override PakType RequiresPak(ProjectParams Params)
	{
		return PakType.DontCare;
	}
    
	public override List<string> GetDebugFileExtentions()
	{
		return new List<string> { };
	}
}
