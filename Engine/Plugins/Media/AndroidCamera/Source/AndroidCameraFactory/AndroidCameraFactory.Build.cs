// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AndroidCameraFactory : ModuleRules
	{
		public AndroidCameraFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AndroidCamera",
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AndroidCameraFactory/Private",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
				});

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				DynamicallyLoadedModuleNames.Add("AndroidCamera");
			}
		}
	}
}
