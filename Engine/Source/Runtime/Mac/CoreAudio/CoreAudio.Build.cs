// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CoreAudio : ModuleRules
{
	public CoreAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"UEOgg",
			"Vorbis",
			"VorbisFile"
			);

		PublicFrameworks.AddRange(new string[] { "CoreAudio", "AudioUnit", "AudioToolbox" });

		AdditionalBundleResources.Add(new UEBuildBundleResource("../Build/Mac/RadioEffectUnit/RadioEffectUnit.component"));

		// Add contents of component directory as runtime dependencies
		foreach (string FilePath in Directory.EnumerateFiles("../Build/Mac/RadioEffectUnit/RadioEffectUnit.component", "*", SearchOption.AllDirectories))
		{
			RuntimeDependencies.Add(new RuntimeDependency(FilePath));
		}
	}
}
