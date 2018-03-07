// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GraphEditor : ModuleRules
{
	public GraphEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/GraphEditor/Private",
				"Editor/GraphEditor/Private/KismetNodes",
				"Editor/GraphEditor/Private/KismetPins",
				"Editor/GraphEditor/Private/MaterialNodes",
				"Editor/GraphEditor/Private/MaterialPins",
				"Editor/GraphEditor/Private/SoundNodes",
			}
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {                
                "IntroTutorials",
            }
        );
         
//         PublicDependencyModuleNames.AddRange(
//             new string[] {
//                 "AudioEditor"
//             }
//         );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"EditorWidgets",
				"UnrealEd",
				"AssetRegistry",
				"ClassViewer",
                "Kismet",
				"KismetWidgets",
				"BlueprintGraph",
				"Documentation",
				"RenderCore",
				"RHI",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"ContentBrowser",
			}
		);
	}
}
