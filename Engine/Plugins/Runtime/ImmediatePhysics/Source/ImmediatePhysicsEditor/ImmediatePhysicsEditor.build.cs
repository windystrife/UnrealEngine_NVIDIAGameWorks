// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImmediatePhysicsEditor: ModuleRules
{
	public ImmediatePhysicsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
                "AnimGraph",
                "BlueprintGraph",
                "ImmediatePhysics",
                "UnrealEd"
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/ImmediatePhysicsEditor/Private"
            });
    }
}
