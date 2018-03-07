// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LocalizationCommandletExecution : ModuleRules
{
	public LocalizationCommandletExecution(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"InputCore",
                "UnrealEd",
				"EditorStyle",
				"Engine",
				"DesktopPlatform",
                "SourceControl",
				"Localization",
			}
		);
	}
}
