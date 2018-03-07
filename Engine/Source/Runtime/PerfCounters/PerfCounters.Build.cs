// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PerfCounters : ModuleRules
{
    public PerfCounters(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"Json",
				"Sockets",
				"HTTP"
			}
        );
	}
}
