// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KismetCompiler : ModuleRules
{
	public KismetCompiler(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"UnrealEd",
				"MovieScene",
				"MovieSceneTools",
				"FunctionalTesting",
				"BlueprintGraph",
				"AnimGraph",
                "MessageLog",
				"Kismet",
			}
			);
	}
}
