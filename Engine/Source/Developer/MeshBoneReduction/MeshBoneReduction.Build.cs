// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MeshBoneReduction : ModuleRules
{
    public MeshBoneReduction(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Developer/MeshBoneReduction/Public");
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Engine");
        PrivateDependencyModuleNames.Add("RenderCore");
		PrivateDependencyModuleNames.Add("RHI");
        PrivateDependencyModuleNames.Add("AnimationModifiers");
    }
}
