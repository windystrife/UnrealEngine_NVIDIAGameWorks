// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XAudio2 : ModuleRules
{
	public XAudio2(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			// VS2015 updated some of the CRT definitions but not all of the Windows SDK has been updated to match.
			// Microsoft provides this shim library to enable building with VS2015 until they fix everything up.
			//@todo: remove when no longer neeeded (no other code changes should be necessary).
			if (Target.WindowsPlatform.bNeedsLegacyStdioDefinitionsLib)
			{
				PublicAdditionalLibraries.Add("legacy_stdio_definitions.lib");
			}

		}
        else if (Target.Platform == UnrealTargetPlatform.XboxOne)
        {
            PrivateDependencyModuleNames.Add("XMA2");
        }

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"DX11Audio",
			"UEOgg",
			"Vorbis",
			"VorbisFile"
			);

		Definitions.Add("WITH_OGGVORBIS=1");
	}
}
