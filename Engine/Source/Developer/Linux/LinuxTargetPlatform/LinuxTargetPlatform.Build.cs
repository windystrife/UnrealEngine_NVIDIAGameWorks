// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LinuxTargetPlatform : ModuleRules
{
    public LinuxTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
        BinariesSubFolder = "Linux";

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"Projects"
			}
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
				"Developer/LinuxTargetPlatform/Classes"
			}
        );

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}
	}
}
