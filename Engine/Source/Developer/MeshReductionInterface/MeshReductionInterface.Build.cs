// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MeshReductionInterface : ModuleRules
{
	public MeshReductionInterface(ReadOnlyTargetRules Target) : base(Target)
	{		
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Engine");
    }
}
