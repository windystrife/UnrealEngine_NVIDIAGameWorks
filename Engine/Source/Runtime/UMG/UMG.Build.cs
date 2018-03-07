// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UMG : ModuleRules
{
	public UMG(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/UMG/Private" // For PCH includes (because they don't work with relative paths, yet)
            })
		;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "ShaderCore",
				"RenderCore",
				"RHI",
			}
		);

        PublicDependencyModuleNames.AddRange(
            new string[] {
				"HTTP",
				"MovieScene",
                "MovieSceneTracks",
			}
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
				"ImageWrapper",
                 "TargetPlatform",
            }
        );

		if (Target.Type != TargetType.Server)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateRHIRenderer",
				}
			);

            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
				    "ImageWrapper",
				    "SlateRHIRenderer",
			    }
            );
		};
	}
}
