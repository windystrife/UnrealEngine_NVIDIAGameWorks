// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameCircleRuntimeSettings : ModuleRules
{
	public GameCircleRuntimeSettings(ReadOnlyTargetRules Target) : base(Target)
    {
		BinariesSubFolder = "Android";
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

        if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
			    {
					"Settings",
					"TargetPlatform",
                    "Android_MultiTargetPlatform"
			    }
            );
        }
	}
}
