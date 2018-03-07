// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImgMedia : ModuleRules
	{
		public ImgMedia(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"ImageWrapper",
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"ImgMediaFactory",
					"RenderCore",
					"RHI",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"ImageWrapper",
					"Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"ImgMedia/Private",
					"ImgMedia/Private/Assets",
					"ImgMedia/Private/Loader",
					"ImgMedia/Private/Player",
					"ImgMedia/Private/Readers",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
				});

			if ((Target.Platform == UnrealTargetPlatform.Mac) ||
				(Target.Platform == UnrealTargetPlatform.Win32) ||
				(Target.Platform == UnrealTargetPlatform.Win64))
			{
				PrivateDependencyModuleNames.Add("OpenExrWrapper");
			}
		}
	}
}
