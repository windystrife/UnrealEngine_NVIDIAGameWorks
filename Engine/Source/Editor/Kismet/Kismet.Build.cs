// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Kismet : ModuleRules
{
	public Kismet(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Editor/Kismet/Private");

		PrivateIncludePathModuleNames.AddRange(
			new string[] { 
				"AssetRegistry", 
				"AssetTools",
                "BlueprintRuntime",
                "ClassViewer",
				"Analytics",
                "DerivedDataCache",
                "LevelEditor",
				"GameProjectGeneration",
				"Sequencer",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"EditorWidgets",
				"Engine",
				"Json",
				"Merge",
				"MessageLog",
				"UnrealEd",
				"GraphEditor",
				"KismetWidgets",
				"KismetCompiler",
				"BlueprintGraph",
				"AnimGraph",
				"PropertyEditor",
				"SourceControl",
                "SharedSettingsWidgets",
                "InputCore",
				"EngineSettings",
                "Projects",
                "JsonUtilities",
                "DerivedDataCache",
				"DesktopPlatform",
				"HotReload",
                "BlueprintNativeCodeGen",
                "UMGEditor",
                "UMG", // for SBlueprintDiff
                "WorkspaceMenuStructure"
            }
			);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "BlueprintRuntime",
                "ClassViewer",
				"Documentation",
				"GameProjectGeneration",
                "BlueprintCompilerCppBackend",
			}
            );

        CircularlyReferencedDependentModules.AddRange(
            new string[] {
                "BlueprintGraph",
                "UMGEditor",
            }
        ); 
	}
}
