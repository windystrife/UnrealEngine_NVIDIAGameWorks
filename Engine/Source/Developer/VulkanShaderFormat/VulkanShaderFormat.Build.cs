// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VulkanShaderFormat : ModuleRules
{
	public VulkanShaderFormat(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		// Do not link the module (as that would require the vulkan dll), only the include paths
		PublicIncludePaths.Add("Runtime/VulkanRHI/Public");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ShaderCore",
				"ShaderCompilerCommon",
				"ShaderPreprocessor",
			}
			);

		// GlsLang has a shadowed variable:
		bEnableShadowVariableWarnings = false;

		AddEngineThirdPartyPrivateStaticDependencies(Target, "HLSLCC");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "GlsLang");

		if (Target.Platform != UnrealTargetPlatform.Win64 || Target.Platform != UnrealTargetPlatform.Win32 || Target.Platform != UnrealTargetPlatform.Android || Target.Platform != UnrealTargetPlatform.Linux)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}

		AddEngineThirdPartyPrivateStaticDependencies(Target, "Vulkan");
	}
}
