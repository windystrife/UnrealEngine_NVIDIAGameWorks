// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MixedRealityFramework : ModuleRules
{
	public MixedRealityFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"MixedRealityFramework/Public"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"MixedRealityFramework/Private"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Media",
				"MediaAssets",
				"HeadMountedDisplay",
				"InputCore",
                "MediaUtils"
			}
		);
	}
}
