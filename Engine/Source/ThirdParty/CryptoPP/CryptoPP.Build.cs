// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CryptoPP : ModuleRules
{
    public CryptoPP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibFolder = "lib/";
		string LibPrefix = "";
        string LibPostfixAndExt = ".";//(Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d." : ".";
        string CryptoPPPath = Target.UEThirdPartySourceDirectory + "CryptoPP/5.6.5/";

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicIncludePaths.Add(CryptoPPPath + "include");
            PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory);
            LibFolder += "Win64/VS2015/";
            LibPostfixAndExt += "lib";
            PublicLibraryPaths.Add(CryptoPPPath + LibFolder);
        }

        PublicAdditionalLibraries.Add(LibPrefix + "cryptlib" + LibPostfixAndExt);
	}

}
