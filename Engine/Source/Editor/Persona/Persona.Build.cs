// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Persona : ModuleRules
{
    public Persona(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePaths.Add("Editor/Persona/Private");  // For PCH includes (because they don't work with relative paths, yet)

        PublicIncludePathModuleNames.AddRange(
            new string[] {
                "SkeletonEditor",
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "AdvancedPreviewScene",
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetRegistry", 
                "MainFrame",
                "DesktopPlatform",
                "ContentBrowser",
                "AssetTools",
                "AnimationEditor",
                "MeshReductionInterface"
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
                "Engine", 
                "UnrealEd", 
                "GraphEditor", 
                "InputCore",
                "Kismet", 
                "KismetWidgets",
                "AnimGraph",
                "PropertyEditor",
                "EditorWidgets",
                "BlueprintGraph",
                "RHI",
                "Json",
                "JsonUtilities",
                "ClothingSystemRuntime",
                "ClothingSystemEditorInterface",
                "ClothingSystemRuntimeInterface",
                "AnimGraphRuntime",
            }
        );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "ContentBrowser",
                "Documentation",
                "MainFrame",
                "DesktopPlatform",
                "SkeletonEditor",
                "AssetTools",
                "AnimationEditor",
                "MeshReductionInterface"
            }
        );
    }
}
