// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayAbilities : ModuleRules
	{
		public GameplayAbilities(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("GameplayAbilities/Private");
			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"GameplayTags",
					"GameplayTasks",
				}
				);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("Slate");
				PrivateDependencyModuleNames.Add("SequenceRecorder");
			}

			if (Target.bBuildDeveloperTools || (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test))
			{
				PrivateDependencyModuleNames.Add("GameplayDebugger");
				Definitions.Add("WITH_GAMEPLAY_DEBUGGER=1");
			}
			else
			{
				Definitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
			}
		}
	}
}
