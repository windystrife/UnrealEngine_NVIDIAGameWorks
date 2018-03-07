// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShaderCompilerCommon : ModuleRules
{
	public ShaderCompilerCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ShaderCore",
			}
			);

		// We only need a header containing definitions
		PublicSystemIncludePaths.Add("ThirdParty/hlslcc/hlslcc/src/hlslcc_lib");
    }
}
