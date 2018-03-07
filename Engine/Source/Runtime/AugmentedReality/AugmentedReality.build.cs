// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AugmentedReality : ModuleRules
	{
		public AugmentedReality(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("Runtime/AugmentedReality/Private");
			PublicIncludePaths.Add("Runtime/AugmentedReality/Public");

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
