// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KismetWidgets : ModuleRules
{
	public KismetWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Editor/KismetWidgets/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"BlueprintGraph",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"ContentBrowser",
                "AssetTools",
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
			    "ContentBrowser",
                "AssetTools",
			}
		);
	}
}
