// Copyright 2015 Kite & Lightning.  All rights reserved.

namespace UnrealBuildTool.Rules
{
	public class StereoPanorama : ModuleRules
	{
		public StereoPanorama( ReadOnlyTargetRules Target ) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/StereoPanorama/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"ImageWrapper",
					"InputCore",
					"RenderCore",
					"ShaderCore",
					"RHI",
					"Slate",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
		}
	}
}
