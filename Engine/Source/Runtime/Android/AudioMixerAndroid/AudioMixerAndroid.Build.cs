// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioMixerAndroid : ModuleRules
{
	public AudioMixerAndroid(ReadOnlyTargetRules Target) : base(Target)
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

		PrivateDependencyModuleNames.Add("AudioMixer");

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"UEOgg",
			"Vorbis",
			"VorbisFile"
			);
	}
}
