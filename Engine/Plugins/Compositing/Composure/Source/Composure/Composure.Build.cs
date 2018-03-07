// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Composure : ModuleRules
	{
		public Composure(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"FX/Composure/Private",
                    "../../../../Source/Runtime/Engine/",
				}
				);
            
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "CoreUObject",
                    "Engine",
					"MovieScene",
					"MovieSceneTracks"
				}
				);
            
            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
            }
        }
    }
}
