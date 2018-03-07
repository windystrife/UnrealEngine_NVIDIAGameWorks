// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProceduralMeshComponentEditor : ModuleRules
	{
        public ProceduralMeshComponentEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("ProceduralMeshComponentEditor/Private");
            PublicIncludePaths.Add("ProceduralMeshComponentEditor/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Slate",
                    "SlateCore",
                    "Engine",
                    "UnrealEd",
                    "PropertyEditor",
                    "RenderCore",
                    "ShaderCore",
                    "RHI",
                    "ProceduralMeshComponent",
                    "RawMesh",
                    "AssetTools",
                    "AssetRegistry"
                }
				);
		}
	}
}
