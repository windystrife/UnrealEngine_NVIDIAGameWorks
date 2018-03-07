// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ForsythTriOptimizer : ModuleRules
{
	public ForsythTriOptimizer(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string ForsythTriOptimizerPath = Target.UEThirdPartySourceDirectory + "ForsythTriOO/";
        PublicIncludePaths.Add(ForsythTriOptimizerPath + "Src");

		string ForsythTriOptimizerLibPath = ForsythTriOptimizerPath + "Lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            ForsythTriOptimizerLibPath += "Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(ForsythTriOptimizerLibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("ForsythTriOptimizerD_64.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("ForsythTriOptimizer_64.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
            ForsythTriOptimizerLibPath += "Win32/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(ForsythTriOptimizerLibPath);
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("ForsythTriOptimizerD.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("ForsythTriOptimizer.lib");
			}
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(ForsythTriOptimizerLibPath + "Mac/libForsythTriOptimizer" + Postfix + ".a");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(ForsythTriOptimizerLibPath + "Linux/" + Target.Architecture + "/libForsythTriOptimizer" + Postfix + ".a");
        }
	}
}
