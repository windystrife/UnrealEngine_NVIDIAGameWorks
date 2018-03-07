// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AvfMediaFactory : ModuleRules
	{
		public AvfMediaFactory(ReadOnlyTargetRules Target) : base(Target)
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
					"AvfMedia",
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AvfMediaFactory/Private",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
				});

			if (Target.Type == TargetType.Editor)
			{
				DynamicallyLoadedModuleNames.Add("Settings");
				PrivateIncludePathModuleNames.Add("Settings");
			}

			if ((Target.Platform == UnrealTargetPlatform.IOS) ||
				(Target.Platform == UnrealTargetPlatform.Mac))
			{
				DynamicallyLoadedModuleNames.Add("AvfMedia");
			}
		}
	}
}
