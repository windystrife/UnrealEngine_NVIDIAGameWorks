// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Niagara : ModuleRules
{
    public Niagara(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "NiagaraShader",
                "Core",
                "Engine",
				"RenderCore",
                "UtilityShaders",
                "ShaderCore"
            }
        );


        PublicDependencyModuleNames.AddRange(
            new string[] {
                "NiagaraShader",
                "MovieScene",
				"MovieSceneTracks",
				"CoreUObject",
                "VectorVM",
                "RHI",
                "UtilityShaders",
                "NiagaraVertexFactories",
                "ShaderCore"
            }
        );


        PrivateIncludePaths.AddRange(
            new string[] {
            })
        ;

        // If we're compiling with the engine, then add Core's engine dependencies
        if (Target.bCompileAgainstEngine == true)
        {
            if (!Target.bBuildRequiresCookedData)
            {
                DynamicallyLoadedModuleNames.AddRange(new string[] { "DerivedDataCache" });
            }
        }


        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                "TargetPlatform",
            });
        }
    }
}
