// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MessageLog : ModuleRules
{
	public MessageLog(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
				"Developer/MessageLog/Private",
				"Developer/MessageLog/Private/Presentation",
				"Developer/MessageLog/Private/UserInterface",
                "Developer/MessageLog/Private/Model",
			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Slate",
				"SlateCore",
                "InputCore",
                "EditorStyle",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Documentation",
					"IntroTutorials",
				}
			);		
		}
	}
}
