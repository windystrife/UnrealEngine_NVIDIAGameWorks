// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class SimplygonMeshReduction : ModuleRules
{
	public SimplygonMeshReduction(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicIncludePaths.Add("Developer/SimplygonMeshReduction/Public");

        PrivateDependencyModuleNames.AddRange(
            new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
                "RawMesh",
                "MaterialUtilities",
                "MeshBoneReduction",
                "RHI",
                "AnimationModifiers",
            }
        );

        PrivateIncludePathModuleNames.AddRange(
            new string[] { 
                "MeshUtilities",
                "MeshReductionInterface",
            }
        );

        AddEngineThirdPartyPrivateStaticDependencies(Target, "Simplygon");

		string SimplygonPath = Target.UEThirdPartySourceDirectory + "NotForLicensees/Simplygon/Simplygon-latest/Inc/SimplygonSDK.h";
		if (Target.Platform == UnrealTargetPlatform.Win64 && File.Exists(SimplygonPath))
		{
			PrecompileForTargets = PrecompileTargetsType.Editor;
		}
		else
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}
	}
}
