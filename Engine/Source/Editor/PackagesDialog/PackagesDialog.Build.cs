// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PackagesDialog : ModuleRules
{
	public PackagesDialog(ReadOnlyTargetRules Target) : base(Target)
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
                "EditorStyle",
				"UnrealEd",
				"SourceControl"
			}
		);

		DynamicallyLoadedModuleNames.Add("AssetTools");
	}
}
