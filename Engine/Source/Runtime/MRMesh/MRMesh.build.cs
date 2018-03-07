// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MRMesh : ModuleRules
	{
		public MRMesh(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateIncludePaths.Add("Runtime/MRMesh/Private");
            PublicIncludePaths.Add("Runtime/MRMesh/Public");

			PrivateDependencyModuleNames.AddRange(
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
