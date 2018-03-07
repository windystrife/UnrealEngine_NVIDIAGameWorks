// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AndroidMedia : ModuleRules
	{
		public AndroidMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AndroidMediaFactory",
					"Core",
					"Engine",
					"MediaUtils",
					"RenderCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AndroidMedia/Private",
					"AndroidMedia/Private/Player",
					"AndroidMedia/Private/Shared",
				});
		}
	}
}
