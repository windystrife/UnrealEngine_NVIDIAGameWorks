// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SPL : ModuleRules
{
    public SPL(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

		bOutputPubliclyDistributable = true;

        string SPLDirectory = Target.UEThirdPartySourceDirectory + "NotForLicensees/SPL/";
        string SPLLibPath = SPLDirectory;
        PublicIncludePaths.Add(SPLDirectory + "Public/Include");
		PublicIncludePaths.Add(SPLDirectory + "Public/json");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            SPLLibPath = SPLLibPath + "lib/win64/vs" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
            PublicLibraryPaths.Add(SPLLibPath );

            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add("SPLd.lib");
            }
            else
            {
                PublicAdditionalLibraries.Add("SPL.lib");
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string LibPath = SPLDirectory + "lib/Mac/";
            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add(LibPath + "libspl_debug.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(LibPath + "libspl.a");
            }
            
        }
    }
}

