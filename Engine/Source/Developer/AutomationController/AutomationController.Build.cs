// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationController : ModuleRules
	{
		public AutomationController(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			); 
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
                    "AssetRegistry",
                    "AutomationMessages",
					"UnrealEdMessages",
                    "MessageLog",
                    "Json",
                    "JsonUtilities",
					"ScreenShotComparisonTools",
					"HTTP",
                    "AssetRegistry"
                }
			);

            if (Target.bBuildEditor)
            {
                PrivateDependencyModuleNames.AddRange(
                    new string[] {
                        "UnrealEd",
                        "Engine", // Needed for UWorld/GWorld to find current level
				    }
                );
            }

            PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"MessagingCommon",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/AutomationController/Private"
				}
			);
		}
	}
}
