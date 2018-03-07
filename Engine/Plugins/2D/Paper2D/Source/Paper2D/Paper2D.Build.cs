// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Paper2D : ModuleRules
{
	public Paper2D(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Paper2D/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"ShaderCore",
				"RenderCore",
				"RHI",
                "SlateCore",
                "Slate"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Renderer",
			}
		);

		if (Target.bBuildEditor == true)
		{
			//@TODO: Needed for the triangulation code used for sprites (but only in editor mode)
			//@TOOD: Try to move the code dependent on the triangulation code to the editor-only module
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
