// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EnvironmentQueryEditor : ModuleRules
{
	public EnvironmentQueryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
				"Editor/GraphEditor/Private",
				"Editor/AIGraph/Private",
				"Editor/EnvironmentQueryEditor/Private",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"AssetTools",
                "PropertyEditor",
                "DesktopPlatform",
            }
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
                "InputCore",
				"Engine", 
                "RenderCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd", 
				"MessageLog", 
				"GraphEditor",
				"KismetWidgets",
                "PropertyEditor",
				"AnimGraph",
				"BlueprintGraph",
                "AIGraph",
                "AIModule",
			}
		);

		PublicIncludePathModuleNames.Add("LevelEditor");

        DynamicallyLoadedModuleNames.AddRange(
            new string[] { 
                "WorkspaceMenuStructure",
            }
		);
	}
}
