// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealAudioWasapi : ModuleRules
{
	public UnrealAudioWasapi(ReadOnlyTargetRules Target) : base(Target)
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

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
