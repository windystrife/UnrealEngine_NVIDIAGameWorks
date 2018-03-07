// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HLSLCC : ModuleRules
{
	public HLSLCC(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Target.UEThirdPartySourceDirectory + "hlslcc/hlslcc/src/hlslcc_lib");

		string LibPath = Target.UEThirdPartySourceDirectory + "hlslcc/hlslcc/lib/";
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			LibPath = LibPath + (Target.Platform == UnrealTargetPlatform.Win32 ? "Win32/" : "Win64/");
			LibPath = LibPath + "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			
			PublicLibraryPaths.Add(LibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add("hlslccd_64.lib");
				}
				else if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicAdditionalLibraries.Add("hlslccd.lib");
				}
			}
			else
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add("hlslcc_64.lib");
				}
				else if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicAdditionalLibraries.Add("hlslcc.lib");
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(LibPath + "Mac/libhlslccd.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(LibPath + "Mac/libhlslcc.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicAdditionalLibraries.Add(LibPath + "Linux/" + Target.Architecture + "/libhlslcc.a");
		}
		else
		{
			string Err = string.Format("Attempt to build against HLSLCC on unsupported platform {0}", Target.Platform);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}
	}
}

