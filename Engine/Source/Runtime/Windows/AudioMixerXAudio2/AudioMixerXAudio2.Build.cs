// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMixerXAudio2 : ModuleRules
{
	public AudioMixerXAudio2(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePaths.Add("Runtime/AudioMixer/Public");
		PrivateIncludePaths.Add("Runtime/AudioMixer/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
					"Core",
					"CoreUObject",
					"Engine",
                    "AudioMixer",
                }
		);

		PrecompileForTargets = PrecompileTargetsType.None;

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"DX11Audio",
			"UEOgg",
			"Vorbis",
			"VorbisFile"
		);

        if(Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            PrivateDependencyModuleNames.Add("XMA2");
        }
	}
}
