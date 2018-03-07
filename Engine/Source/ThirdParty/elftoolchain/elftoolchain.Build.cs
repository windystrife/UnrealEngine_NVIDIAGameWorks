// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class elftoolchain : ModuleRules
{
	public elftoolchain(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "elftoolchain/include/" + Target.Architecture);

        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
			string LibDir = Target.UEThirdPartySourceDirectory + "elftoolchain/lib/Linux/" + Target.Architecture;
			PublicAdditionalLibraries.Add(LibDir + "/libelf.a");
			PublicAdditionalLibraries.Add(LibDir + "/libdwarf.a");
        }
	}
}
