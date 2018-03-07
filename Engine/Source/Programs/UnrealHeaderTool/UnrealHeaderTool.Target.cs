// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealHeaderToolTarget : TargetRules
{
	public UnrealHeaderToolTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Modular;
		LaunchModuleName = "UnrealHeaderTool";
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
		// Lean and mean
		bCompileLeanAndMeanUE = true;

		// Never use malloc profiling in Unreal Header Tool.  We set this because often UHT is compiled right before the engine
		// automatically by Unreal Build Tool, but if bUseMallocProfiler is defined, UHT can operate incorrectly.
		bUseMallocProfiler = false;

        // No editor needed
        bCompileICU = false;
        bBuildEditor = false;
		// Editor-only data, however, is needed
		bBuildWithEditorOnlyData = true;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;

		// Force execption handling across all modules.
		bForceEnableExceptions = true;

		// Plugin support
		bCompileWithPluginSupport = true;
		bBuildDeveloperTools = true;

		// UnrealHeaderTool is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = true;

		OutCPPEnvironmentConfiguration.Definitions.Add("HACK_HEADER_GENERATOR=1");

		OutCPPEnvironmentConfiguration.Definitions.Add("USE_LOCALIZED_PACKAGE_CACHE=0");
	}
}
