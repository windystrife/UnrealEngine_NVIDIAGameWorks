// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SequenceRecorderSections : ModuleRules
	{
		public SequenceRecorderSections(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Editor/SequenceRecorderSections/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "MovieScene",
                    "MovieSceneTracks",
					"SequenceRecorder",
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
				}
				);
		}
	}
}
