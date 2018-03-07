// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class MeshUtilities : ModuleRules
{
	public MeshUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"MaterialUtilities",

			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"RawMesh",
				"RenderCore", // For FPackedNormal
				"SlateCore",
				"Slate",
				"MaterialUtilities",
				"MeshBoneReduction",
				"UnrealEd",
				"RHI",
				"HierarchicalLODUtilities",
				"Landscape",
				"LevelEditor",
				"AnimationBlueprintEditor",
				"AnimationEditor",
				"SkeletalMeshEditor",
				"SkeletonEditor",
				"PropertyEditor",
				"EditorStyle",
                "GraphColor",
            }
		);

        PublicIncludePathModuleNames.AddRange(
            new string[] {
                "MeshMergeUtilities"
            }
        );

        PrivateIncludePathModuleNames.AddRange(
          new string[] {
                "MeshMergeUtilities",
                "MaterialBaking",
          }
      );

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "MeshMergeUtilities",
                "MaterialBaking",
            }
        );

        AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTriStrip");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "ForsythTriOptimizer");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "QuadricMeshReduction");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "MikkTSpace");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTessLib");

		if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
		{
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX9");
		}

		if (Target.bCompileSimplygon == true)
		{
            AddEngineThirdPartyPrivateDynamicDependencies(Target, "SimplygonMeshReduction");
            
            if (Target.bCompileSimplygonSSF == true)
            {
                DynamicallyLoadedModuleNames.AddRange(
                    new string[] {
                    "SimplygonSwarm"
                }
                );
            }
		}

        // EMBREE
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string SDKDir = Target.UEThirdPartySourceDirectory + "IntelEmbree/Embree2140/Win64/";

            PublicIncludePaths.Add(SDKDir + "include");
            PublicLibraryPaths.Add(SDKDir + "lib");
            PublicAdditionalLibraries.Add("embree.2.14.0.lib");
            RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Win64/embree.2.14.0.dll"));
            RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Win64/tbb.dll"));
            RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/Win64/tbbmalloc.dll"));
            Definitions.Add("USE_EMBREE=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string SDKDir = Target.UEThirdPartySourceDirectory + "IntelEmbree/Embree2140/MacOSX/";

            PublicIncludePaths.Add(SDKDir + "include");
            PublicAdditionalLibraries.Add(SDKDir + "lib/libembree.2.14.0.dylib");
			PublicAdditionalLibraries.Add(SDKDir + "lib/libtbb.dylib");
			PublicAdditionalLibraries.Add(SDKDir + "lib/libtbbmalloc.dylib");
            Definitions.Add("USE_EMBREE=1");
        }
        else
        {
            Definitions.Add("USE_EMBREE=0");
        }
	}
}
