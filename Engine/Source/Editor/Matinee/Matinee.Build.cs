// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Matinee : ModuleRules
{
	public Matinee(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Editor/DistCurveEditor/Public",
				"Editor/UnrealEd/Public",
				"Developer/DesktopPlatform/Public",
			}
			);

		PrivateIncludePaths.Add("Editor/UnrealEd/Private");	//compatibility for FBX exporter

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "UnrealEd",	//compatibility for FBX exporter
                "MainFrame",
                "WorkspaceMenuStructure",
            }
            );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "InputCore",
				"LevelEditor",
				"DistCurveEditor",
				"UnrealEd",
				"RenderCore",
				"AssetRegistry",
				"ContentBrowser",
				"MovieSceneCapture",
                "MovieSceneCaptureDialog",
				"BlueprintGraph"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				}
			);
	}
}
