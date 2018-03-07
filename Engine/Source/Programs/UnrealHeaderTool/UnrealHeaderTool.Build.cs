// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealHeaderTool : ModuleRules
{
	public UnrealHeaderTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Json",
				"Projects",
				"ApplicationCore"
			}
		);
	
		PrivateIncludePaths.AddRange(
			new string[]
			{
				// For LaunchEngineLoop.cpp include
				"Runtime/Launch/Private",
				"Programs/UnrealHeaderTool/Private",
			});
		
		bEnableExceptions = true;
	}
}
