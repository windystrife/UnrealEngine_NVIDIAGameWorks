// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BehaviorTreeEditor : ModuleRules
{
	public BehaviorTreeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
				"Editor/GraphEditor/Private",
				"Editor/AIGraph/Private",
				"Editor/BehaviorTreeEditor/Private",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"AssetRegistry",
				"AssetTools",
                "PropertyEditor",
				"ContentBrowser"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
				"ApplicationCore",
				"Engine", 
                "RenderCore",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd", 
                "AudioEditor",
				"MessageLog", 
				"GraphEditor",
                "Kismet",
				"KismetWidgets",
                "PropertyEditor",
				"AnimGraph",
				"BlueprintGraph",
                "AIGraph",
                "AIModule",
				"ClassViewer"
			}
		);

		PublicIncludePathModuleNames.Add("LevelEditor");

		DynamicallyLoadedModuleNames.AddRange(
            new string[] { 
                "WorkspaceMenuStructure",
				"AssetTools",
				"AssetRegistry",
				"ContentBrowser"
            }
		);
	}
}
