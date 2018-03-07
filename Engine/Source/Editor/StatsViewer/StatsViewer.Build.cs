// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StatsViewer : ModuleRules
{
	public StatsViewer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"RHI",
				"UnrealEd",
				"Landscape"
			}
		);

        PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "PropertyEditor",
				"Landscape"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor"
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/StatsViewer/Private",
				"Editor/StatsViewer/Private/StatsPages",
                "Editor/StatsViewer/Private/StatsEntries"
			}
		);
	}
}
