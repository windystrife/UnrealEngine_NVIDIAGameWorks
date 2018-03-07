// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioMixer : ModuleRules
	{
		public AudioMixer(ReadOnlyTargetRules Target) : base(Target)
		{
			OptimizeCode = CodeOptimization.Always;

			PrivateIncludePathModuleNames.Add("TargetPlatform");

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/AudioMixer/Private",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
				}
			);

			if ((Target.Platform == UnrealTargetPlatform.Win64) ||
				(Target.Platform == UnrealTargetPlatform.Win32))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile",
					"libOpus"
					);
			}

			// TODO test this for HTML5 !
			//if (Target.Platform == UnrealTargetPlatform.HTML5)
			//{
			//	AddEngineThirdPartyPrivateStaticDependencies(Target,
			//		"UEOgg",
			//		"Vorbis",
			//		"VorbisFile"
			//		);
			//}

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"libOpus"
					);
				PublicFrameworks.AddRange(new string[] { "AVFoundation", "CoreVideo", "CoreMedia" });
			}

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile"
					);
			}

			if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"UEOgg",
					"Vorbis",
					"VorbisFile",
					"libOpus"
					);
			}

			if (Target.Platform == UnrealTargetPlatform.XboxOne)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target,
					"libOpus"
					);
			}
		}
	}
}
