// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateNullRenderer : ModuleRules
{
	public SlateNullRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.Add("Runtime/SlateNullRenderer/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"SlateCore"
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"RenderCore",
				"RHI"
			}
		);
		}
	}
}
