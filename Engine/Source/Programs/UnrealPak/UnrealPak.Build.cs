// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealPak : ModuleRules
{
	public UnrealPak(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "PakFile", "Json", "Projects", "ApplicationCore" });

		PrivateIncludePaths.Add("Runtime/Launch/Private");      // For LaunchEngineLoop.cpp include

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
                "Json"
        });

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry"
        });
    }
}
