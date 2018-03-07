// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CookedIterativeFile : ModuleRules
{
	public CookedIterativeFile(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "NetworkFile",
                "Sockets",
            }
        );
    }
}
