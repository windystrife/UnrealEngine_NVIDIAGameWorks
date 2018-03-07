// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Settings : ModuleRules
{
	public Settings(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
			});

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Developer/Settings/Private",
			});

		PrecompileForTargets = PrecompileTargetsType.Any;
	}
}
