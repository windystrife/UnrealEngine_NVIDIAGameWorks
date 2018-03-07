// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMixerAudioUnit : ModuleRules
{
	public AudioMixerAudioUnit(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePaths.Add("Runtime/AudioMixer/Public");
		PrivateIncludePaths.Add("Runtime/AudioMixer/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			}
			);

		PrecompileForTargets = PrecompileTargetsType.None;

		PrivateDependencyModuleNames.Add("AudioMixer");

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"UEOgg",
			"Vorbis",
			"VorbisFile"
			);

		PublicFrameworks.AddRange(new string[]
		{
			"AudioToolbox",
			"CoreAudio"
		});
		
		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicFrameworks.Add("AVFoundation");
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicFrameworks.Add("AudioUnit");
        }

		Definitions.Add("WITH_OGGVORBIS=1");
	}
}
