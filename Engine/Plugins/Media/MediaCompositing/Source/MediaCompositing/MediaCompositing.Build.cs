// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaCompositing : ModuleRules
	{
		public MediaCompositing(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"MovieScene",
					"MovieSceneTracks",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Media",
					"MediaAssets",
					"RenderCore",
					"RHI",
					"ShaderCore",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MediaCompositing/Private",
					"MediaCompositing/Private/Actors",
					"MediaCompositing/Private/Components",
					"MediaCompositing/Private/MovieScene",
				});
		}
	}
}
