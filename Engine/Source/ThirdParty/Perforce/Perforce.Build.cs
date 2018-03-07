// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Perforce : ModuleRules
{
	public Perforce(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibFolder = "lib/";
		string LibPrefix = "";
		string LibPostfixAndExt = ".";
		string P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2015.2/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibFolder += "win64";
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			LibFolder += "win32";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2014.1/";
			LibFolder += "mac";
		}
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2014.1/" ;
            LibFolder += "linux/" + Target.Architecture;
        }

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
            if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
                LibPostfixAndExt = "d.";

            LibPostfixAndExt += "lib";
            PublicLibraryPaths.Add(P4APIPath + LibFolder);

            RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Perforce/p4api.dll"));
            RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Perforce/p4api.xml"));
        }
        else
        {
            LibPrefix = P4APIPath + LibFolder + "/";
            LibPostfixAndExt = ".a";
        }

        PublicSystemIncludePaths.Add(P4APIPath + "include");
        PublicAdditionalLibraries.Add(LibPrefix + "libclient" + LibPostfixAndExt);

        if (Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(LibPrefix + "libp4sslstub" + LibPostfixAndExt);
        }

        PublicAdditionalLibraries.Add(LibPrefix + "librpc" + LibPostfixAndExt);
        PublicAdditionalLibraries.Add(LibPrefix + "libsupp" + LibPostfixAndExt);
    }
}
