// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatIntelISPCTexComp : ModuleRules
{
	public TextureFormatIntelISPCTexComp(ReadOnlyTargetRules Target) : base(Target)
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

		AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelISPCTexComp");

		if (Target.Platform != UnrealTargetPlatform.Win64 &&
            Target.Platform != UnrealTargetPlatform.Win32 &&
            Target.Platform != UnrealTargetPlatform.Mac &&
            Target.Platform != UnrealTargetPlatform.Linux)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
