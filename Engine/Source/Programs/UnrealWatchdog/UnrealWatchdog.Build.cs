// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealWatchdog : ModuleRules
{
	public UnrealWatchdog(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange
		(
			new string[]
			{
				"Runtime/Launch/Public",
				"Programs/UnrealWatchdog/Private",
			}
		);

		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Runtime/Launch/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
				"Analytics",
				"AnalyticsET",
				"Projects",
				"ApplicationCore"
		   }
		);

		WhitelistRestrictedFolders.Add("Private/NotForLicensees");
	}
}
