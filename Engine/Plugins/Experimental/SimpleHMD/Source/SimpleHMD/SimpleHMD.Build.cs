// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SimpleHMD : ModuleRules
	{
		public SimpleHMD(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"SimpleHMD/Private",
					"../../../../Source/Runtime/Renderer/Private",
					// ... add other private include paths required here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InputCore",
					"RHI",
					"RenderCore",
					"Renderer",
					"ShaderCore",
					"HeadMountedDisplay"
				}
				);
		}
	}
}
