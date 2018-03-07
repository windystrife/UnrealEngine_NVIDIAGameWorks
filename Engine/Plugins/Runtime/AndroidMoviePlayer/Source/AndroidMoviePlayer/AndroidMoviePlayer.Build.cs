// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AndroidMoviePlayer : ModuleRules
	{
        public AndroidMoviePlayer(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/AndroidMoviePlayer/Private",
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// "OpenGL"
				});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
				    "Engine",
                    "MoviePlayer",
                    "RenderCore",
					// "OpenGLDrv",
                    // "RHI",
                    // "Slate"
				}
				);
		}
	}
}
