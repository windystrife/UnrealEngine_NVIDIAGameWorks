// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OculusEditor : ModuleRules
{
	public OculusEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("OculusEditor/Private");

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"Core",
				"CoreUObject",
				"OculusHMD",
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
            }
            );
	}
}
