// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SSF : ModuleRules
{
    public SSF(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

		bOutputPubliclyDistributable = true;

        string SSFDirectory = Target.UEThirdPartySourceDirectory + "NotForLicensees/SSF/";
        string SSFLibPath = SSFDirectory;
        PublicIncludePaths.Add(SSFDirectory + "Public");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            SSFLibPath = SSFLibPath + "lib/win64/vs" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
            PublicLibraryPaths.Add(SSFLibPath);

            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add("SSFd.lib");
            }
            else
            {
                PublicAdditionalLibraries.Add("SSF.lib");
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string LibPath = SSFDirectory + "lib/Mac/";
            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add(LibPath + "libssf_debug.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(LibPath + "libssf.a");
            }
 
        }
    }
}

