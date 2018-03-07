// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImagePlateEditor : ModuleRules
	{
		public ImagePlateEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"Core",
					"CoreUObject",
					"EditorStyle",
					"Engine",
					"ImagePlate",
					"MovieScene",
					"MovieSceneTools",
					"MovieSceneTracks",
					"RHI",
					"Sequencer",
					"Slate",
					"SlateCore",
					"UnrealEd",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/ImagePlateEditor/Private",
				}
			);
		}
	}
}
