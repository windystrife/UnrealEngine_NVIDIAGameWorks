// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OptionalMobileFeaturesBPLibrary : ModuleRules
	{
        public OptionalMobileFeaturesBPLibrary(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"OptionalMobileFeaturesBPLibrary",
					"ApplicationCore",
					// ... add private dependencies that you statically link with here ...
				}
				);

            PublicIncludePathModuleNames.Add("OptionalMobileFeaturesBPLibrary");
        }
	}
}
