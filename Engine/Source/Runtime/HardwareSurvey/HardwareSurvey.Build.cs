// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HardwareSurvey : ModuleRules
{
	public HardwareSurvey(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ApplicationCore"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Runtime/HardwareSurvey/Private",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/HardwareSurvey/Public",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
		new string[] {
				"Analytics",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics",
			}
		);

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
