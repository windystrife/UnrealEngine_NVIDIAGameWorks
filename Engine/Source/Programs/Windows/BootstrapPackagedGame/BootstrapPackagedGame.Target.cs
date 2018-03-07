// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedConfigurations(UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.Development, UnrealTargetConfiguration.Shipping)]
public class BootstrapPackagedGameTarget : TargetRules
{
	public BootstrapPackagedGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "BootstrapPackagedGame";

		bUseStaticCRT = true;
	}

	//
	// TargetRules interface.
	//

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		bUseUnityBuild = false;
		bUseSharedPCHs = false;
		bUseMallocProfiler = false;

		// Disable all parts of the editor.
		bCompileLeanAndMeanUE = true;
		bCompileICU = false;
		bBuildEditor = false;
		bBuildWithEditorOnlyData = false;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;
	}
}
