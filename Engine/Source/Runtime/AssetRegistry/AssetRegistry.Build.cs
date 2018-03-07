// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetRegistry : ModuleRules
{
	public AssetRegistry(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Projects",
			}
			);

		if (Target.bBuildEditor == true)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "DirectoryWatcher" });
			DynamicallyLoadedModuleNames.AddRange(new string[] { "DirectoryWatcher" });
		}
	}
}
