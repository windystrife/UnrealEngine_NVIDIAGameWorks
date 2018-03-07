// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RuntimePhysXCooking : ModuleRules
{
	public RuntimePhysXCooking(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
				"Engine",
                "PhysXCooking"
			}
		);

		SetupModulePhysXAPEXSupport(Target);
	}
}
