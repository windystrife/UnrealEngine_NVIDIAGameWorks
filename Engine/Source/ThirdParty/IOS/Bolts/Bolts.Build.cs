// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Bolts : ModuleRules
{
	public Bolts(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        Definitions.Add("WITH_BOLTS=1");

		string BoltsPath = Target.UEThirdPartySourceDirectory + "IOS/Bolts/";
        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            BoltsPath += "Bolts/";

			PublicIncludePaths.Add(BoltsPath + "Bolts/Include");

			string LibDir = BoltsPath + "Lib/Release" + Target.Architecture;

            PublicLibraryPaths.Add(LibDir);
            PublicAdditionalLibraries.Add("Bolts");

            PublicAdditionalShadowFiles.Add(LibDir + "/libBolts.a");
        }
    }
}
