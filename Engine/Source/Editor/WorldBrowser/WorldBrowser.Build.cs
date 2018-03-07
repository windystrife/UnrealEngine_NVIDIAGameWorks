// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WorldBrowser : ModuleRules
{
    public WorldBrowser(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.Add("Editor/WorldBrowser/Public");

        PrivateIncludePaths.Add("Editor/WorldBrowser/Private");	// For PCH includes (because they don't work with relative paths, yet)

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
				"AssetTools",
                "ContentBrowser",
				"Landscape",
                "MeshUtilities",
                "MaterialUtilities",
                "MeshMergeUtilities",
            }
        );
     
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "AppFramework",
                "Core", 
                "CoreUObject",
                "RenderCore",
                "ShaderCore",
                "InputCore",
                "Engine",
				"Landscape",
                "Slate",
				"SlateCore",
                "EditorStyle",
                "UnrealEd",
                "GraphEditor",
                "LevelEditor",
                "PropertyEditor",
                "DesktopPlatform",
                "MainFrame",
                "SourceControl",
				"SourceControlWindows",
                "RawMesh",
                "LandscapeEditor",
                "FoliageEdit",
                "ImageWrapper",
                "Foliage",
                "MaterialUtilities",
                "RHI",
                "Json",
            }
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
				"AssetTools",
				"SceneOutliner",
                "MeshUtilities",
                "ContentBrowser",
                "MeshMergeUtilities",
            }
		);
    }
}
