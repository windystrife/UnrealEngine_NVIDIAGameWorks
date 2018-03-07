// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class AMD_AGS : ModuleRules
{
	public AMD_AGS(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string AmdAgsPath = Target.UEThirdPartySourceDirectory + "AMD/AMD_AGS/";
		PublicSystemIncludePaths.Add(AmdAgsPath + "inc/");

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			string AmdApiLibPath = AmdAgsPath + "lib/";
			AmdApiLibPath = AmdApiLibPath + "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(AmdApiLibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add("amd_ags_x64d.lib");
				}
				else if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicAdditionalLibraries.Add("amd_ags_x86d.lib");
				}
			}
			else
			{
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					PublicAdditionalLibraries.Add("amd_ags_x64.lib");
				}
				else if (Target.Platform == UnrealTargetPlatform.Win32)
				{
					PublicAdditionalLibraries.Add("amd_ags_x86.lib");
				}
			}
		}
	}
}

