// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashReportClient : ModuleRules
{
	public CrashReportClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange
		(
			new string[] 
			{ 
				"Runtime/Launch/Public",
				"Programs/CrashReportClient/Private",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"CrashDebugHelper",
				"HTTP",
				"Json",
				"Projects",
				"XmlParser",
				"Analytics",
				"AnalyticsET",
				"DesktopPlatform",
                "LauncherPlatform"
			}
			);

		if (Target.Platform != UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] 
				{
					"Slate",
					"SlateCore",
					"StandaloneRenderer",
					"MessageLog",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);
		}

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include

		WhitelistRestrictedFolders.Add("Private/NotForLicensees");
	}
}
