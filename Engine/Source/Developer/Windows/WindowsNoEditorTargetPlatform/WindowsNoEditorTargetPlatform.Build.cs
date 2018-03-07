// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WindowsNoEditorTargetPlatform : ModuleRules
{
	public WindowsNoEditorTargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"TargetPlatform",
				"DesktopPlatform",
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange( new string[] {
				"Engine"
				}
			);

			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/Windows/WindowsTargetPlatform/Private"
			}
		);
	}
}
