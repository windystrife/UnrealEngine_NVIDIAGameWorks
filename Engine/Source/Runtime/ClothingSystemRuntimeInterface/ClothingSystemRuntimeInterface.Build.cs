// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ClothingSystemRuntimeInterface : ModuleRules
{
	public ClothingSystemRuntimeInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/ClothingSystemRuntimeInterface/Private");

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
            }
        );
    }
}
