// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AvfMedia : ModuleRules
	{
		public AvfMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AvfMediaFactory",
					"Core",
					"ApplicationCore",
					"ShaderCore",
					"MediaUtils",
					"RenderCore",
					"RHI",
					"UtilityShaders",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"UtilityShaders",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AvfMedia/Private",
					"AvfMedia/Private/Player",
					"AvfMedia/Private/Shared",
					"AvfMedia/Private/Tracks",
				});

			PublicFrameworks.AddRange(
				new string[] {
					"CoreMedia",
					"CoreVideo",
					"AVFoundation",
					"AudioToolbox",
					"QuartzCore"
				});
		}
	}
}
