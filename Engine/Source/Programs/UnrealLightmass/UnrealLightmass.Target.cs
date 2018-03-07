// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public class UnrealLightmassTarget : TargetRules
{
	public UnrealLightmassTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Modular;
		AdditionalPlugins.Add("UdpMessaging");
		LaunchModuleName = "UnrealLightmass";
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
		bBuildEditor = false;
		// Editor-only data, however, is needed
		bBuildWithEditorOnlyData = true;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;
		//bCompileAgainstCoreUObject = false;

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			// On Mac/Linux UnrealLightmass is executed locally and communicates with the editor using Messaging module instead of SwarmAgent
			// Plugins and developer tools are needed for that
			bCompileWithPluginSupport = true;
			bBuildDeveloperTools = true;
		}

		// UnrealHeaderTool is a console application, not a Windows app (sets entry point to main(), instead of WinMain())
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = true;

		// Disable logging, lightmass will create its own unique logging file
		OutCPPEnvironmentConfiguration.Definitions.Add("ALLOW_LOG_FILE=0");
	}
}
