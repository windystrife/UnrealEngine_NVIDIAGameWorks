// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BlueprintGraph : ModuleRules
{
	public BlueprintGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
            new string[] {
                "Editor/BlueprintGraph/Private",
                "Editor/KismetCompiler/Public",
            }
		);

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject", 
				"Engine",
                "InputCore",
				"Slate",
                "EditorStyle",
			}
		);

		PrivateDependencyModuleNames.AddRange( 
			new string[] {
				"EditorStyle",
                "KismetCompiler",
				"UnrealEd",
                "GraphEditor",
				"SlateCore",
                "Kismet",
                "KismetWidgets",
                "PropertyEditor",
			}
		);

		CircularlyReferencedDependentModules.AddRange(
            new string[] {
                "KismetCompiler",
                "UnrealEd",
                "GraphEditor",
                "Kismet",
            }
		); 
	}
}
