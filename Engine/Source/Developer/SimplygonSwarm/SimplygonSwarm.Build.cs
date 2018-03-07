// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class SimplygonSwarm : ModuleRules
{
	public SimplygonSwarm(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Developer/SimplygonSwarm/Public");
        PrivateIncludePaths.Add("Developer/SimplygonSwarm/Private");

        PublicDependencyModuleNames.AddRange(
        new string[] {
                "Core",
                "CoreUObject",
                "InputCore",
                "Json",
                "RHI",
            }
        );

        PrivateDependencyModuleNames.AddRange(
        new string[] { 
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RawMesh",
                "MeshBoneReduction",
                "ImageWrapper",
                "HTTP",
                "Json",
                "UnrealEd",
                "MaterialUtilities"
			}
        );

        PrivateIncludePathModuleNames.AddRange(
        new string[] { 
				"MeshUtilities",
				"MaterialUtilities",
                "SimplygonMeshReduction",
                "MeshReductionInterface"
            }
        );

       PublicIncludePathModuleNames.AddRange(
       new string[] {
                "MeshReductionInterface"
           }
       );


        AddEngineThirdPartyPrivateStaticDependencies(Target, "Simplygon");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "SSF");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "SPL");
		AddEngineThirdPartyPrivateDynamicDependencies(Target, "PropertyEditor");

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
