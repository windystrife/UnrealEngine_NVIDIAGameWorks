// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ApplicationCore : ModuleRules
{
	public ApplicationCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
                "RHI"
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "InputDevice",
                "Analytics",
				"SynthBenchmark"
			}
		);

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, 
				"XInput"
				);
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, 
				"OpenGL"
				);
			if (Target.bBuildEditor == true)
			{
				PublicAdditionalLibraries.Add("/System/Library/PrivateFrameworks/MultitouchSupport.framework/Versions/Current/MultitouchSupport");
			}
		}
		else if ((Target.Platform == UnrealTargetPlatform.Linux))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, 
				"SDL2"
				);

			// We need FreeType2 and GL for the Splash, but only in the Editor
			if (Target.Type == TargetType.Editor)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "FreeType2");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
				PrivateIncludePathModuleNames.Add("ImageWrapper");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenAL");
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture != "-win32")
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
			PrivateDependencyModuleNames.Add("HTML5JS");
			PrivateDependencyModuleNames.Add("MapPakDownloader");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PublicIncludePaths.AddRange(new string[] {"Runtime/ApplicationCore/Public/Apple", "Runtime/ApplicationCore/Public/IOS"});
		}

	}
}
