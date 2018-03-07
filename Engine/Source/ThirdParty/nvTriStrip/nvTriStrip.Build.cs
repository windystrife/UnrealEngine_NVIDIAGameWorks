// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class nvTriStrip : ModuleRules
{
	public nvTriStrip(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string NvTriStripPath = Target.UEThirdPartySourceDirectory + "nvTriStrip/nvTriStrip-1.0.0/";
        PublicIncludePaths.Add(NvTriStripPath + "Inc");

		string NvTriStripLibPath = NvTriStripPath + "Lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			NvTriStripLibPath += "Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(NvTriStripLibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("nvTriStripD_64.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("nvTriStrip_64.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			NvTriStripLibPath += "Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(NvTriStripLibPath);
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("nvTriStripD.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("nvTriStrip.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
			PublicAdditionalLibraries.Add(NvTriStripLibPath + "Mac/libnvtristrip" + Postfix + ".a");
		}
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(NvTriStripLibPath + "Linux/" + Target.Architecture + "/libnvtristrip" + Postfix + ".a");
        }
	}
}
