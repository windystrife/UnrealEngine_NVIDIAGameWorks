// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RealtimeProfiler : ModuleRules
{
	public RealtimeProfiler(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"TaskGraph",
				"Engine"
			}
		);
	}
}
