// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MacServerTargetPlatform : ModuleRules
{
	public MacServerTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"TargetPlatform",
				"DesktopPlatform",
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("CoreUObject"); // @todo Mac: for some reason it's needed to link in debug on Mac
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/Mac/MacTargetPlatform/Private"
			}
		);
	}
}
