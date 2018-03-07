// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PlacementMode : ModuleRules
{
    public PlacementMode(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePathModuleNames.Add("AssetTools");

        PrivateDependencyModuleNames.AddRange( 
            new string[] { 
                "Core", 
                "CoreUObject",
                "Engine", 
                "InputCore",
                "Slate",
				"SlateCore",
                "UnrealEd",
                "ContentBrowser",
                "CollectionManager",
                "LevelEditor",
                "AssetTools",
                "EditorStyle"
            } 
        );
    }
}
