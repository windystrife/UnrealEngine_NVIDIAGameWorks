// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CinematicCamera : ModuleRules
{
	public CinematicCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/CinematicCamera/Private"
            })
		;
	}
}
