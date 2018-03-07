// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameMenuBuilder : ModuleRules
{
    public GameMenuBuilder(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] { 
					"Engine",
					"Core",
                    "CoreUObject",
					"InputCore",
					"Slate",
                    "SlateCore",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/GameMenuBuilder/Private",
			}
		);		
	}
}
