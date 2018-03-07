// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SynthesisEditor : ModuleRules
	{
        public SynthesisEditor(ReadOnlyTargetRules Target) : base(Target)
		{
            OptimizeCode = CodeOptimization.Never;

            PublicDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
                    "UnrealEd",
					"AudioEditor",
                    "Synthesis",
					"AudioMixer",
				}
			);

            PrivateIncludePathModuleNames.AddRange(
            new string[] {
                    "AssetTools"
            });
        }
	}
}