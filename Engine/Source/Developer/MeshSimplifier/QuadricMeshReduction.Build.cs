// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class QuadricMeshReduction : ModuleRules
{
	public QuadricMeshReduction(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Developer/MeshSimplifier/Public");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Engine");
		PrivateDependencyModuleNames.Add("RenderCore");
		PrivateDependencyModuleNames.Add("RawMesh");

        PrivateIncludePathModuleNames.AddRange(
        new string[] {
                "MeshReductionInterface",
             }
        );
    }
}
