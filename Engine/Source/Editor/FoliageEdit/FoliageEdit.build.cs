// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FoliageEdit : ModuleRules
{
	public FoliageEdit(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"UnrealEd",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"RenderCore",
				"LevelEditor",
				"Landscape",
                "PropertyEditor",
                "DetailCustomizations",
                "AssetTools",
                "Foliage",
				"ViewportInteraction",
				"VREditor"
			}
		);

		CircularlyReferencedDependentModules.AddRange(
			new string[]
			{
				"ViewportInteraction",
				"VREditor"
			}
		);

	}
}
