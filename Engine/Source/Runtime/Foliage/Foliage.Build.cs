// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Foliage: ModuleRules
{
	public Foliage(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
				"RenderCore",
				"RHI"
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Foliage/Private"
            })
		;
	}
}
