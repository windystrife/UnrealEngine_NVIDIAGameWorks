// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BuildPatchTool : ModuleRules
{
	public BuildPatchTool( ReadOnlyTargetRules Target ) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Runtime/Launch/Private");
		PrivateIncludePaths.Add("Programs/BuildPatchTool/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ApplicationCore",
				"BuildPatchServices",
				"Projects",
				// The below items are not strictly needed by BPT, but core appears to need them during initialization
				"PakFile",
				"SandboxFile",
				"NetworkFile",
				"StreamingFile"
			}
		);


		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Messaging",
					"AutomationWorker",
					"AutomationController"
				}
			);
		}
	}
}
