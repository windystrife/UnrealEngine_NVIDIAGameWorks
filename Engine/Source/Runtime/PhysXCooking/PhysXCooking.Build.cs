// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PhysXCooking : ModuleRules
{
    public PhysXCooking(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateIncludePathModuleNames.Add("TargetPlatform");

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
				"Engine",
                "PhysXCookingLib"
            }
            );

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Engine/Private",   //A bit hacky, but we don't want to expose the low level physx structs to other modules
			}
        );


        SetupModulePhysXAPEXSupport(Target);
    }
}
