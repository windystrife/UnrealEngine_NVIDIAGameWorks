// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SourceCodeAccess : ModuleRules
{
	public SourceCodeAccess(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Settings",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Slate",
			}
		);

		PrivateIncludePaths.Add("Developer/SourceCodeAccess/Private");
	}
}
