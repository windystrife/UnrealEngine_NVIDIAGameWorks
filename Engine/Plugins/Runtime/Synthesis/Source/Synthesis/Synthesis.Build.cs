// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Synthesis : ModuleRules
	{
        public Synthesis(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
					"CoreUObject",
					"Engine",
					"AudioMixer",
                    "UMG",
                    "Slate",
                    "SlateCore",
                    "InputCore",
                    "Projects"
                }
            );
		}
	}
}