// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PhysXVehicles : ModuleRules
	{
        public PhysXVehicles(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePaths.Add("PhysXVehicles/Public");
            PrivateIncludePaths.Add("PhysXVehicles/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "ShaderCore",
                    "AnimGraphRuntime",
                    "RHI",
                    "PhysXVehicleLib"
				}
				);

            SetupModulePhysXAPEXSupport(Target);
        }
    }
}
