// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ApexDestruction : ModuleRules
{
	public ApexDestruction(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
				"Engine",
                "PhysX",
                "APEX",
                "RHI",
                "RenderCore",
                "ApexDestructionLib"
            }
		);

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "UnrealEd"
                }
            );
        }
        
        SetupModulePhysXAPEXSupport(Target);
	}
}
