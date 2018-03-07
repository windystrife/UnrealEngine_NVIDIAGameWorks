// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoviePlayer : ModuleRules
{
	public MoviePlayer(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("Runtime/MoviePlayer/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
					"Engine",
					"ApplicationCore",
				}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[] {
                    "Core",
                    "InputCore",
                    "RenderCore",
                    "ShaderCore",
                    "CoreUObject",
                    "RHI",
                    "Slate",
					"SlateCore",
                    "HeadMountedDisplay"
				}
        );
	}
}
