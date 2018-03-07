// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IOSAudio : ModuleRules
{
	public IOSAudio(ReadOnlyTargetRules Target) : base(Target)
	{
		BinariesSubFolder = "IOS";

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PublicIncludePaths.AddRange(new string[]
		{
			"Runtime/Audio/IOSAudio/Public",
		});
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PublicFrameworks.AddRange(new string[]
		{
			"AudioToolbox",
			"CoreAudio",
			"AVFoundation"
		});
	}
}
