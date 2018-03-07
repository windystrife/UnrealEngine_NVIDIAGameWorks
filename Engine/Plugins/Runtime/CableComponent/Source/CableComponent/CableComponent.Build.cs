// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CableComponent : ModuleRules
	{
        public CableComponent(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("CableComponent/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "ShaderCore",
                    "RHI"
				}
				);
		}
	}
}
