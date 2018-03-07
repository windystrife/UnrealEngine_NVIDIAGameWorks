// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraShader : ModuleRules
{
    public NiagaraShader(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Engine",
				"RenderCore",
                "ShaderCore",
            }
        );


        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "VectorVM",
                "RHI",
            }
        );

        if (Target.bBuildEditor == true)
        {
            PublicDependencyModuleNames.AddRange(
                new string[] {
                "DerivedDataCache",
                "TargetPlatform",
            });

            PrivateDependencyModuleNames.AddRange(
                new string[] {
            });
        }

        PublicIncludePaths.AddRange(
            new string[] {
                "DerivedDataCache",
                "Niagara",
            });

        PrivateIncludePaths.AddRange(
            new string[] {
                "Niagara",
            });
    }
}
