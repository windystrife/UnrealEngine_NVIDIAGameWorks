// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaCompositingEditor : ModuleRules
	{
		public MediaCompositingEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"EditorStyle",
					"Engine",
					"MediaAssets",
					"MediaCompositing",
					"MovieScene",
					"MovieSceneTools",
					"MovieSceneTracks",
					"RHI",
					"Sequencer",
					"Slate",
					"SlateCore",
					"UnrealEd",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MediaCompositingEditor/Private",
					"MediaCompositingEditor/Private/Sequencer",
					"MediaCompositingEditor/Private/Shared",
				});
		}
	}
}
