// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SourceControlWindows : ModuleRules
{
	public SourceControlWindows(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core", 
				"CoreUObject", 
                "InputCore",
				"Engine", 
				"Slate",
				"SlateCore",
                "EditorStyle",
				"SourceControl", 
				"AssetTools",
                "UnrealEd"			// We need this dependency here because we use PackageTools.
			}
		);
	}
}
