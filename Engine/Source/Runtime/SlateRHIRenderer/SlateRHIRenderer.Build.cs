// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateRHIRenderer : ModuleRules
{
    public SlateRHIRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("Runtime/SlateRHIRenderer/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Slate",
				"SlateCore",
                "Engine",
                "RHI",
                "RenderCore",
				"ShaderCore",
				"Renderer",
                "ImageWrapper"
			}
		);
	}
}
