// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClothPainter : ModuleRules
{
	public ClothPainter(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateDependencyModuleNames.AddRange(
			new string[] 
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Slate",
                "SlateCore",
                "EditorStyle",
                "UnrealEd",
                "MainFrame",
                "PropertyEditor",
                "Kismet",
                "AssetTools",
                "ClassViewer",
                "Persona",
                "SkeletalMeshEditor",                
                "AssetRegistry",
                "MeshPaint",
                "ClothingSystemRuntime",
                "ClothingSystemRuntimeInterface",
                "ClothingSystemEditorInterface",
                "SkeletalMeshEditor",
                "AdvancedPreviewScene"
            }
		);
    }
}
