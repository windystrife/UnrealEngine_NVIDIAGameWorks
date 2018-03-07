// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraVertexFactories : ModuleRules
{
    public NiagaraVertexFactories(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"Engine",		
				"RenderCore",
				"ShaderCore",
                "Niagara",
                "RHI",
			}
        );

        PublicDependencyModuleNames.AddRange(
            new string[] {
            }
        );


        PrivateIncludePaths.AddRange(
            new string[] {
            })
        ;
    }
}
