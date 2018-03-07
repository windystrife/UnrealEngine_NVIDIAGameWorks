// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioCapture : ModuleRules
{
	public AudioCapture(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"SequenceRecorder",
                "AudioEditor"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"AudioCapture/Private",
			}
		);

        if (Target.Platform == UnrealTargetPlatform.Win32 ||
            Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Add __WINDOWS_WASAPI__ so that RtAudio compiles with WASAPI
            Definitions.Add("__WINDOWS_DS__");

            // Allow us to use direct sound
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DirectSound");
        }
	}
}
