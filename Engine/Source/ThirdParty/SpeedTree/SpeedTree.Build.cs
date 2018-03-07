// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class SpeedTree : ModuleRules
{
	public SpeedTree(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		var bPlatformAllowed = ((Target.Platform == UnrealTargetPlatform.Win32) ||
								(Target.Platform == UnrealTargetPlatform.Win64) ||
								(Target.Platform == UnrealTargetPlatform.Mac) || (Target.Platform == UnrealTargetPlatform.Linux));

		if (bPlatformAllowed &&
			Target.bCompileSpeedTree)
		{
			Definitions.Add("WITH_SPEEDTREE=1");
			Definitions.Add("SPEEDTREE_KEY=INSERT_KEY_HERE");

			string SpeedTreePath = Target.UEThirdPartySourceDirectory + "SpeedTree/SpeedTreeSDK-v7.0/";
			PublicIncludePaths.Add(SpeedTreePath + "Include");

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				if (Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 || Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2017)
				{
					PublicLibraryPaths.Add(SpeedTreePath + "Lib/Windows/VC14.x64");

					if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
					{
						PublicAdditionalLibraries.Add("SpeedTreeCore_Windows_v7.0_VC14_MTDLL64_Static_d.lib");
					}
					else
					{
						PublicAdditionalLibraries.Add("SpeedTreeCore_Windows_v7.0_VC14_MTDLL64_Static.lib");
					}
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				if (Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 || Target.WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2017)
				{
					PublicLibraryPaths.Add(SpeedTreePath + "Lib/Windows/VC14");

					if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
					{
                        PublicAdditionalLibraries.Add("SpeedTreeCore_Windows_v7.0_VC14_MTDLL_Static_d.lib");
					}
					else
					{
                        PublicAdditionalLibraries.Add("SpeedTreeCore_Windows_v7.0_VC14_MTDLL_Static.lib");
					}
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicLibraryPaths.Add(SpeedTreePath + "Lib/MacOSX");
				if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/MacOSX/Debug/libSpeedTreeCore.a");
				}
				else
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/MacOSX/Release/libSpeedTreeCore.a");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				if (Target.LinkType == TargetLinkType.Monolithic)
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/Linux/" + Target.Architecture + "/Release/libSpeedTreeCore.a");
				}
				else
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/Linux/" + Target.Architecture + "/Release/libSpeedTreeCore_fPIC.a");
				}
			}
		}
	}
}

