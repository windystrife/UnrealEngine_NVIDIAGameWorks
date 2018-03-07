// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OutputLog : ModuleRules
{
	public OutputLog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
				"Engine",
                "InputCore",
				"UnrealEd",
				"Slate",
				"SlateCore",
                "EditorStyle",
                "TargetPlatform",
                "DesktopPlatform",
			}
		);
	}
}
