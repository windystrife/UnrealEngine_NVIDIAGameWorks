// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BlueprintStats : ModuleRules
	{
        public BlueprintStats(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"GraphEditor",
					"BlueprintGraph",
					"MessageLog"
				}
			);
		}
	}
}
