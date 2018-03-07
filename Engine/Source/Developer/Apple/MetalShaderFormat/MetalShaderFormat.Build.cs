// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MetalShaderFormat : ModuleRules
{
	public MetalShaderFormat(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PublicIncludePaths.Add("Runtime/Apple/MetalRHI/Public");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/DerivedDataCache/Public",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ShaderCore",
				"ShaderCompilerCommon",
				"ShaderPreprocessor"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"HLSLCC"
			);
			
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{ 
				"DerivedDataCache",
			}
			);
	}
}
