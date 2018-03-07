// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PListEditor : ModuleRules
{
	public PListEditor( ReadOnlyTargetRules Target ) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"UnrealEd",
				"DesktopPlatform",
				"XmlParser",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MainFrame",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
			}
		);
	}
}
