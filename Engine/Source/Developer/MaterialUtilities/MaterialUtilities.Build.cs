// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MaterialUtilities : ModuleRules
{
	public MaterialUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string [] {
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
                "Renderer",
                "RHI",
                "Landscape",
                "UnrealEd",
                "ShaderCore"
            }
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "MeshMergeUtilities",
                "MaterialBaking",
            }
        );

        PublicDependencyModuleNames.AddRange(
			new string [] {
				 "RawMesh",            
			}
		);      

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "Landscape",
                "MeshMergeUtilities",
                "MaterialBaking",
            }
        );

        CircularlyReferencedDependentModules.AddRange(
            new string[] {
                "Landscape"
            }
        );
	}
}
