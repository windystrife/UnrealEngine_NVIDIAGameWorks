// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTTPChunkInstaller : ModuleRules
{
	public HTTPChunkInstaller(ReadOnlyTargetRules Target)
		: base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "BuildPatchServices",
                "Core",
				"ApplicationCore",
                "Engine",
                "Http",
                "Json",
                "PakFile",
            }
            );

 //       if (Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.Win32 && Target.Platform != UnrealTargetPlatform.IOS)
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }
    }
}
