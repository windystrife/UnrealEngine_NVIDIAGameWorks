// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioFormatOpus : ModuleRules
{
	public AudioFormatOpus(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine"
			}
			);

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.Linux) ||
			(Target.Platform == UnrealTargetPlatform.Mac) ||
			(Target.Platform == UnrealTargetPlatform.XboxOne)
			//(Target.Platform == UnrealTargetPlatform.HTML5) // TODO test this for HTML5 !
		)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"libOpus"
				);
		}

		Definitions.Add("WITH_OGGVORBIS=1");
	}
}
