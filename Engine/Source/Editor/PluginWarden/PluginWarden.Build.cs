// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PluginWarden : ModuleRules
{
	public PluginWarden(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"LauncherServices",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"UnrealEd",
				"PortalServices",
				"LauncherPlatform",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"LauncherServices",
			}
		);
	}
}
