// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MeshPaintMode : ModuleRules
{
    public MeshPaintMode(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
                "AssetTools"
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "MeshPaint",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "AppFramework",
                "Core", 
                "CoreUObject",
                "DesktopPlatform",
                "Engine", 
                "InputCore",
                "RenderCore",
                "RHI",
                "ShaderCore",
                "Slate",
				"SlateCore",
                "EditorStyle",
                "UnrealEd",
                "RawMesh",
                "SourceControl",
                "ViewportInteraction",
                "VREditor",
                "PropertyEditor",
                "MainFrame",
				"MeshPaint",
            }
        );

        CircularlyReferencedDependentModules.AddRange(
            new string[]
            {
                "ViewportInteraction",
                "VREditor"
            }
        );

        PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"AssetTools",
				"LevelEditor"
            });

		DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
                "AssetTools"
            }
        );
    }
}
