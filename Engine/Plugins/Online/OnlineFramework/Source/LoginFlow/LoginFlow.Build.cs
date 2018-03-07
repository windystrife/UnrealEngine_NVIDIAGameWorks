// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LoginFlow : ModuleRules
{
	public LoginFlow(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Slate",
			}
		);

		PrivateDependencyModuleNames.AddRange(
		new string[]
			{
				"SlateCore",
				"WebBrowser",
				"OnlineSubsystem",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Runtime/LoginFlow/Private",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/LoginFlow/Public",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
		new string[] {
				"Analytics",
				"AnalyticsET",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics",
				"AnalyticsET",
			}
		);
	}
}
