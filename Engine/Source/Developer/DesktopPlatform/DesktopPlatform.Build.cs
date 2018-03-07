// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DesktopPlatform : ModuleRules
{
	public DesktopPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Developer/DesktopPlatform/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"Json",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateFileDialogs",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateFileDialogs",
				}
			);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}
	}
}
