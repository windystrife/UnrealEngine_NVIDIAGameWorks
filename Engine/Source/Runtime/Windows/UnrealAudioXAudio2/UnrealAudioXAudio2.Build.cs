// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealAudioXAudio2 : ModuleRules
{
	public UnrealAudioXAudio2(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/UnrealAudio/Private");
		PublicIncludePaths.Add("Runtime/UnrealAudio/Public");

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
		new string[] {
				"Core",
				"CoreUObject",
			}
		);
		
		PublicDependencyModuleNames.Add("UnrealAudio");

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"DX11Audio"
		);

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
