// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatAndroid : ModuleRules
{
	public TextureFormatAndroid(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"TextureCompressor",
				"Engine"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ImageCore",
				"ImageWrapper"
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "QualcommTextureConverter");
		}
		else
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "QualcommTextureConverter");
		}

                // opt-out from precompile for Linux (this module cannot be built for Linux atm)
                if (Target.Platform == UnrealTargetPlatform.Linux)
                {
                    PrecompileForTargets = PrecompileTargetsType.None;
                }
	}
}
