// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UATHelper : ModuleRules
	{
        public UATHelper(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
                new string[] {
				    "Core",
				    "CoreUObject",
				    "Engine",
                    "InputCore",
				    "Slate",
					"SlateCore",
                    "GameProjectGeneration",
                    "UnrealEd",
					"Analytics",
			    }
			);
		}
	}
}
