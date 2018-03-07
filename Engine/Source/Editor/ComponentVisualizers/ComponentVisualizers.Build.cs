// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ComponentVisualizers : ModuleRules
{
	public ComponentVisualizers(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Editor/ComponentVisualizers/Private");	// For PCH includes (because they don't work with relative paths, yet)

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
                "SlateCore",
				"UnrealEd",
                "PropertyEditor",
                "EditorStyle",
				"AIModule",
				"ViewportInteraction"
			}
		);
	}
}
