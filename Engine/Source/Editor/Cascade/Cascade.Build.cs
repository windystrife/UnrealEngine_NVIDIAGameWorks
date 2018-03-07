// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Cascade : ModuleRules
{
	public Cascade(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/Cascade/Private"
			}
		);

		PublicIncludePaths.AddRange(
			new string[] {
				"Editor/DistCurveEditor/Public",
				"Editor/UnrealEd/Public",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
                "InputCore",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"DistCurveEditor",
				"UnrealEd",
				"RHI"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"PropertyEditor"
			}
		);
	}
}
