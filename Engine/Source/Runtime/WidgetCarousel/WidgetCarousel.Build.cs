// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WidgetCarousel : ModuleRules
{
	public WidgetCarousel(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"InputCore",
				"CoreUObject"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/WidgetCarousel/Private",
			}
		);
	}
}
