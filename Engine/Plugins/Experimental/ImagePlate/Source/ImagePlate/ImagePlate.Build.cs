// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImagePlate : ModuleRules
	{
		public ImagePlate(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"MovieScene",
					"MovieSceneTracks",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"ImageWrapper",
					"RenderCore",
					"RHI",
					"ShaderCore",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/ImagePlate/Private",
				}
			);
		}
	}
}
